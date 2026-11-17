#include "Processor.hpp"
#include "Panel.hpp"

namespace rmx
{
juce::AudioProcessorValueTreeState::ParameterLayout Processor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout list;
    for(const auto& s:params)
    {
        const juce::ParameterID id(s.id,1);
        if(s.max==1)
            list.add(std::make_unique<juce::AudioParameterBool>(id,s.label,s.initial>0));
        else if(s.step==1)
            list.add(std::make_unique<juce::AudioParameterInt>(id,s.label,int(s.min),int(s.max),int(s.initial)));
        else
            list.add(std::make_unique<juce::AudioParameterFloat>(id,s.label,
                     juce::NormalisableRange<float>(s.min,s.max,s.step),s.initial));
    }
    return list;
}

Processor::Processor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                      .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      state(*this,nullptr,"RMX16",layout())
{
    for(size_t i=0;i<params.size();++i)
        values[i]=state.getRawParameterValue(params[i].id);
}

void Processor::set(Param p,float v)
{
    auto* param=state.getParameter(params[p].id);
    const auto normal=param->convertTo0to1(v);
    if(param->getValue()==normal) return;
    param->beginChangeGesture();
    param->setValueNotifyingHost(normal);
    param->endChangeGesture();
}

float Processor::value(Param p) const
{
    const auto& sp=programs[static_cast<size_t>(currentAlgorithm())];
    switch(p)
    {
        case preA: return std::min(get(p),float(sp.maxPre));
        case preB: return std::min(get(p),float(sp.maxB));
        case decayA: case decayB: return std::min(get(p),sp.maxDecay);
        case lowA: case lowB: return sp.low?get(p):0;
        case highA: case highB: return sp.high?get(p):0;
        default: return get(p);
    }
}

void Processor::selectAlgorithm(int p)
{
    if(p<0 || p>=programCount || p==currentAlgorithm()) return;
    set(algorithm,float(p+1));
    set(preA,0); set(preB,0); set(decayB,0);
    set(decayA,programs[static_cast<size_t>(p)].shape==diffuse
             ? std::min(2.f,programs[static_cast<size_t>(p)].maxDecay):0);
    set(lowA,0); set(lowB,0); set(highA,0); set(highB,0);
    selected.store(0);
}

void Processor::save(int n)
{
    if(n<1 || n>9) return;
    const juce::ScopedLock guard(lock);
    auto& target=stores[static_cast<size_t>(n-1)];
    for(size_t i=0;i<target.size();++i) target[i]=value(static_cast<Param>(i));
    lastSlot.store(n);
}

bool Processor::recall(int n)
{
    if(n<1 || n>9) return false;
    Snapshot source;
    {
        const juce::ScopedLock guard(lock);
        source=stores[static_cast<size_t>(n-1)];
    }
    if(source[0]<1) return false;
    for(size_t i=0;i<source.size();++i) set(static_cast<Param>(i),source[i]);
    lastSlot.store(n);
    selected.store(0);
    return true;
}

void Processor::prepareToPlay(double sr,int)
{
    engine.prepare(sr);
    setLatencySamples(engine.latencySamples());
    prepared=true;
}

bool Processor::isBusesLayoutSupported(const BusesLayout& buses) const
{
    const auto input=buses.getMainInputChannelSet(),output=buses.getMainOutputChannelSet();
    return (input==juce::AudioChannelSet::mono() || input==juce::AudioChannelSet::stereo())
        && (output==juce::AudioChannelSet::mono() || output==juce::AudioChannelSet::stereo());
}

void Processor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&)
{
    if(!prepared || audio.getNumChannels()==0) return;
    juce::ScopedNoDenormals noDenormals;
    Settings s;
    s.program=currentAlgorithm();
    s.pre={get(preA),get(preB)};
    s.decay={get(decayA),get(decayB)};
    s.low={get(lowA),get(lowB)};
    s.high={get(highA),get(highB)};
    s.input=get(gainIn);s.output=get(gainOut);s.wet=get(mix)*.01f;s.power=get(power)>.5f;
    engine.set(s);
    auto* left=audio.getWritePointer(0);
    auto* right=audio.getNumChannels()>1?audio.getWritePointer(1):nullptr;
    const bool stereoIn=getTotalNumInputChannels()==2 && right;
    const bool stereoOut=getTotalNumOutputChannels()==2 && right;
    for(int i=0;i<audio.getNumSamples();++i)
    {
        const float l=std::isfinite(left[i])?left[i]:0;
        const float r=stereoIn?(std::isfinite(right[i])?right[i]:0):l;
        const auto result=engine.process(l,r);
        left[i]=stereoOut?result[0]:.5f*(result[0]+result[1]);
        if(stereoOut) right[i]=result[1];
    }
    peak.store(engine.peak(),std::memory_order_relaxed);
}

void Processor::setCurrentProgram(int i)
{
    if(i<0 || i>=getNumPrograms()) return;
    if(i==0)
    {
        for(int p=algorithm;p<=highB;++p) set(static_cast<Param>(p),params[p].initial);
    }
    else
    {
        const auto& pr=presets[static_cast<size_t>(i-1)];
        set(algorithm,float(pr.program+1));
        set(preA,pr.pre); set(decayA,pr.decay); set(lowA,pr.low); set(highA,pr.high);
        set(preB,pr.preB); set(decayB,pr.decayB); set(lowB,pr.low); set(highB,pr.high);
    }
    selected.store(i);
}

const juce::String Processor::getProgramName(int i)
{
    return i>0 && i<getNumPrograms()?juce::String(presets[static_cast<size_t>(i-1)].name):"Init";
}

void Processor::getStateInformation(juce::MemoryBlock& dest)
{
    auto snapshot=state.copyState();
    snapshot.setProperty("preset",selected.load(),nullptr);
    snapshot.setProperty("lastSlot",lastSlot.load(),nullptr);
    {
        const juce::ScopedLock guard(lock);
        for(size_t n=0;n<stores.size();++n)
            for(size_t p=0;p<stores[n].size();++p)
                snapshot.setProperty("slot"+juce::String(n)+"_"+juce::String(p),stores[n][p],nullptr);
    }
    juce::MemoryOutputStream stream(dest,false);
    snapshot.writeToStream(stream);
}

void Processor::setStateInformation(const void* bytes,int size)
{
    if(size<=0) return;
    auto data=juce::ValueTree::readFromData(bytes,static_cast<size_t>(size));
    if(!data.hasType("RMX16")) return;
    {
        const juce::ScopedLock guard(lock);
        for(size_t n=0;n<stores.size();++n)
            for(size_t p=0;p<stores[n].size();++p)
            {
                const float v=static_cast<float>(data.getProperty("slot"+juce::String(n)+"_"+juce::String(p),0));
                stores[n][p]=std::isfinite(v)?std::clamp(v,p==algorithm?0.f:params[p].min,params[p].max):0.f;
            }
    }
    lastSlot.store(juce::jlimit(1,9,int(data.getProperty("lastSlot",1))));
    selected.store(juce::jlimit(0,getNumPrograms()-1,int(data.getProperty("preset",0))));
    state.replaceState(data);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Panel(*this); }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new rmx::Processor; }
