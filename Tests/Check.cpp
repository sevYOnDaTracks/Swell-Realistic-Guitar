#include "Processor.h"
#include "Compressor.h"
#include <iostream>
#include <stdexcept>
using namespace juce;
using guitar::Processor;
static void require(bool ok,const std::string& msg){if(!ok)throw std::runtime_error(msg);}
static void set(Processor& p,const char* key,int v){auto* a=p.params.getParameter(key);a->setValueNotifyingHost(a->convertTo0to1((float)v));}
static void play(Processor& p,MidiBuffer& midi,double* energy=nullptr,int size=256){
    AudioBuffer<float> buf(2,size);p.processBlock(buf,midi);
    for(int c=0;c<2;++c)for(int i=0;i<size;++i){float s=buf.getSample(c,i);require(std::isfinite(s),"non-finite output");if(energy)*energy+=s*s;}
}
int main(int argc,char** argv) {
 try {
    ScopedJuceInitialiser_GUI init;swell::Licence::unlockForTesting();Processor p;p.prepareToPlay(48000,256);p.setNonRealtime(true);
    const bool preview=argc==2;
    for(int pass: {1,0,2,3,4,5,6,7}){
        const int sel=preview?5:pass;
        if(preview){set(p,"stroke",2);set(p,"ampOn",1);set(p,"roomOn",1);set(p,"tone",79);set(p,"width",114);set(p,"room",18);set(p,"spread",14);set(p,"release",170);}
        set(p,"model",sel/2);set(p,"mode",sel%2);
        MidiBuffer midi;play(p,midi);
        require(p.loaded==sel && p.missing==0,"bank failed "+std::to_string(sel));
        if (!preview && sel >= 4) {
            set(p,"art",1);
            set(p,"handNoise",0);
            for (int note : {40, 52, 64})
                for (int velocity : {12, 32, 64, 100, 127}) {
                    for (int repeat = 0; repeat < 2; ++repeat) {
                        midi.addEvent(MidiMessage::allSoundOff(1),0); play(p,midi);
                        midi.addEvent(MidiMessage::noteOn(1,note,(uint8)velocity),0);
                        double tailEnergy = 0;
                        for (int block = 0; block < 24; ++block)
                            play(p,midi,block >= 8 ? &tailEnergy : nullptr);
                        require(tailEnergy > .000001, "muted note lost its tail: " + std::to_string(sel) + "/" + std::to_string(note) + "/" + std::to_string(velocity));
                    }
                }
            set(p,"handNoise",1);
        }
        for(int art=0;art<(preview?1:(int)guitar::arts.size());++art){
            set(p,"art",art);
            midi.addEvent(MidiMessage::allSoundOff(1),0);
            for(int n: {48,55,64})midi.addEvent(MidiMessage::noteOn(1,n,(uint8)98),5);
            double attack=0,tail=0;
            for(int k=0;k<95;++k)play(p,midi,k<34?&attack:&tail);
            require(attack>.00001,"silent articulation "+std::to_string(sel)+"/"+std::to_string(art));
            if(art==0)require(tail>.01,"silent streamed tail");
            if(preview){
                std::unique_ptr<AudioProcessorEditor> ed(p.createEditor());
                if(std::getenv("SWELL_BOARD")){
                    midi.addEvent(MidiMessage::allSoundOff(1),0);play(p,midi);p.hand.store(7);
                    for(int n:{60,64,67})midi.addEvent(MidiMessage::noteOn(1,n,(uint8)98),0);
                    for(int k=0;k<4;++k)play(p,midi);
                    for(auto* c:ed->getChildren())if(auto* b=dynamic_cast<Button*>(c))if(b->getName()=="Clavier / Manche" && b->onClick)b->onClick();
                }
                auto img=ed->createComponentSnapshot(ed->getLocalBounds(),true,2.0f);
                FileOutputStream out(File::getCurrentWorkingDirectory().getChildFile(argv[1]));out.setPosition(0);out.truncate();
                require(PNGImageFormat().writeImageToStream(img,out),"could not save image");out.flush();
                std::cout<<"preview "<<img.getWidth()<<" × "<<img.getHeight()<<"\n";return 0;
            }
            for(int n:{48,55,64})midi.addEvent(MidiMessage::noteOff(1,n),1);play(p,midi);
        }
        std::cout<<"model/mode "<<sel<<"   "<<p.count.load()<<" mappings, 9 articulations, no missing media"<<std::endl;
    }
    {   // Hand position: E4 from first position vs. after climbing the neck.
        set(p,"model",1);set(p,"mode",0);set(p,"art",0);MidiBuffer midi;play(p,midi);
        auto strike=[&](int note){
            midi.addEvent(MidiMessage::allSoundOff(1),0);midi.addEvent(MidiMessage::noteOn(1,note,(uint8)98),5);
            for(int k=0;k<8;++k)play(p,midi);   // pick mode waits 20 ms to detect chords
            int s=-1;for(int k=0;k<6;++k)if(p.frets[k].load()>=0)s=k;
            midi.addEvent(MidiMessage::noteOff(1,note),0);play(p,midi);return s;
        };
        p.hand.store(0);int low=strike(64);
        for(int n:{69,74,76})strike(n);
        int handHigh=p.hand.load();int high=strike(64);
        std::cout<<"E4 string "<<low<<" in first position, string "<<high<<" with hand at fret "<<handHigh<<"\n";
        require(low>=0 && high>=0 && handHigh>3 && high<low,"hand position does not change the string");
        // Chord in pick mode: every note on its own string.
        p.hand.store(0);midi.addEvent(MidiMessage::allSoundOff(1),0);
        for(int n:{48,52,55,60,64})midi.addEvent(MidiMessage::noteOn(1,n,(uint8)90),3);
        for(int k=0;k<8;++k)play(p,midi);
        int used=0;for(int k=0;k<6;++k)if(p.frets[k].load()>=0)++used;
        std::cout<<"C chord uses "<<used<<" strings\n";require(used==5,"chord notes share a string");
        for(int n:{48,52,55,60,64})midi.addEvent(MidiMessage::noteOff(1,n),0);play(p,midi);
        // Efimov key switches.
        auto run=[&](int blocks){for(int k=0;k<blocks;++k)play(p,midi);};
        auto shown=[&](int s){return p.frets[s].load();};
        midi.addEvent(MidiMessage::allSoundOff(1),0);p.hand.store(0);
        midi.addEvent(MidiMessage::noteOn(1,26,(uint8)90),0);midi.addEvent(MidiMessage::noteOn(1,64,(uint8)90),2);run(8);
        require(shown(3)==9,"string key D0 did not put E4 on the G string");
        midi.addEvent(MidiMessage::noteOff(1,64),0);midi.addEvent(MidiMessage::noteOff(1,26),0);run(2);
        midi.addEvent(MidiMessage::noteOn(1,95,(uint8)90),0);run(1);
        require(p.hand.load()==8,"position key B5 did not set position 8");
        midi.addEvent(MidiMessage::noteOn(1,88,(uint8)120),0);run(1);
        require(p.hand.load()==7,"E5 high velocity did not shift the position down");
        // Highest green key then D4: the hand stays on the last real position and D4 is played there.
        midi.addEvent(MidiMessage::allSoundOff(1),0);midi.addEvent(MidiMessage::noteOn(1,107,(uint8)90),0);run(1);
        require(p.hand.load()==p.lastPosition(),"B6 did not stop on the last real position");
        for(int k=0;k<3;++k){midi.addEvent(MidiMessage::noteOn(1,62,(uint8)90),0);run(8);midi.addEvent(MidiMessage::noteOff(1,62),0);run(2);}
        midi.addEvent(MidiMessage::noteOn(1,62,(uint8)90),0);run(8);
        int at=-1;for(int s=0;s<6;++s)if(shown(s)>0)at=shown(s);
        require(p.hand.load()==p.lastPosition() && at>=p.lastPosition() && at<=p.lastPosition()+4,"chosen position did not stay");
        midi.addEvent(MidiMessage::noteOff(1,62),0);midi.addEvent(MidiMessage::noteOn(1,103,(uint8)90),0);run(2);
        require(!p.handLocked.load(),"same position key did not return to automatic");
        std::cout<<"Position B6 stays at fret "<<p.hand.load()<<", D4 played at fret "<<at<<"\n";
        midi.addEvent(MidiMessage::noteOn(1,94,(uint8)90),0);run(1);   // lock position 7 for the next checks
        midi.addEvent(MidiMessage::allSoundOff(1),0);p.hand.store(0);
        midi.addEvent(MidiMessage::noteOn(1,39,(uint8)120),0);midi.addEvent(MidiMessage::noteOn(1,60,(uint8)90),1);run(8);
        int first=-1;for(int s=0;s<6;++s)if(shown(s)>=0)first=s;
        midi.addEvent(MidiMessage::noteOn(1,62,(uint8)90),0);run(8);
        int lit=0;for(int s=0;s<6;++s)if(shown(s)>=0)++lit;
        require(first>=0 && lit==1 && shown(first)==62-guitar::tuning[first],"legato note left its string");
        midi.addEvent(MidiMessage::noteOff(1,60),0);midi.addEvent(MidiMessage::noteOff(1,62),0);midi.addEvent(MidiMessage::noteOn(1,39,(uint8)40),0);run(2);
        midi.addEvent(MidiMessage::allSoundOff(1),0);p.hand.store(7);
        midi.addEvent(MidiMessage::noteOn(1,27,(uint8)120),0);midi.addEvent(MidiMessage::noteOn(1,28,(uint8)90),1);run(4);
        require(shown(4)==0 && p.hand.load()==7,"open-string key E0 did not ring the open B string");
        midi.addEvent(MidiMessage::noteOff(1,28),0);midi.addEvent(MidiMessage::noteOn(1,27,(uint8)40),0);run(2);
        std::cout<<"Key switches: string, position, shift, legato and open string passed\n";
    }
    {   // Melody played in real time (Le Jazz Guitar Afro settings): count everything that can sound like noise.
      for(int art:{6,1}){
        set(p,"handNoise",std::getenv("SWELL_QUIET")?0:1);set(p,"model",2);set(p,"mode",0);set(p,"art",art);set(p,"ampOn",0);set(p,"compOn",0);set(p,"release",230);
        MidiBuffer midi;play(p,midi);while(p.loaded.load()!=p.selection())Thread::sleep(20);
        midi.addEvent(MidiMessage::allSoundOff(1),0);for(int k=0;k<200;++k)play(p,midi);   // start from silence
        p.handLocked.store(false);p.hand.store(0);
        p.underruns=0;p.steals=0;p.fretNoises=0;p.releaseNoises=0;
        const int melody[]={64,67,69,71,74,72,71,69,67,64,62,64,67,69,72,76,74,72,71,69,67,69,71,72,74,76,79,76,74,72,71,67};
        p.setNonRealtime(!std::getenv("SWELL_REALTIME")?true:false);
        const int block=256,step=48000*125/1000;   // sixteenth notes at 120 bpm
        AudioBuffer<float> all(2,48000*6);int written=0,next=0,idx=0,held=-1;
        auto t0=Time::getMillisecondCounterHiRes();
        for(int pos=0;pos+block<=all.getNumSamples();pos+=block){
            while(next<pos+block && idx<(int)std::size(melody)){
                int at=std::max(0,next-pos);
                midi.addEvent(MidiMessage::noteOn(1,melody[idx],(uint8)(80+idx%3*12)),at);
                if(held>=0)midi.addEvent(MidiMessage::noteOff(1,held),std::min(block-1,at+40));
                held=melody[idx++];next+=step;
            }
            if(idx==(int)std::size(melody) && held>=0 && next<pos+block){midi.addEvent(MidiMessage::noteOff(1,held),0);held=-1;}
            AudioBuffer<float> buf(2,block);p.processBlock(buf,midi);midi.clear();
            for(int c=0;c<2;++c)all.copyFrom(c,pos,buf,c,0,block);written=pos+block;
            if(std::getenv("SWELL_REALTIME")){double due=t0+(pos+block)*1000./48000;while(Time::getMillisecondCounterHiRes()<due)Thread::sleep(1);}
        }
        std::cout<<(art==1?"Palm-mute melody: ":"Vibrato melody: ")<<p.underruns.load()<<" late pages, "<<p.steals.load()<<" cut voices, "<<p.fretNoises.load()<<" fret noises, "<<p.releaseNoises.load()<<" release noises\n";
        if(art==1)require(p.releaseNoises.load()==0,"palm-mute notes still trigger release samples");
        if(auto* env=std::getenv("SWELL_MELODY")){String out=String(env)+(art==1?"_mute.wav":"_vib.wav");WavAudioFormat wav;File f(File::getCurrentWorkingDirectory().getChildFile(out));f.deleteFile();
            std::unique_ptr<AudioFormatWriter> w(wav.createWriterFor(new FileOutputStream(f),48000,2,24,{},0));w->writeFromAudioSampleBuffer(all,0,written);}
        p.setNonRealtime(true);
      }
      MidiBuffer midi;set(p,"art",0);set(p,"model",3);set(p,"mode",1);play(p,midi);
    }
    {   // Compressor distortion over a whole plucked note (attack included): the old per-sample one against the new one.
        auto thd=[](const std::vector<float>& x,double hz,int from){
            auto bin=[&](double f){double re=0,im=0;const double n=(double)(x.size()-(size_t)from);for(size_t i=(size_t)from;i<x.size();++i){double win=.5-.5*std::cos(MathConstants<double>::twoPi*(double)(i-(size_t)from)/n),w=MathConstants<double>::twoPi*f*(double)i/48000.;re+=win*x[i]*std::cos(w);im+=win*x[i]*std::sin(w);}return re*re+im*im;};
            double h=0;for(int k=2;k<=10;++k)h+=bin(hz*k);return 10*std::log10(h/bin(hz));};
        for(double hz:{82.41,196.0}){
            std::vector<float> oldOut,newOut;float env=0;guitar::Compressor c;c.prepare(48000);
            const float follower=std::exp(-1.f/(.08f*48000));
            for(int i=0;i<48000;++i){
                float x=.7f*std::exp(-(float)i/48000.f*1.5f)*(float)std::sin(MathConstants<double>::twoPi*hz*i/48000.);
                float o=x;float peak=std::abs(x);env=std::max(peak,env*follower);if(env>.2f)o*=(.2f+(env-.2f)*.3f)/env;
                oldOut.push_back(o);newOut.push_back(x*c.gain(x,x));
            }
            double a=thd(oldOut,hz,0),b=thd(newOut,hz,0);
            std::cout<<"Compressor at "<<hz<<" Hz: distortion "<<a<<" dB before, "<<b<<" dB now\n";
            require(b<-50 && b<a-15,"compressor still distorts");
        }
    }
    MemoryBlock saved;p.getStateInformation(saved);set(p,"mode",0);set(p,"model",0);p.setStateInformation(saved.getData(),(int)saved.getSize());
    require(p.selection()==7,"recall of model/mode failed");
    p.setLibrary(File("/missing-swell-test-library"));MidiBuffer midi;play(p,midi);require(p.failed.load(),"missing library state not reported");
    std::cout<<"State recall and missing folder passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
