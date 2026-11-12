#pragma once
#include <algorithm>
#include <cmath>
namespace guitar {
// Guitar bus compressor. It follows the average level (not each sample), so the gain never
// changes inside one cycle of the waveform: no distortion, even on the low E string.
// The gain change itself is smoothed: 10 ms attack lets the pick click through, 150 ms release.
struct Compressor {
    float thresholdDb=-18.f, ratio=3.f, kneeDb=6.f, makeupDb=3.f;
    void prepare(double sampleRate){
        auto coef=[&](double seconds){return (float)std::exp(-1./(seconds*sampleRate));};
        detectAttack=coef(.005);detectRelease=coef(.060);gainAttack=coef(.010);gainRelease=coef(.150);
        reset();
    }
    void reset(){power=0;reductionDb=0;}
    // Static curve: gain reduction in dB for a level in dB (soft knee around the threshold).
    float reductionFor(float levelDb) const{
        const float over=levelDb-thresholdDb,slope=1.f-1.f/ratio;
        if(over<=-kneeDb*.5f)return 0.f;
        if(over>=kneeDb*.5f)return over*slope;
        const float x=over+kneeDb*.5f;return slope*x*x/(2.f*kneeDb);
    }
    // Returns the gain to apply to this stereo sample.
    float gain(float l,float r){
        const float in=.5f*(l*l+r*r);
        power=in+(in>power?detectAttack:detectRelease)*(power-in);
        const float levelDb=10.f*std::log10(power+1e-12f)+3.f;   // RMS, +3 dB so a sine reads at its peak level
        const float target=reductionFor(levelDb);
        reductionDb=target+(target>reductionDb?gainAttack:gainRelease)*(reductionDb-target);
        return std::pow(10.f,(makeupDb-reductionDb)*.05f);
    }
    float power=0, reductionDb=0;
    float detectAttack=0, detectRelease=0, gainAttack=0, gainRelease=0;
};
}
