#pragma once
#include "Processor.hpp"

namespace rmx
{
class Panel final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Panel(Processor&);
    ~Panel() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    enum Focus { prog, pre, decay, low, high, save, recall };
    struct Button : juce::Button
    {
        explicit Button(const juce::String& name):juce::Button(name){}
        void paintButton(juce::Graphics&,bool,bool) override {}
    };
    struct Knobs : juce::LookAndFeel_V4
    {
        void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    };
    void button(const juce::String&,juce::Rectangle<int>,std::function<void()>,bool repeat=false);
    void knob(juce::Slider&,int,int);
    void focusOn(Focus);
    void key(char);
    void nudge(int);
    void change(float);
    void timerCallback() override;
    Param parameter() const;
    std::array<float,2> range() const;
    Processor& owner;
    Knobs knobs;
    juce::Slider input,output,pot,wet;
    juce::ComboBox library;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,3> attachments;
    std::vector<std::unique_ptr<Button>> hotspots;
    juce::String pending;
    Focus focus=prog;
    int echoSide=0, nudgeLamp=0, nudgeTicks=0;
    bool potEnabled=false,error=false,edited=false,syncPot=false;
};
}
