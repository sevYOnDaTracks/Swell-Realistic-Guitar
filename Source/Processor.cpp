#include "Processor.h"
#include <chrono>
#include <cmath>
namespace guitar {
using namespace juce;
using ID=ParameterID;
AudioProcessorValueTreeState::ParameterLayout Processor::layout() {
    AudioProcessorValueTreeState::ParameterLayout p;
    auto choice=[&](const char* id,const char* label,const StringArray& items){p.add(std::make_unique<AudioParameterChoice>(ID{id,1},label,items,0));};
    auto control=[&](const char* id,const char* label,float lo,float hi,float step,float skew,float def){
        p.add(std::make_unique<AudioParameterFloat>(ID{id,1},label,NormalisableRange<float>(lo,hi,step,skew),def));};
    p.add(std::make_unique<AudioParameterChoice>(ID{"model",1},"Guitar",models,2)); choice("mode","Play mode",{"Pick","Strum"});
    choice("art","Articulation",arts); choice("stroke","Stroke",{"Down","Up","Alternate"});
    control("spread","Strum spacing (ms)",1,45,.1f,1,12);
    control("tone","Tone (%)",0,100,.1f,1,84);
    control("release","Release (ms)",15,900,1,.55f,120);
    control("output","Output (dB)",-36,6,.1f,1,-7);
    control("attack","Attack (ms)",1,180,.1f,.45f,2);
    control("width","Stereo width (%)",0,160,1,1,100);
    control("room","Reverb mix (%)",0,75,.1f,1,16);
    control("echo","Delay mix (%)",0,65,.1f,1,16);
    control("wheel","Pitch wheel",-2,2,.01f,1,0);
    control("mod","Modulation (%)",0,100,1,1,0);
    const char* fxIds[]={"ampOn","eqOn","compOn","chorusOn","echoOn","roomOn"};
    const char* fxNames[]={"Amp","EQ","Compressor","Chorus","Delay","Reverb"};
    for(int i=0;i<6;++i)p.add(std::make_unique<AudioParameterBool>(ID{fxIds[i],1},fxNames[i],i==1));
    p.add(std::make_unique<AudioParameterBool>(ID{"handNoise",1},"Hand noises",true));
    return p;
}
Processor::Processor():AudioProcessor(BusesProperties().withOutput("Output",AudioChannelSet::stereo(),true)),
    Thread("Swell guitar library"),params(*this,nullptr,"SwellRealisticGuitar",layout()) {
    bends.fill(1); for(auto& f:frets) f.store(-1); startThread();
}
Processor::~Processor(){signalThreadShouldExit();notify();stopThread(-1);bank.reset();}
int Processor::selection() const noexcept {return (int)param("model")*2+(int)param("mode");}
String Processor::getLibrary() const {const ScopedLock l(pathLock);return root;}
void Processor::setLibrary(const File& f) {
    {const ScopedLock l(pathLock);root=f.getFullPathName();}
    pathRevision.fetch_add(1); notify();
}
void Processor::run() {
    int previous=-1;unsigned revision=~0u;
    while(!threadShouldExit()) {
        int which=selection();auto rev=pathRevision.load();
        if(previous==which && revision==rev){wait(15);continue;}
        previous=which;revision=rev; progress.store(0);failed.store(false);failedSelection.store(-1);
        auto stale=[&]{return threadShouldExit() || which!=selection() || rev!=pathRevision.load();};
        auto next=std::make_unique<Bank>(which);
        bool ok=next->load(File(getLibrary()),stale,progress);
        if(stale()){previous=-1;continue;}
        if(ok) next->start(); else next.reset();
        {const SpinLock::ScopedLockType l(bankLock);
            resetVoices();bank.swap(next);
            count.store(bank?(int)bank->takes.size():0);missing.store(bank?bank->missing:0);
            readyRevision.store(rev); loaded.store(bank?which:-1);
            failed.store(!bank);failedSelection.store(bank?-1:which);progress.store(bank?100:0);
        }
    }
}
void Processor::silence(int i){if(bank && voices[i].active)bank->request(i,0);voices[i].active=false;}
void Processor::resetVoices(){
    for(int i=0;i<slots;++i)silence(i);
    for(auto& p:pending)p.phase=0;
    bends.fill(1);modWheel.fill(0);pedals.fill(false);upNext.fill(false);repeats={};
    for(auto& f:frets)f.store(-1);damping[0]=damping[1]=0;
    pickedString.store(-1);heldArt=-1;legatoHold=false;openHeld.fill(false);
}
void Processor::prepareToPlay(double sr,int){
    const SpinLock::ScopedLockType l(bankLock);rate=sr;timeline=0;resetVoices();keyboard.reset();
    echoLine.setSize(2,(int)std::ceil(rate*.375)+2);echoLine.clear();
    chorusLine.setSize(2,(int)std::ceil(rate*.04)+2);chorusLine.clear();
    echoPos=chorusPos=0;chorusPhase=0;lastReverb=-1;
    reverb.setSampleRate(rate);reverb.reset();
    compressor.prepare(rate);
    gain.reset(rate,.015);gain.setCurrentAndTargetValue(Decibels::decibelsToGain(param("output")));
    cutoff.reset(rate,.015);cutoff.setCurrentAndTargetValue(1);
}
void Processor::releaseResources(){const SpinLock::ScopedLockType l(bankLock);resetVoices();}
bool Processor::isBusesLayoutSupported(const BusesLayout& l) const{return l.getMainOutputChannelSet()==AudioChannelSet::stereo();}
int Processor::effectiveArt() const{int a=heldArt>=0?heldArt:(int)param("art");return bank->hasArt(a)?a:0;}
// Cost of fretting `fret` with the hand where it is now: free inside the window,
// expensive outside, open strings discouraged once the hand has left first position.
int Processor::fretCost(int f) const{
    if(f<0)return 100000;
    const int h=hand.load(std::memory_order_relaxed),lo=handLow(h),hi=lo+handSpan-1+handReach;
    if(f==0)return lo<=2?0:60+h;
    int outside=f<lo?lo-f:f>hi?f-hi:0;
    return outside*10+f;   // inside the position, the lowest fret (highest string) wins
}
// A string still held by a pressed key is avoided when another one can take the note.
int Processor::stringCost(int ch,int s,int f) const{
    int cost=fretCost(f);
    for(const auto& v:voices)if(v.active && !v.noise && !v.released && !v.pedal && v.chan==ch && v.string==s){cost+=25;break;}
    return cost;
}
int Processor::lastPosition() const{return bank?std::max(1,bank->topFret()-handSpan+1):19-handSpan+1;}
bool Processor::moveHand(int lowest,int highest){
    if(highest<=0 || handLocked.load(std::memory_order_relaxed))return false;
    const int old=hand.load(std::memory_order_relaxed),lo=handLow(old),hi=lo+handSpan-1+handReach;
    int h=old;
    if(lowest>0 && lowest<lo)h=lowest;
    else if(highest>hi)h=std::max(1,highest-handSpan+1);
    hand.store(h,std::memory_order_relaxed);
    return handLow(h)!=lo;
}
int Processor::stringFor(int a,int n,int ch) const{
    int best=-1,score=100000;
    for(int s=0;s<6;++s)if(bank->onString(a,n,s)){
        int cost=stringCost(ch,s,n-tuning[s]);
        if(cost<score){score=cost;best=s;}
    }
    return best;
}
int Processor::freeSlot() const{
    int slot=0;
    for(int i=0;i<slots;++i){
        if(!voices[i].active)return i;
        if((voices[i].released && !voices[slot].released) ||
           (voices[i].released==voices[slot].released && voices[i].started<voices[slot].started))slot=i;
    }
    return slot;
}
// Fingers scraping the strings when the playing position shifts (recorded scr_* samples).
void Processor::fretNoise(int ch){
    if(param("handNoise")<.5f || !bank->hasArt(scratchArt))return;
    int id=bank->choose(scratchArt,scratchNote,0,.5f,++scratchCount*7u+(unsigned)(timeline&15));
    if(id<0)return;
    fretNoises.fetch_add(1,std::memory_order_relaxed);
    int slot=freeSlot();const auto& take=bank->takes[id];
    Voice voice;voice.active=true;voice.noise=true;voice.key=-1;voice.chan=ch;voice.id=id;voice.started=timeline;
    // As loud as what is being played: soft or muted notes get a soft scrape.
    float playing=0;for(const auto& o:voices)if(o.active && !o.noise && o.chan==ch)playing=std::max(playing,o.level);
    voice.step=take.rate/rate;voice.amp=.2f*juce::jlimit(.2f,1.f,playing/.45f);voice.ticket=(uint64_t(++serial)<<32)|uint32_t(id+1);
    voices[slot]=voice;bank->request(slot,voice.ticket);
}
// Legato (Efimov D#1 / A0): a note overlapping a held one, close enough, on the same
// string becomes a hammer-on (up) or pull-off (down). Returns that string, or -1.
int Processor::legatoString(int ch,int n,int& art) const{
    if(!(legatoOn.load(std::memory_order_relaxed) || legatoHold))return -1;
    const Voice* prev=nullptr;
    for(const auto& o:voices)if(o.active && !o.noise && !o.released && !o.pedal && o.chan==ch && o.key!=n && (!prev || o.started>prev->started))prev=&o;
    if(!prev || std::abs(n-prev->key)>legatoRange)return -1;
    int la=n>prev->key?3:4,f=n-tuning[prev->string];
    if(f<1 || !bank->hasArt(la) || !bank->onString(la,n,prev->string))return -1;
    art=la;return prev->string;
}
void Processor::launch(int ch,int n,float v,int s,int64_t duration,bool noise){
    int a=noise?9:effectiveArt();
    bool legato=false;
    if(!noise){int la=a,ls=legatoString(ch,n,la);if(ls>=0){a=la;s=ls;legato=true;}}
    if(!bank->hasArt(a))return;
    int rootNote=bank->nearest(a,n);
    if(rootNote<0 || std::abs(n-rootNote)>24)return;
    // Held string-select key (Efimov A-1..F0): play on that string when the note exists there.
    const int picked=pickedString.load(std::memory_order_relaxed);
    if(!noise && !legato && s<0 && picked>=0 && bank->onString(a,rootNote,picked)){
        s=picked;int f=rootNote-tuning[s];if(moveHand(f,f))fretNoise(ch);
    }
    bool chosen=s<0 || !bank->onString(a,rootNote,s);
    if(chosen)s=stringFor(a,rootNote,ch);
    if(s<0)return;
    if((chosen||legato) && !noise){int f=rootNote-tuning[s];if(moveHand(f,f))fretNoise(ch);}
    int id=bank->choose(a,rootNote,s,v,noise?++noiseCount:repeats[ch-1][n]++);
    if(id<0)return;
    // One note per string: a new note on a string stops the one already ringing there.
    if(!noise)for(auto& o:voices)if(o.active && !o.noise && !o.released && o.chan==ch && o.string==s){o.released=true;o.pedal=false;}
    int slot=freeSlot();if(voices[slot].active)steals.fetch_add(1,std::memory_order_relaxed);
    const auto& take=bank->takes[id];
    Voice voice;
    voice.active=true;voice.key=n;voice.chan=ch;voice.string=s;voice.shownFret=n-tuning[s];
    voice.id=id;voice.started=timeline;voice.noise=noise;voice.art=a;
    voice.vibrato=!noise && (heldArt>=0?heldArt:(int)param("art"))==6 && a==0;
    voice.step=take.rate/rate*std::exp2((n-rootNote)/12.);
    voice.amp=(noise?.085f:.77f)*std::pow(jlimit(.001f,1.f,v),.95f);
    voice.ticket=(uint64_t(++serial)<<32)|uint32_t(id+1);
    if(duration>=0)voice.autoOff=timeline+std::max<int64_t>(duration,(int64_t)(rate*.018));
    voices[slot]=voice;bank->request(slot,voice.ticket);
}
void Processor::keyUp(int ch,int n){
    for(auto& p:pending)if(p.phase && p.channel==ch && p.note==n && p.duration<0)p.duration=timeline-p.pressed;
    // Release sample (finger lifting off the string). The LP/TC ones are almost full-scale and still
    // carry the note, so they follow how loud the note still is, and muted notes get none.
    float loudest=0;
    for(auto& v:voices)if(v.active && !v.noise && v.key==n && v.chan==ch && !v.released){
        if(pedals[ch-1])v.pedal=true;
        else{v.released=true;if(v.art!=1 && v.art!=2)loudest=std::max(loudest,v.level);}
    }
    const float strength=std::min(1.f,loudest/.45f);
    if(strength>.05f && param("handNoise")>.5f){releaseNoises.fetch_add(1,std::memory_order_relaxed);launch(ch,n,.65f*strength,-1,-1,true);}
}
void Processor::message(const MidiMessage& m){
    int ch=m.getChannel();if(ch<1||ch>16)return;
    if(m.isNoteOn() && keySwitch(ch,m.getNoteNumber(),m.getVelocity(),true))return;
    if(m.isNoteOff() && keySwitch(ch,m.getNoteNumber(),m.getVelocity(),false))return;
    if(m.isNoteOn()){
        int n=m.getNoteNumber();auto vel=m.getFloatVelocity();
        // Pick mode waits 20 ms (Efimov "Chord mode A") so the notes of a chord share one position.
        const double window=(selection()&1)?.008:.020;
        for(auto& p:pending)if(!p.phase){p={1,n,ch,-1,vel,timeline,timeline+(int64_t)(rate*window),-1};return;}
        launch(ch,n,vel);
    }else if(m.isNoteOff())keyUp(ch,m.getNoteNumber());
    else if(m.isControllerOfType(1))modWheel[ch-1]=m.getControllerValue()/127.f;
    else if(m.isPitchWheel())bends[ch-1]=std::exp2((m.getPitchWheelValue()-8192)/(8192.*6.));
    else if(m.isAllSoundOff()||m.isAllNotesOff()){
        for(int i=0;i<slots;++i)if(voices[i].chan==ch)silence(i);
        for(auto& p:pending)if(p.channel==ch)p.phase=0;
        pedals[ch-1]=false;
    }else if(m.isControllerOfType(64)){
        bool pressed=m.getControllerValue()>=64;
        if(!pressed)for(auto& v:voices)if(v.active && v.chan==ch && v.pedal){v.pedal=false;v.released=true;}
        pedals[ch-1]=pressed;
    }
}
// Ilya Efimov key switches (Kontakt names, C3 = 60). Velocity above 100 = "high velo".
bool Processor::keySwitch(int ch,int n,int vel,bool on){
    const bool high=vel>100;
    auto stringKey=[](int k){switch(k){case 21:return 0;case 23:return 1;case 24:return 2;case 26:return 3;case 28:return 4;case 29:return 5;default:return -1;}};
    if(int s=stringKey(n);s>=0){                       // A-1 B-1 C0 D0 E0 F0
        if(on){
            if(openMode.load() && vel<127){              // open-string mode: the key plays the open string
                for(auto& o:voices)if(o.active && !o.noise && !o.released && o.chan==ch && o.string==s){o.released=true;o.pedal=false;}
                int a=effectiveArt(),id=bank->hasArt(a)?bank->choose(a,tuning[s],s,vel/127.f,repeats[ch-1][tuning[s]]++):-1;
                if(id>=0){
                    const auto& take=bank->takes[id];Voice voice;voice.active=true;voice.key=tuning[s];voice.chan=ch;voice.string=s;
                    voice.shownFret=0;voice.id=id;voice.started=timeline;voice.step=take.rate/rate*std::exp2((tuning[s]-take.note)/12.);
                    voice.amp=.77f*std::pow(jlimit(.001f,1.f,vel/127.f),.95f);voice.ticket=(uint64_t(++serial)<<32)|uint32_t(id+1);
                    int slot=freeSlot();voices[slot]=voice;bank->request(slot,voice.ticket);openHeld[(size_t)s]=true;
                }
            }else pickedString.store(s);
        }else{
            if(openHeld[(size_t)s]){openHeld[(size_t)s]=false;keyUp(ch,tuning[s]);}
            if(pickedString.load()==s)pickedString.store(-1);
        }
        return true;
    }
    if(n==27){if(on)openMode.store(high);return true;}           // D#0 open strings on/off
    if(n==39){if(on)legatoOn.store(high);return true;}           // D#1 legato on/off
    if(n==33){legatoHold=on;return true;}                         // A0 hold = legato
    // Held articulations: F#0 harmonics, G0 vibrato, G#0/A#0 slide, B0 palm mute, C#1 gliss.
    int art=-1;
    switch(n){case 30:art=2;break;case 31:art=6;break;case 32:case 34:art=5;break;case 35:art=1;break;case 37:art=high?8:7;break;default:break;}
    if(art>=0){if(on)heldArt=art;else if(heldArt==art || (n==37 && (heldArt==7||heldArt==8)))heldArt=-1;return true;}
    if(n>=22 && n<=38)return true;                               // other Efimov switches: silent
    if(n>=88 && n<=107){                                          // E5..B6 playing position
        if(on){
            int h=handLow(hand.load());
            if(n==88 && high)h=std::max(1,h-1);                   // E5 high: one fret down
            else if(n==89 && high)h=h+1;                          // F5 high: one fret up
            else h=n-87;                                          // E5 = 1st position, F5 = 2nd...
            h=juce::jlimit(1,lastPosition(),h);                      // keys above stay on the last real position
            const bool plain=!(high && (n==88 || n==89));
            if(plain && handLocked.load() && handLow(hand.load())==h){handLocked.store(false);return true;}  // same key again: automatic
            handLocked.store(true);                               // a chosen position stays put
            if(handLow(hand.exchange(h))!=h)fretNoise(ch);
        }
        return true;
    }
    return false;
}
void Processor::planChord(int channel){
    std::array<Pending*,128> notes{};int size=0;
    for(auto& p:pending)if(p.phase==1 && p.channel==channel)notes[size++]=&p;
    std::sort(notes.begin(),notes.begin()+size,[](auto a,auto b){return a->note<b->note;});
    int art=effectiveArt();
    // A single note with a held string key or a legato partner picks its string in launch().
    int legatoArt=0;
    const bool single=size==1 && (pickedString.load()>=0 || legatoString(channel,notes[0]->note,legatoArt)>=0);
    // Find an ascending, physically playable assignment with the smallest combined fret distance.
    if(size<=6 && !single){
        std::array<int,6> strings{},bestStrings{};int best=100000;
        auto assign=[&](auto&& self,int pos,int low,int cost)->void{
            if(cost>=best)return;
            if(pos==size){best=cost;bestStrings=strings;return;}
            int rootNote=bank->nearest(art,notes[pos]->note);if(rootNote<0)return;
            for(int s=low;s<=6-(size-pos);++s)if(bank->onString(art,rootNote,s)){
                strings[pos]=s;self(self,pos+1,s+1,cost+stringCost(channel,s,rootNote-tuning[s]));
            }
        };
        assign(assign,0,0,0);
        if(best<100000){
            int lowest=99,highest=0;
            for(int j=0;j<size;++j){
                notes[j]->string=bestStrings[j];
                int f=bank->nearest(art,notes[j]->note)-tuning[bestStrings[j]];
                if(f>0){lowest=std::min(lowest,f);highest=std::max(highest,f);}
            }
            if(highest>0 && moveHand(lowest,highest))fretNoise(channel);
        }
    }
    const bool strum=(selection()&1)!=0;
    int dir=(int)param("stroke");bool up=dir==1 || (dir==2 && upNext[channel-1]);
    if(strum && dir==2)upNext[channel-1]=!upNext[channel-1];
    int64_t spread=strum?juce::roundToInt(rate*param("spread")*.001):0;
    for(int i=0;i<size;++i){notes[i]->phase=2;notes[i]->when=timeline+spread*(up?(size-1-i):i);}
}
void Processor::dispatch(){
    for(auto& p:pending)if(p.phase==1 && p.when<=timeline)planChord(p.channel);
    for(auto& p:pending)if(p.phase==2 && p.when<=timeline){
        launch(p.channel,p.note,p.velocity,p.string,p.duration);p.phase=0;
    }
}
void Processor::renderVoices(AudioBuffer<float>& buf,int start,int length){
    const float releaseFactor=std::exp(-6.91f/(float)(rate*.001*param("release")));
    const float levelFall=std::exp(-1.f/(float)(rate*.05));
    for(int i=0;i<slots;++i){
        auto& v=voices[i]; if(!v.active)continue;
        const auto& t=bank->takes[v.id];
        const float* left=nullptr;const float* right=nullptr;int lastPage=-1;
        auto* outL=buf.getWritePointer(0,start);auto* outR=buf.getWritePointer(1,start);
        const double speed=v.step*bends[v.chan-1]*manualBend;
        float mod=std::max(manualMod,modWheel[v.chan-1]);
        float declick=(float)std::min(1.,(timeline-v.started)/(rate*attackSeconds));
        for(int j=0;j<length;++j){
            int src=(int)v.pos;
            if(src>=t.frames-1 || v.envelope<.00025f){silence(i);break;}
            if(v.autoOff>=0 && timeline+j>=v.autoOff){v.released=!pedals[v.chan-1];v.pedal=!v.released;v.autoOff=-1;}
            int pg=src/pageFrames,at=src%pageFrames;
            if(pg!=lastPage){
                const AudioBuffer<float>* data=nullptr;
                if(!pg)data=&t.attack;else if(auto* p=bank->page(i,v.ticket,pg,isNonRealtime()))data=&p->audio;
                if(!data)underruns.fetch_add(1,std::memory_order_relaxed);
                left=data?data->getReadPointer(0):nullptr;right=data?data->getReadPointer(1):nullptr;
                lastPage=pg;
            }
            if(v.released)v.envelope*=releaseFactor;
            declick=std::min(1.f,declick+(float)(1./(rate*attackSeconds)));
            if(left){float frac=(float)(v.pos-src),level=v.amp*v.envelope*declick;
                float l=(left[at]+frac*(left[at+1]-left[at]))*level,r=(right[at]+frac*(right[at+1]-right[at]))*level;
                outL[j]+=l;outR[j]+=r;v.level=std::max(std::max(std::abs(l),std::abs(r)),v.level*levelFall);
            }
            double vibrato=1.;
            if(v.vibrato || mod>.001f){double age=(timeline+j-v.started)/rate;double depth=std::max<double>(mod*.012,v.vibrato?.0037*std::clamp((age-.16)*5.,0.,1.):0.);vibrato+=depth*std::sin(MathConstants<double>::twoPi*5.2*age);}
            v.pos+=speed*vibrato;
        }
    }
    processEffects(buf,start,length);
}
void Processor::processEffects(AudioBuffer<float>& buf,int start,int length){
    auto* left=buf.getWritePointer(0,start);auto* right=buf.getWritePointer(1,start);
    const bool amp=param("ampOn")>.5f,eq=param("eqOn")>.5f,comp=param("compOn")>.5f;
    const bool chorus=param("chorusOn")>.5f,delay=param("echoOn")>.5f,room=param("roomOn")>.5f;
    const float width=param("width")*.01f,echoMix=param("echo")*.01f;
    const int echoLen=echoLine.getNumSamples(),chorLen=chorusLine.getNumSamples();
    auto* dl=echoLine.getWritePointer(0);auto* dr=echoLine.getWritePointer(1);
    auto* cl=chorusLine.getWritePointer(0);auto* cr=chorusLine.getWritePointer(1);
    for(int j=0;j<length;++j){
        float l=left[j],r=right[j],f=cutoff.getNextValue();
        if(amp){l=std::tanh(2.8f*l)*.77f;r=std::tanh(2.8f*r)*.77f;}
        damping[0]+=f*(l-damping[0]);damping[1]+=f*(r-damping[1]);
        if(eq){l=damping[0];r=damping[1];}
        {float k=compressor.gain(l,r);if(comp){l*=k;r*=k;}}
        cl[chorusPos]=l;cr[chorusPos]=r;
        if(chorus){
            auto tap=[&](const float* line,double phase){double p=chorusPos-rate*(.014+.0027*std::sin(phase));if(p<0)p+=chorLen;
                int k=(int)p;return line[k]+(line[(k+1)%chorLen]-line[k])*(float)(p-k);};
            l=.78f*l+.22f*tap(cl,chorusPhase);r=.78f*r+.22f*tap(cr,chorusPhase+1.5);
        }
        chorusPhase+=MathConstants<double>::twoPi*.8/rate;if(chorusPhase>MathConstants<double>::twoPi)chorusPhase-=MathConstants<double>::twoPi;
        chorusPos=(chorusPos+1)%chorLen;
        float delayedL=dl[echoPos],delayedR=dr[echoPos];
        dl[echoPos]=delay?l+.34f*delayedR:0;dr[echoPos]=delay?r+.34f*delayedL:0;
        echoPos=(echoPos+1)%echoLen;
        if(delay){l+=delayedL*echoMix;r+=delayedR*echoMix;}
        left[j]=l;right[j]=r;
    }
    float mix=room?param("room")*.01f:0.f;
    if(mix!=lastReverb){
        if(lastReverb>0 && mix==0)reverb.reset();
        Reverb::Parameters settings;settings.roomSize=.67f;settings.damping=.58f;settings.wetLevel=mix*.45f;
        settings.dryLevel=(1.f-.25f*mix)*.5f;settings.width=1.f;reverb.setParameters(settings);lastReverb=mix;
    }
    if(mix>0)reverb.processStereo(left,right,length);
    for(int j=0;j<length;++j){
        float g=gain.getNextValue(),mid=(left[j]+right[j])*.5f,side=(left[j]-right[j])*.5f*width;
        left[j]=std::tanh((mid+side)*g);right[j]=std::tanh((mid-side)*g);
    }
}
void Processor::renderPart(AudioBuffer<float>& buf,int start,int length){
    while(length>0){
        dispatch();int n=length;
        for(const auto& p:pending)if(p.phase && p.when>timeline && p.when<timeline+n)n=(int)(p.when-timeline);
        renderVoices(buf,start,n);timeline+=n;start+=n;length-=n;
    }
}
void Processor::processBlock(AudioBuffer<float>& buf,MidiBuffer& midi){
    ScopedNoDenormals noDenormals;buf.clear();
    if(!licence->isUnlocked()){midi.clear();return;}
    int which=selection();
    if(isNonRealtime() && (loaded.load()!=which || readyRevision.load()!=pathRevision.load())){
        for(int i=0;i<15000 && (loaded.load()!=which || readyRevision.load()!=pathRevision.load());++i){
            if(failedSelection.load()==which && readyRevision.load()==pathRevision.load())break;
            Thread::sleep(2);
        }
    }
    SpinLock::ScopedTryLockType lock(bankLock);
    if(!lock.isLocked() || !bank || bank->selection!=which || readyRevision.load()!=pathRevision.load()){midi.clear();return;}
    keyboard.processNextMidiBuffer(midi,0,buf.getNumSamples(),true);
    float hz=1250.f*std::pow(16.f,param("tone")*.01f);
    cutoff.setTargetValue(1-std::exp((float)(-MathConstants<double>::twoPi*std::min(hz,(float)(rate*.47))/rate)));
    gain.setTargetValue(Decibels::decibelsToGain(param("output")));
    attackSeconds=param("attack")*.001f;manualBend=std::exp2(param("wheel")/12.);manualMod=param("mod")*.01f;
    int pos=0;
    for(const auto meta:midi){int at=jlimit(pos,buf.getNumSamples(),meta.samplePosition);renderPart(buf,pos,at-pos);message(meta.getMessage());pos=at;}
    midi.clear();renderPart(buf,pos,buf.getNumSamples()-pos);
    std::array<int,6> visible;visible.fill(-1);
    for(const auto& v:voices)if(v.active && !v.released && !v.noise)visible[v.string]=v.shownFret;
    for(int s=0;s<6;++s)frets[s].store(visible[s],std::memory_order_relaxed);
}
void Processor::getStateInformation(MemoryBlock& data){
    auto tree=params.copyState();tree.setProperty("library",getLibrary(),nullptr);if(auto xml=tree.createXml())copyXmlToBinary(*xml,data);
}
void Processor::setStateInformation(const void* data,int bytes){
    if(auto xml=getXmlFromBinary(data,bytes))if(xml->hasTagName(params.state.getType())){
        auto tree=ValueTree::fromXml(*xml);auto path=tree.getProperty("library",getLibrary()).toString();
        params.replaceState(tree);if(File::isAbsolutePath(path) && path!=getLibrary())setLibrary(File(path));notify();
    }
}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new guitar::Processor();}
