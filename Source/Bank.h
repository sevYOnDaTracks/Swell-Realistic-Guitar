#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace guitar {
inline constexpr int tuning[] {40,45,50,55,59,64};
inline constexpr int slots = 40, pageFrames = 8192, artCount = 11;
// Fret noise (fingers sliding on the strings), loaded from the scr_*.wav exports.
inline constexpr int scratchArt = 10, scratchNote = 64;
inline const juce::StringArray models {"AG","NG","LP","TC"};
inline const juce::StringArray arts {"Sustain","Palm mute","Harmonics","Hammer-on","Pull-off","Slide","Vibrato","Gliss up","Gliss down"};
struct Take {
    juce::File file;
    juce::AudioBuffer<float> attack;
    int frames = 0, note = 0, string = 0, layer = 0;
    double rate = 44100;
};
struct Page {
    uint64_t key = 0;
    int number = -1;
    juce::AudioBuffer<float> audio {2,pageFrames+1};
};
// Worker publishes pages in order. Only the audio thread advances read.
struct Stream {
    static constexpr unsigned capacity = 3;
    std::array<Page,capacity> pages;
    std::atomic<unsigned> write {0}, read {0};
    std::atomic<uint64_t> request {0};
    std::atomic<int> wanted {1};
    uint64_t workerKey = 0;
    int next = 1;
    std::unique_ptr<juce::AudioFormatReader> reader;
};
class Bank {
public:
    explicit Bank(int id) : selection(id) {}
    ~Bank();
    bool load(const juce::File& root, const std::function<bool()>& cancel,
              std::atomic<int>& progress);
    void start();
    void request(int slot, uint64_t key);
    const Page* page(int slot, uint64_t key, int number, bool offline);
    bool hasArt(int art) const { return available[(size_t)art]; }
    int nearest(int art,int note) const { return nearestNote[(size_t)art][(size_t)note]; }
    bool onString(int art,int note,int string) const;
    // Highest sustained fret reached by every string above the low E (where the hand can still play chords).
    int topFret() const {
        int top=99;
        for(int s=1;s<6;++s){int m=0;for(int n=0;n<128;++n)if(!groups[0][(size_t)n][(size_t)s].empty())m=std::max(m,n-tuningOf(s));top=std::min(top,m);}
        return top==99?19:top;
    }
    int choose(int art,int note,int string,float velocity,unsigned repeat) const;

    const int selection;
    int missing = 0;
    std::vector<Take> takes;
    std::array<bool,artCount> available {};
private:
    static int tuningOf(int s){static constexpr int t[6]{40,45,50,55,59,64};return t[s];}
    using Strings = std::array<std::vector<int>,6>;
    std::array<std::array<Strings,128>,artCount> groups;
    std::array<std::array<int,128>,artCount> nearestNote {};
    std::array<Stream,slots> streams;
    std::atomic<bool> stopping {false};
    std::thread disk;
    void serve();
};
}
