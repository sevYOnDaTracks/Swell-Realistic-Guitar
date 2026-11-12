#include "Bank.h"
#include <sstream>
#include <chrono>
namespace guitar {
static const char catalogue[] =
#include "Catalog.inc"
;
static std::unique_ptr<juce::AudioFormatReader> openWave(const juce::File& file) {
    juce::WavAudioFormat wav;
    if (auto input = file.createInputStream())
        return std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(input.release(),true));
    return {};
}
Bank::~Bank() {
    stopping.store(true);
    if (disk.joinable()) disk.join();
}
bool Bank::load(const juce::File& root,const std::function<bool()>& cancel,std::atomic<int>& progress) {
    auto model = models[selection/2];
    auto folder = root.getChildFile(model).getChildFile(model + ((selection%2)?" Strum Samples":" Samples"));
    if (!folder.isDirectory()) return false;
    std::istringstream lines(catalogue);
    std::string line;
    int count = 0;
    auto prefix = std::to_string(selection)+"\t";
    std::vector<std::string> rows;
    while (std::getline(lines,line))
        if (line.compare(0,prefix.size(),prefix)==0 ||
            (selection%2 && line.size()>3 && line[0]==char('0'+selection-1) && line[2]>='6' && line[2]<='9'))
            rows.push_back(std::move(line));
    takes.reserve(rows.size());
    for (auto& row : rows) {
        if ((count++ & 15)==0) {
            if (cancel()) return false;
            progress.store((int)(100.0*count/rows.size()));
        }
        std::istringstream record(row);
        int id,art,note,string,layer;
        record>>id>>art>>note>>string>>layer;
        record.get();
        std::string name;
        std::getline(record,name);
        auto source = (id==selection ? folder : root.getChildFile(model).getChildFile(model+" Samples"));
        auto file = source.getChildFile(juce::String::fromUTF8(name.c_str()));
        auto r = openWave(file);
        if (!r || r->lengthInSamples < 2 || r->lengthInSamples > INT_MAX || r->sampleRate < 1) {
            ++missing; continue;
        }
        Take t;
        t.file=file; t.note=note; t.string=string; t.layer=layer;
        t.frames=(int)r->lengthInSamples; t.rate=r->sampleRate;
        int frames=std::min(pageFrames+1,t.frames);
        t.attack.setSize(2,frames);
        if (!r->read(&t.attack,0,frames,0,true,true)) { ++missing; continue; }
        groups[art][note][string].push_back((int)takes.size());
        available[art]=true;
        takes.push_back(std::move(t));
    }
    {   // Fret noises: prefer this guitar's own scr_<model>_* files, else any scr_* in the folder.
        auto own = folder.findChildFiles(juce::File::findFiles,false,"scr_"+model.toLowerCase()+"_*.wav");
        if (own.isEmpty()) own = folder.findChildFiles(juce::File::findFiles,false,"scr_*.wav");
        own.sort();
        for (auto& file : own) {
            if (file.getFileName().startsWith("._")) continue;
            auto r = openWave(file);
            if (!r || r->lengthInSamples < 2 || r->lengthInSamples > INT_MAX || r->sampleRate < 1) continue;
            Take t; t.file=file; t.note=scratchNote; t.string=0; t.layer=(int)groups[scratchArt][scratchNote][0].size();
            t.frames=(int)r->lengthInSamples; t.rate=r->sampleRate;
            t.attack.setSize(2,std::min(pageFrames+1,t.frames));
            if (!r->read(&t.attack,0,t.attack.getNumSamples(),0,true,true)) continue;
            groups[scratchArt][scratchNote][0].push_back((int)takes.size());
            available[scratchArt]=true;
            takes.push_back(std::move(t));
        }
    }
    for (int a=0;a<artCount;++a) {
        std::array<bool,128> present {};
        for (int n=0;n<128;++n) for (auto& group : groups[a][n]) {
            std::sort(group.begin(),group.end(),[&](int x,int y){ return takes[x].layer<takes[y].layer; });
            present[n] |= !group.empty();
        }
        for (int n=0;n<128;++n) {
            int best=-1;
            for (int j=0;j<128;++j)
                if (present[j] && (best<0 || std::abs(n-j)<std::abs(n-best))) best=j;
            nearestNote[a][n]=best;
        }
    }
    return available[0] && !cancel();
}
bool Bank::onString(int art,int note,int string) const {return !groups[art][note][string].empty();}
int Bank::choose(int art,int note,int string,float velocity,unsigned repeat) const {
    const auto& g=groups[art][note][string];
    if (g.empty()) return -1;
    if (art==9 || art==5 || art==scratchArt) return g[repeat%g.size()];
    float norm=juce::jlimit(0.f,1.f,(velocity-.12f)/.88f);
    int i=juce::roundToInt(norm*((int)g.size()-1));
    if ((repeat&1u) && g.size()>3) i += (i+1<(int)g.size()?1:-1);
    return g[(size_t)i];
}
void Bank::start(){disk=std::thread([this]{serve();});}
void Bank::request(int slot,uint64_t key) {
    auto& s=streams[(size_t)slot];
    s.read.store(s.write.load(std::memory_order_acquire),std::memory_order_release);
    s.wanted.store(1);
    s.request.store(key,std::memory_order_release);
}
const Page* Bank::page(int slot,uint64_t key,int number,bool offline) {
    auto& s=streams[(size_t)slot]; s.wanted.store(number,std::memory_order_relaxed);
    const auto deadline = offline ? std::chrono::steady_clock::now()+std::chrono::seconds(2)
                                  : std::chrono::steady_clock::time_point{};
    for (;;) {
        auto r=s.read.load(std::memory_order_relaxed);
        auto w=s.write.load(std::memory_order_acquire);
        while (r!=w) {
            auto& block=s.pages[r%Stream::capacity];
            if (block.key==key && block.number>=number) return block.number==number ? &block : nullptr;
            s.read.store(++r,std::memory_order_release);
        }
        if (!offline || std::chrono::steady_clock::now()>=deadline) return nullptr;
        std::this_thread::sleep_for(std::chrono::microseconds(150));
    }
}
void Bank::serve() {
    while (!stopping.load(std::memory_order_relaxed)) {
        bool didWork=false;
        for (auto& s:streams) {
            uint64_t key=s.request.load(std::memory_order_acquire);
            if (s.workerKey!=key) {
                s.reader.reset(); s.next=1; s.workerKey=key;
                if (key) {
                    int id=(int)(uint32_t(key)-1);
                    if (takes[(size_t)id].frames>pageFrames) s.reader=openWave(takes[(size_t)id].file);
                }
            }
            if (!key || !s.reader) continue;
            unsigned w=s.write.load(std::memory_order_relaxed);
            if (w-s.read.load(std::memory_order_acquire)>=Stream::capacity) continue;
            s.next=std::max(s.next,s.wanted.load(std::memory_order_relaxed));
            juce::int64 offset=juce::int64(s.next)*pageFrames;
            if (offset>=s.reader->lengthInSamples) continue;
            auto& dest=s.pages[w%Stream::capacity];
            int count=(int)std::min<juce::int64>(pageFrames+1,s.reader->lengthInSamples-offset);
            if (!s.reader->read(&dest.audio,0,count,offset,true,true)) dest.audio.clear();
            dest.key=key; dest.number=s.next++;
            s.write.store(w+1,std::memory_order_release); didWork=true;
        }
        if (!didWork) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
}
