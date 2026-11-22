#pragma once
#include "Bank.h"
#include "Compressor.h"
#include "Licence/SwellLicence.h"
#include <juce_audio_utils/juce_audio_utils.h>
namespace guitar {
class Processor final : public juce::AudioProcessor, private juce::Thread {
public:
    Processor(); ~Processor() override;
    juce::AudioProcessorValueTreeState params;
    juce::MidiKeyboardState keyboard;
    juce::SharedResourcePointer<swell::Licence> licence;
    std::array<std::atomic<int>,6> frets;
    // Fretting-hand position: lowest fret under the index finger (0 = open position).
    // Same rules as the Ilya Efimov script: a 4-fret playing position (the orange bar),
    // extended by one fret upwards; the position shifts only when a note falls outside.
    static constexpr int handSpan = 4, handReach = 1;
    std::atomic<int> hand {0};
    // Highest position whose 4 frets all exist in the loaded guitar's samples.
    int lastPosition() const;
    // Set by the green position keys: the hand then stays where the player put it.
    std::atomic<bool> handLocked {false};
    static int handLow(int h) noexcept {return h<1?1:h;}
    // Key-switch state (shown by the editor): held string key, open-string mode, legato mode.
    std::atomic<int> pickedString {-1};
    std::atomic<bool> openMode {false}, legatoOn {false};
    static constexpr int legatoRange = 5;
    std::atomic<int> loaded {-1}, progress {0}, count {0}, missing {0};
    std::atomic<bool> failed {false};
    // Diagnostics: streamed pages that arrived too late, voices cut while sounding, added noises.
    std::atomic<int> underruns {0}, steals {0}, fretNoises {0}, releaseNoises {0};
    int selection() const noexcept;
    void setLibrary(const juce::File&);
    juce::String getLibrary() const;
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layout) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    const juce::String getName() const override {return JucePlugin_Name;}
    bool acceptsMidi() const override {return true;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 5.;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
private:
    struct Voice {
        bool active=false, released=false, pedal=false, noise=false, vibrato=false;
        int id=0, key=0, chan=1, string=0, shownFret=0, art=0;
        float level=0;   // recent output peak, so the release sample never sounds louder than the note
        uint64_t ticket=0;
        int64_t started=0, autoOff=-1;
        double pos=0, step=1;
        float amp=0, envelope=1;
    };
    struct Pending {
        int phase=0, note=0, channel=1, string=-1;
        float velocity=0;
        int64_t pressed=0, when=0, duration=-1;
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void run() override;
    void resetVoices();
    void message(const juce::MidiMessage&);
    void launch(int ch,int note,float v,int string=-1,int64_t duration=-1,bool noise=false);
    void keyUp(int ch,int note);
    void processEffects(juce::AudioBuffer<float>&,int,int);
    void renderPart(juce::AudioBuffer<float>&,int start,int length);
    void renderVoices(juce::AudioBuffer<float>&,int start,int length);
    void dispatch();
    void planChord(int channel);
    int stringFor(int art,int root,int ch) const;
    int fretCost(int fret) const;
    int stringCost(int ch,int string,int fret) const;
    bool moveHand(int lowest,int highest);
    void fretNoise(int ch);
    int freeSlot() const;
    int scratchCount=0;unsigned noiseCount=0;
    int legatoString(int ch,int note,int& art) const;
    bool keySwitch(int ch,int note,int velocity,bool on);
    int heldArt=-1;
    bool legatoHold=false;
    std::array<bool,6> openHeld {};
    int effectiveArt() const;
    void silence(int slot);
    float param(const char* key) const {return params.getRawParameterValue(key)->load();}
    std::unique_ptr<Bank> bank;
    juce::SpinLock bankLock;
    mutable juce::CriticalSection pathLock;
    juce::String root="/Volumes/SSD/Swell Guitar";
    std::atomic<unsigned> pathRevision {0}, readyRevision {0};
    std::atomic<int> failedSelection {-1};
    std::array<Voice,slots> voices;
    std::array<Pending,128> pending;
    std::array<std::array<unsigned,128>,16> repeats {};
    std::array<double,16> bends;
    std::array<bool,16> pedals {};
    std::array<bool,16> upNext {};
    double rate=44100;
    int64_t timeline=0;
    uint32_t serial=0;
    float damping[2] {};
    Compressor compressor;
    std::array<float,16> modWheel {};
    juce::AudioBuffer<float> echoLine,chorusLine;
    int echoPos=0,chorusPos=0;
    double chorusPhase=0;
    juce::Reverb reverb;
    float lastReverb=-1;
    float attackSeconds=.0015f;
    static constexpr double chordWindow=.020;   // chord-gathering wait, reported as latency
    double manualBend=1;
    float manualMod=0;
    juce::SmoothedValue<float> gain, cutoff;
};
}
