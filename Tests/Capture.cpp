// Captures real editor states for the promo film. Usage: GuitarCapture <outDir>
#include "Processor.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
using namespace juce;
using guitar::Processor;
static void pump(int ms){CFRunLoopRunInMode(kCFRunLoopDefaultMode,ms/1000.0,false);}
static void setP(Processor& p,const char* k,float v){auto* a=p.params.getParameter(k);a->setValueNotifyingHost(a->convertTo0to1(v));}
static void block(Processor& p,MidiBuffer& m){AudioBuffer<float> b(2,256);p.processBlock(b,m);m.clear();}
template<class T> static T* findChild(Component& c,std::function<bool(T&)> ok){
    for(auto* ch: c.getChildren()){if(auto* t=dynamic_cast<T*>(ch))if(ok(*t))return t;if(auto* r=findChild<T>(*ch,ok))return r;}
    return nullptr;
}
int main(int argc,char** argv){
    ScopedJuceInitialiser_GUI gui;
    if(!(argc>2&&String(argv[2])=="licence"))swell::Licence::unlockForTesting();
    const File out(argc>1?argv[1]:"/tmp");out.createDirectory();
    Processor p;p.prepareToPlay(48000,256);
    std::unique_ptr<AudioProcessorEditor> ed(p.createEditor());
    ed->setSize(1040,554);
    auto* combo=findChild<ComboBox>(*ed,[](ComboBox& c){return c.getNumItems()==9;});
    auto* view=findChild<Button>(*ed,[](Button& b){return b.getName().contains("Manche");});
    std::cout<<"combo "<<(combo!=nullptr)<<" view "<<(view!=nullptr)<<"\n";
    MidiBuffer m;
    auto waitLoad=[&]{for(int i=0;i<1500;++i){block(p,m);pump(5);if(p.loaded==p.selection()&&p.progress>=p.count)break;}for(int i=0;i<8;++i)pump(25);};
    auto preset=[&](const String& n){for(int i=1;i<=9;++i)if(combo->getItemText(i-1)==n)combo->setSelectedId(i,sendNotificationSync);waitLoad();};
    auto chord=[&](std::initializer_list<int> notes){
        m.addEvent(MidiMessage::allNotesOff(1),0);block(p,m);
        for(int n=0;n<128;++n)p.keyboard.noteOff(1,n,0.f);
        for(int n: notes){m.addEvent(MidiMessage::noteOn(1,n,(uint8)100),0);p.keyboard.noteOn(1,n,.8f);}
        for(int i=0;i<12;++i){block(p,m);pump(10);}
    };
    auto save=[&](const String& name,Rectangle<int> area,float scale){
        for(int i=0;i<8;++i)pump(25);
        auto img=ed->createComponentSnapshot(area,true,scale);
        File f=out.getChildFile(name);f.deleteFile();FileOutputStream os(f);PNGImageFormat().writeImageToStream(img,os);
        std::cout<<name<<" "<<img.getWidth()<<"x"<<img.getHeight()<<"\n";
    };
    const auto full=ed->getLocalBounds();
    if(argc>2&&String(argv[2])=="licence"){waitLoad();save("licence.png",full,2.f);
        AudioBuffer<float> b(2,512);MidiBuffer mm;mm.addEvent(MidiMessage::noteOn(1,52,(uint8)110),0);p.processBlock(b,mm);
        std::cout<<"locked peak "<<b.getMagnitude(0,512)<<"\n";return 0;}
    struct S{const char* preset;const char* file;std::initializer_list<int> notes;};
    preset("Cedar Air");chord({});save("ui_ag_idle.png",full,2.f);
    chord({48,55,64});save("ui_ag.png",full,2.f);
    preset("Nylon Close");chord({45,52,57,60});save("ui_ng.png",full,2.f);
    preset("Sunset Strum");chord({43,50,55,59});save("ui_lp.png",full,2.f);
    preset("Copper Twang");chord({40,47,52,56});save("ui_tc.png",full,2.f);
    // Articulations
    preset("Sunset Strum");chord({});
    const char* artNames[]={"sustain","palm","harm"};
    for(int a=0;a<3;++a){setP(p,"art",(float)a);save(String("art_")+artNames[a]+".png",full,2.f);}
    setP(p,"art",0);
    // Effects chain toggles
    const char* fx[]={"ampOn","eqOn","compOn","chorusOn","echoOn","roomOn"};
    for(auto* f: fx)setP(p,f,0);
    save("fx_0.png",full,2.f);
    for(int i=0;i<6;++i){setP(p,fx[i],1);save("fx_"+String(i+1)+".png",full,2.f);}
    // Tone knob sweep (macro area)
    const Rectangle<int> macro(210,232,820,104);
    for(int i=0;i<13;++i){setP(p,"tone",i*100.f/12.f);save("tone_"+String(i).paddedLeft('0',2)+".png",macro,3.f);}
    setP(p,"tone",79);
    // Fretboard view with chords
    if(view)view->triggerClick();pump(100);
    const std::vector<std::vector<int>> prog{{43,50,55,59,62,67},{45,52,57,60,64},{48,52,55,60,64},{50,57,62,66}};
    for(size_t i=0;i<prog.size();++i){
        m.addEvent(MidiMessage::allNotesOff(1),0);block(p,m);for(int n=0;n<128;++n)p.keyboard.noteOff(1,n,0.f);
        for(int n: prog[i]){m.addEvent(MidiMessage::noteOn(1,n,(uint8)100),0);p.keyboard.noteOn(1,n,.8f);}
        for(int k=0;k<12;++k){block(p,m);pump(10);}
        save("board_"+String((int)i)+".png",full,2.f);
    }
    return 0;
}
