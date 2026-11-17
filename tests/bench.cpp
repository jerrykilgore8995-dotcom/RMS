#include "../src/Engine.hpp"
#include <cstdio>
#include <string_view>

using namespace rmx;
namespace
{
int failures=0;
void check(bool ok, const char* test)
{
    if(!ok) { std::fprintf(stderr,"Failed: %s\n",test); ++failures; }
}

void programGrid()
{
    Core core;
    core.prepare();
    for(int p=0;p<programCount;++p)
    {
        const auto& sp=programs[static_cast<size_t>(p)];
        for(float u:{0.f,.5f,1.f})
        {
            Settings s;
            s.program=p; s.pre={std::round(sp.maxPre*u),std::round(sp.maxB*u)};
            s.decay={sp.maxDecay*u,sp.maxDecay*u};
            s.low={18*u-9,9-18*u}; s.high=s.low;
            if(p==freeze) s.decay[0]=.1f;
            core.reset();core.set(s);
            double sum=0;float peak=0;
            for(int i=0;i<110000;++i)
            {
                auto y=core.process(i<2500&&i%79==0?.7f:0);
                for(auto z:y) {peak=std::max(peak,std::abs(z));sum+=z*z;}
            }
            std::printf("program,%d,%s,%.2f,%.6f,%.9f\n",p+1,sp.name,u,peak,sum);
            check(std::isfinite(sum)&&peak>.0001f,"program grid");
        }
    }
}

void delays()
{
    Core core;core.prepare();
    for(auto rates:{std::array{40,119},std::array{1200,400}})
    {
        Settings s;
        s.program=echo;s.pre={float(rates[0]),float(rates[1])};s.decay={0,0};
        core.reset();core.set(s);
        std::array<int,2> where {-1,-1};
        for(int i=0;i<48030;++i)
        {
            const auto y=core.process(i==0?.6f:0);
            for(size_t ch=0;ch<2;++ch)
                if(std::abs(y[ch])>.1f && where[ch]<0) where[ch]=i;
        }
        for(size_t ch=0;ch<2;++ch)
        {
            std::printf("echo,%zu,%d,%d\n",ch,rates[ch],where[ch]);
            check(where[ch]==rates[ch]*samplesPerMs,"integer echo address");
        }
    }
    for(double sr:{44100,48000,96000})
    {
        Engine e;e.prepare(sr);
        Settings s;s.program=delay;s.pre[0]=1630;s.decay={0,0};e.set(s);
        std::array<float,2> peak {};
        std::array<int,2> where {-1,-1};
        for(int i=0;i<int(1.7*sr);++i)
        {
            auto y=e.process(i==0?.5f:0,i==0?.5f:0);
            for(size_t ch=0;ch<2;++ch)
                if(std::abs(y[ch])>peak[ch]) {peak[ch]=std::abs(y[ch]);where[ch]=i;}
        }
        const auto a=where[0]-e.latencySamples(), b=where[1]-e.latencySamples();
        std::printf("delay,%.0f,%d,%d,%d\n",sr,e.latencySamples(),a,b);
        check(std::abs(a)<=1 && std::abs(b-sr*1.63)<=1,"1630 ms and dry delay output");
    }
}

void tones()
{
    const double sr=48000;
    const int total=48000;
    std::array<double,7> frequency {20,100,1000,5000,10000,15000,18000};
    for(auto hz:frequency)
    {
        Engine e;e.prepare(sr);
        Settings s;s.program=delay;s.decay={0,0};e.set(s);
        double ms=0,dc=0,real=0,imag=0;
        std::array<double,19> harmonicReal {},harmonicImag {};
        for(int i=0;i<total+2048;++i)
        {
            const double angle=2*std::numbers::pi*hz*i/sr;
            const auto x=static_cast<float>(.96*std::sin(angle));
            const double shifted=2*std::numbers::pi*hz*(i-e.latencySamples())/sr;
            const double y=e.process(x,x)[0];
            if(i>=2048)
            {
                ms+=y*y/total;dc+=y/total;
                real+=y*std::sin(shifted)*2/total;
                imag+=y*std::cos(shifted)*2/total;
                for(size_t h=0;h<harmonicReal.size() && (h+2)*hz<sr*.5;++h)
                {
                    harmonicReal[h]+=y*std::sin(shifted*(h+2))*2/total;
                    harmonicImag[h]+=y*std::cos(shifted*(h+2))*2/total;
                }
            }
        }
        const auto mag=std::hypot(real,imag);
        const auto thdn=100*std::sqrt(std::max(0.0,ms-dc*dc-.5*mag*mag))/(mag/std::sqrt(2.0));
        const auto gain=20*std::log10(mag/.96);
        double harmonicMS=0;
        for(size_t h=0;h<harmonicReal.size() && (h+2)*hz<sr*.5;++h)
            harmonicMS+=harmonicReal[h]*harmonicReal[h]+harmonicImag[h]*harmonicImag[h];
        const double thd=100*std::sqrt(harmonicMS)/mag;
        std::printf("converter,%.0f,%.5f,%.5f,%.6f,%.6f",hz,gain,std::atan2(imag,real)*180/std::numbers::pi,thd,thdn);
        for(size_t h=0;h<harmonicReal.size() && (h+2)*hz<sr*.5;++h)
            std::printf(",%.3f",20*std::log10(std::max(1e-20,std::hypot(harmonicReal[h],harmonicImag[h])/mag)));
        std::putchar('\n');
        if(hz==1000) check(thd<.03,"delay path nominal full output THD");
        if(hz==18000) check(gain>=-3 && gain<=.01,"18 kHz bandwidth");
    }
}

void freezeStates()
{
    Core core;core.prepare();
    Settings s;s.program=freeze;s.decay={.1f,0};core.set(s);
    for(int i=0;i<clockHz;++i) core.process(i==0?.5f:0);
    s.decay[0]=.2f;core.set(s);
    double held=0,clear=0;
    for(int i=0;i<clockHz;++i) for(auto y:core.process(0)) held+=y*y;
    s.decay[0]=0;core.set(s);
    for(int i=0;i<clockHz;++i) for(auto y:core.process(0)) clear+=y*y;
    std::printf("freeze,%.9f,%.9f\n",held,clear);
    check(held>0 && clear==0,"hold and clear freeze");
}

void presetGrid()
{
    Core core;core.prepare();
    for(auto& p:presets)
    {
        Settings s;
        s.program=p.program;s.pre={p.pre,p.preB};s.decay={p.decay,p.decayB};
        s.low={p.low,p.low};s.high={p.high,p.high};
        core.reset();core.set(s);
        std::array<double,2> ms {};
        float peak=0;
        double cross=0;
        for(int i=0;i<clockHz*2;++i)
        {
            const auto x=i<clockHz ? static_cast<float>(.25*std::sin(.038*i)+.14*std::sin(.147*i)) : 0.f;
            const auto y=core.process(x);
            for(size_t ch=0;ch<2;++ch) {ms[ch]+=y[ch]*y[ch];peak=std::max(peak,std::abs(y[ch]));}
            cross+=y[0]*y[1];
        }
        std::printf("preset,%s,%.6f,%.8f,%.6f\n",p.name,peak,ms[0]+ms[1],cross/std::sqrt(ms[0]*ms[1]));
        check(std::isfinite(peak)&&peak>.0001f&&ms[0]>0&&ms[1]>0,"preset stereo signal");
    }
}
}

int main(int argc,char** argv)
{
    if(argc>1 && std::string_view(argv[1])=="ambience")
    {
        Engine engine; engine.prepare(48000);
        Settings s;s.decay={2.1f,0};engine.set(s);
        for(int i=0;i<48000*5+engine.latencySamples();++i)
        {
            const auto y=engine.process(i==0?.8f:0,i==0?.8f:0);
            if(i>=engine.latencySamples()) std::printf("%.8f,%.8f\n",y[0],y[1]);
        }
        return 0;
    }
    programGrid();
    delays();
    tones();
    freezeStates();
    presetGrid();
    std::printf("failures,%d\n",failures);
    return failures!=0;
}
