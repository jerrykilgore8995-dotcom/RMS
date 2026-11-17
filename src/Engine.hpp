#pragma once
#include "Programs.hpp"
#include "Resampler.hpp"
#include <limits>

namespace rmx
{
constexpr int clockHz = 40000;
constexpr int samplesPerMs = clockHz / 1000;

inline float coefficient(float hz)
{
    return 1 - std::exp(-2 * std::numbers::pi_v<float> * hz / clockHz);
}
inline float dbGain(float db) { return std::pow(10.f, db * .05f); }

class DelayLine
{
public:
    void prepare(size_t minimum)
    {
        size_t length = 1;
        while (length <= minimum)
            length *= 2;
        data.assign(length,0);
        mask = length-1;
        reset();
    }
    void reset() { count = 0; }
    void push(float sample) { data[count++ & mask] = sample; }
    float get(size_t age) const { return age < count ? data[(count-1-age)&mask] : 0; }
    float read(float age) const
    {
        const auto a = static_cast<size_t>(age);
        return std::lerp(get(a),get(a+1),age-static_cast<float>(a));
    }
private:
    std::vector<float> data;
    size_t mask = 0, count = 0;
};

class Diffuser
{
public:
    void prepare() { memory.prepare(2048); }
    void reset() { memory.reset(); }
    float process(float input, size_t age, float gain)
    {
        const float delayed = memory.get(age-1);
        const float output = delayed - gain*input;
        memory.push(input + gain*output);
        return output;
    }
private:
    DelayLine memory;
};

class Reverb
{
public:
    void prepare()
    {
        for(auto& line : network) line.prepare(8192);
        for(auto& ap : diffusion) ap.prepare();
        earlyLine.prepare(8192);
        reset();
    }
    void reset()
    {
        for(auto& line : network) line.reset();
        for(auto& ap : diffusion) ap.reset();
        earlyLine.reset();
        bass.fill(0); treble.fill(0);
        phase.fill(0);
    }
    void configure(const ProgramSpec& spec, int mode, float seconds, float lo, float hi)
    {
        constexpr std::array<int,8> primes {1151,1319,1559,1777,1999,2347,2609,2971};
        constexpr std::array<float,10> hf {18000,15000,10000,8000,6500,5000,3500,2000,1500,1000};
        size = spec.size;
        ambienceMode = mode==ambience;
        blend = ambienceMode?.180f:0.f;
        earlyGain = spec.early;
        depth = spec.modulation;
        infinite = (mode == ambience && seconds >= 9.89f) || mode == freeze;
        const float bassScale = std::exp2(lo * (2.f / 9));
        const float highScale = hi < 0 ? .22f : std::exp2(hi / 9);
        lowPole = coefficient(350);
        const float highCorner = hf[std::clamp(static_cast<int>(-hi),0,9)];
        highPole = coefficient(ambienceMode?std::min(highCorner,4200.f):highCorner);
        for (size_t i=0; i<primes.size(); ++i)
        {
            length[i] = static_cast<float>(primes[i]) * size + static_cast<float>(i%2)*.37f;
            const float time = length[i] / clockHz;
            const auto gain = [time](float rt) { return std::exp(-6.907755f * time / std::max(.02f,rt)); };
            midGain[i] = infinite ? 1.f : gain(seconds);
            lowGain[i] = lo == 0 ? midGain[i] : gain((infinite ? 9.9f : seconds)*bassScale);
            highGain[i] = hi == 0 && infinite ? 1.f : gain((infinite ? 9.9f : seconds)*highScale)
                                                                  * (1.f-spec.damping*(ambienceMode?.22f:.13f));
        }
    }
    std::array<float,2> process(float x)
    {
        earlyLine.push(x);
        std::array<float,2> out {};
        constexpr std::array<int,6> earlyL {187,361,521,708,871,1321};
        constexpr std::array<int,6> earlyR {233,397,467,761,966,1249};
        for(size_t i=0;i<earlyL.size();++i)
        {
            const auto a = static_cast<size_t>(earlyL[i]*size);
            const auto b = static_cast<size_t>(earlyR[i]*size);
            const float taper = (.36f-.034f*static_cast<float>(i));
            const float scale = earlyGain * taper * (ambienceMode ? (i>=3 ? 2.55f : .57f) : 1.f);
            out[0] += earlyLine.get(a)*scale;
            out[1] += earlyLine.get(b)*scale * (i%3==0 ? -.8f : 1.f);
        }
        constexpr std::array<int,6> spread {107,157,257,367,523,683};
        for(size_t i=0;i<(ambienceMode?diffusion.size():4u);++i)
            x = diffusion[i].process(x, static_cast<size_t>(spread[i]*size)+1, i%2==0?.68f:-.61f);
        std::array<float,8> y {};
        for(size_t i=0;i<y.size();++i)
        {
            const float modulation = infinite ? 0 : depth * triangle(phase[i]+static_cast<float>(i)*.137f);
            phase[i] += (.23f+.067f*static_cast<float>(i))/clockHz;
            if(phase[i] >= 1) phase[i] -= 1;
            const float v = network[i].read(length[i] - 1 + modulation);
            bass[i] += lowPole*(v-bass[i]);
            treble[i] += highPole*(v-treble[i]);
            y[i] = midGain[i]*v + (lowGain[i]-midGain[i])*bass[i]
                               + (highGain[i]-midGain[i])*(v-treble[i]);
            out[0] += v*(i&1 ? -.15f : .15f);
            out[1] += v*(i&2 ? -.15f : .15f);
        }
        // Orthogonal Hadamard feedback with independent stereo projections.
        for(size_t half=1;half<y.size();half*=2)
            for(size_t start=0;start<y.size();start+=2*half)
                for(size_t i=0;i<half;++i)
                {
                    const auto a=y[start+i],b=y[start+i+half];
                    y[start+i]=a+b; y[start+i+half]=a-b;
                }
        for(size_t i=0;i<network.size();++i)
            network[i].push(std::clamp(.3535533906f*y[i]+(i&4?-.36f:.36f)*x,-2.f,2.f));
        return {out[0]+blend*out[1],out[1]+blend*out[0]};
    }
private:
    static float triangle(float phase)
    {
        const auto p=phase-std::floor(phase);
        return 1-4*std::abs(p-.5f);
    }
    std::array<DelayLine,8> network;
    std::array<Diffuser,6> diffusion;
    DelayLine earlyLine;
    std::array<float,8> bass {},treble {}, length {}, midGain {}, lowGain {}, highGain {}, phase {};
    float size=1, earlyGain=0, depth=0, lowPole=0, highPole=0, blend=0;
    bool infinite=false, ambienceMode=false;
};

class Core
{
public:
    void prepare()
    {
        for(auto& line: echoLine) line.prepare(1630*samplesPerMs+1);
        source.prepare(1630*samplesPerMs+1);
        room.prepare();
        reset();
    }

    void set(const Settings& next)
    {
        const auto p=std::clamp(next.program,0,int(programCount)-1);
        if(p!=settings.program || next.power!=settings.power
            || (p==freeze && next.decay[0]<.05f && settings.decay[0]>=.05f))
            reset();
        settings=next;
        settings.program=p;
        const auto& sp=programs[p];
        for(size_t ch=0;ch<2;++ch)
        {
            settings.pre[ch]=std::clamp(settings.pre[ch],0.f,float(ch==0?sp.maxPre:sp.maxB));
            settings.decay[ch]=std::clamp(settings.decay[ch],0.f,sp.maxDecay);
            settings.low[ch]=sp.low?std::clamp(settings.low[ch],-9.f,9.f):0;
            settings.high[ch]=sp.high?std::clamp(settings.high[ch],-9.f,9.f):0;
            colourPole[ch]=coefficient(15500.f*std::exp2(std::min(0.f,settings.high[ch])/3.3f));
        }
        if(sp.shape==diffuse || sp.shape==sustain)
            room.configure(sp,p,settings.decay[0],settings.low[0],settings.high[0]);
        if(sp.shape==gate || sp.shape==reverse || sp.shape==pan)
            configureTaps(sp);
        inGain=dbGain(settings.input); outGain=dbGain(settings.output);
    }

    void reset()
    {
        room.reset(); source.reset();
        for(auto& line:echoLine) line.reset();
        xLow.fill(0); xHigh.fill(0);
        time=0; inputPeak=0;
    }
    float peak() const { return inputPeak; }
    std::array<float,2> process(float sample)
    {
        if(!settings.power) return {};
        const float raw=sample*inGain;
        inputPeak=std::max(std::abs(raw),inputPeak*.99985f);
        const float x=encodeInput(raw);
        source.push(x);
        const auto& sp=programs[settings.program];
        const auto pre=static_cast<size_t>(settings.pre[0]*samplesPerMs);
        std::array<float,2> out {};
        switch(sp.shape)
        {
            case diffuse: out=room.process(source.get(pre)); break;
            case sustain:
                if(settings.decay[0] >= .05f)
                    out=room.process(settings.decay[0]<.15f?x:0);
                break;
            case repeats:
            case straight:
                for(size_t ch=0;ch<2;++ch)
                {
                    if(sp.shape==straight && ch==0) {out[ch]=x;continue;}
                    const size_t control=sp.shape==straight?0:ch;
                    const auto age=static_cast<size_t>(settings.pre[control]*samplesPerMs);
                    const float feedback=std::min(.999f,settings.decay[control]/9.9f);
                    out[ch]=age==0?x:echoLine[ch].get(age-1);
                    float fb=out[ch];
                    if(sp.shape==repeats)
                    {
                        xLow[ch]+=.054f*(fb-xLow[ch]);
                        const float lo=std::clamp(settings.low[ch]*.04f,-.36f,.36f);
                        fb=tone(ch,fb+lo*xLow[ch],settings.high[ch]);
                    }
                    echoLine[ch].push(std::clamp(x+feedback*fb,-1.f,1.f));
                }
                break;
            case ensemble:
                for(int voice=0;voice<5;++voice)
                {
                    // Two incommensurate motions per voice avoid a common repeating sweep.
                    const double t=time/static_cast<double>(clockHz);
                    const float mod=static_cast<float>(std::sin(t*(1.61+.43*voice)+2.3*voice)
                                               +.43*std::sin(t*(.53+.13*voice)-1.8*voice));
                    const float age=std::max(0.f, (2.5f+settings.pre[0]*(voice+1)
                                                +.33f*settings.decay[0]*mod)*samplesPerMs);
                    const float v=source.read(age)*.26f;
                    out[0]+=v*(1.f-voice*.25f); out[1]+=v*(voice*.25f);
                }
                for(size_t ch=0;ch<2;++ch) out[ch]=tone(ch,out[ch],settings.high[0]);
                break;
            case gate:
            case reverse:
            case pan:
                for(const auto& tap:taps)
                {
                    const float v=source.get(tap.age);
                    out[0]+=v*tap.left; out[1]+=v*tap.right;
                }
                for(size_t ch=0;ch<2;++ch) out[ch]=tone(ch,out[ch],settings.high[0]);
                break;
        }
        ++time;
        for(auto& channel:out)
            channel=converter(channel)*outGain;
        return out;
    }
private:
    struct Tap { size_t age; float left,right; };
    void configureTaps(const ProgramSpec& sp)
    {
        const bool discrete=settings.program==nonlin1 || settings.program==reverse1;
        uint32_t seed=7u+217u*static_cast<uint32_t>(settings.program);
        const auto rnd=[&seed]()
        {
            seed=1664525u*seed+1013904223u;
            return float(seed>>8)/16777216.f;
        };
        const float duration=std::max(5.f,(sp.shape==pan?60.f:85.f)*settings.decay[0]*sp.size);
        for(size_t i=0;i<taps.size();++i)
        {
            const float t=(static_cast<float>(i)+rnd())/static_cast<float>(taps.size());
            const float envelope=sp.shape==reverse ? t*t
                                 : t>.84f ? (1-t)*6.25f : 1-.24f*t;
            const float amp=.105f*envelope;
            const float position=sp.shape==pan?t:rnd();
            const float left=amp*(rnd()<.5f?-1.f:1.f)*std::sqrt(1-position);
            const float right=amp*(rnd()<.5f?-1.f:1.f)*std::sqrt(position);
            taps[i]={static_cast<size_t>((settings.pre[0]+duration*t)*samplesPerMs),
                     left,discrete ? (i%9==0 ? 2.4f*right : 0.f) : right};
        }
    }

    float tone(size_t ch, float x, float amount)
    {
        xHigh[ch]+=colourPole[ch]*(x-xHigh[ch]);
        return amount<0?xHigh[ch]:x+(amount/18.f)*(x-xHigh[ch]);
    }
    static float encodeInput(float x)
    {
        x=std::clamp(x,-1.f,1.f);
        const float a=std::abs(x);
        const float scale=a<.125f?8.f:a<.25f?4.f:a<.5f?2.f:1.f;
        return std::clamp(std::round(x*2048*scale),-2048.f,2047.f)/(2048*scale);
    }
    static float converter(float x)
    {
        x=std::clamp(x,-1.f,.99996948f);
        x-=.00050f*x*x*x;
        return std::round(32768.f*x)/32768.f;
    }
    Settings settings;
    Reverb room;
    DelayLine source;
    std::array<DelayLine,2> echoLine;
    std::array<Tap,96> taps {};
    std::array<float,2> xLow {},xHigh {},colourPole {};
    uint64_t time=0;
    float inGain=1,outGain=1,inputPeak=0;
};

class Engine
{
public:
    void prepare(double rate)
    {
        hostRate=rate;
        latency=Resampler::radius+static_cast<int>(std::ceil((Resampler::radius+1)*rate/clockHz));
        input.prepare(rate,clockHz); output.prepare(clockHz,rate);
        dryLeft.prepare(static_cast<size_t>(latency)+1);
        dryRight.prepare(static_cast<size_t>(latency)+1);
        core.prepare(); hostIndex=tickIndex=0;
    }
    void set(const Settings& s) { core.set(s); wet=s.wet; }
    int latencySamples() const { return latency; }
    float peak() const { return core.peak(); }
    std::array<float,2> process(float l,float r)
    {
        const float mono=.5f*(l+r);
        input.push(hostIndex,{mono,mono});
        const double ticks=static_cast<double>(hostIndex)*clockHz/hostRate;
        while(static_cast<double>(tickIndex)<=ticks)
        {
            const auto x=input.get(static_cast<double>(tickIndex)*hostRate/clockHz-Resampler::radius);
            output.push(tickIndex++,core.process(x[0]));
        }
        ++hostIndex;
        const auto y=output.get(ticks-(latency-Resampler::radius)*clockHz/hostRate);
        dryLeft.push(l); dryRight.push(r);
        return {std::lerp(dryLeft.get(latency),y[0],wet),
                std::lerp(dryRight.get(latency),y[1],wet)};
    }
private:
    Resampler input,output;
    DelayLine dryLeft,dryRight;
    Core core;
    int64_t hostIndex=0,tickIndex=0;
    double hostRate=44100;
    int latency=0;
    float wet=1;
};
}
