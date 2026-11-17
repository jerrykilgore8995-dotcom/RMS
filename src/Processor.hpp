#pragma once
#include "Engine.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace rmx
{
enum Param { algorithm, preA, decayA, lowA, highA, preB, decayB, lowB, highB, gainIn, gainOut, mix, power, paramCount };
struct ParamSpec { const char* id; const char* label; float min,max,step,initial; };
inline constexpr std::array<ParamSpec,paramCount> params {{
    {"program","Algorithm",1,18,1,1},
    {"pre","Pre-delay (ms)",0,1630,1,0}, {"decay","Decay",0,9.9f,.1f,2},
    {"low","Decay Low",-9,9,1,0}, {"high","Decay High",-9,9,1,0},
    {"preB","Echo B Pre-delay (ms)",0,400,1,0}, {"decayB","Echo B Feedback",0,9.9f,.1f,0},
    {"lowB","Echo B Decay Low",-9,9,1,0}, {"highB","Echo B Decay High",-9,9,1,0},
    {"input","Input Gain (dB)",-24,12,.1f,0}, {"output","Output Gain (dB)",-24,12,.1f,0},
    {"wet","Wet (%)",0,100,1,100}, {"power","Power",0,1,1,1}
}};

class Processor final : public juce::AudioProcessor
{
public:
    Processor();
    juce::AudioProcessorValueTreeState state;
    float get(Param p) const { return values[p]->load(std::memory_order_relaxed); }
    void set(Param,float);
    int currentAlgorithm() const { return static_cast<int>(get(algorithm))-1; }
    float value(Param) const;
    void selectAlgorithm(int);
    void save(int);
    bool recall(int);
    int previousSlot() const { return lastSlot.load(); }
    std::atomic<float> peak {0};
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RMX16"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 120; }
    int getNumPrograms() override { return static_cast<int>(presets.size())+1; }
    int getCurrentProgram() override { return selected.load(); }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
private:
    using Snapshot=std::array<float,highB+1>;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    std::array<std::atomic<float>*,paramCount> values {};
    std::array<Snapshot,9> stores {};
    juce::CriticalSection lock;
    std::atomic<int> selected {0},lastSlot {1};
    Engine engine;
    bool prepared=false;
};
}
