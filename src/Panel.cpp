#include "Panel.hpp"

namespace rmx
{
namespace
{
constexpr int width=1244, faceHeight=227;
constexpr float pi=std::numbers::pi_v<float>;
constexpr std::array<int,9> controls {397,461,526,593,642,705,753,815,862};
constexpr char keyLabels[]="123A456B789C*0#D";
const auto white=juce::Colour(0xffeee9dd);
const auto red=juce::Colour(0xffff3c2a);
void label(juce::Graphics& g,const juce::String& text,float x,float y,float w,float h,
           float size=12,juce::Colour colour=white)
{
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions("Arial",size,juce::Font::bold)));
    g.drawFittedText(text,juce::Rectangle<int>(int(x-w*.5f),int(y-h*.5f),int(w),int(h)),
                     juce::Justification::centred,2);
}
void lamp(juce::Graphics& g,float x,float y,juce::Colour colour,bool active,float d)
{
    auto box=juce::Rectangle<float>(d,d).withCentre({x,y});
    g.setColour(juce::Colour(0xff100f10));g.fillEllipse(box.expanded(1));
    auto base=active?colour:colour.darker(4.3f);
    g.setGradientFill(juce::ColourGradient(base.brighter(.55f),x-2,y-2,base.darker(.5f),x+4,y+4,false));
    g.fillEllipse(box);
    g.setColour(juce::Colours::white.withAlpha(active?.57f:.14f));
    g.fillEllipse(x-d*.21f,y-d*.28f,d*.32f,d*.2f);
}
void screw(juce::Graphics& g,float x,float y,float diameter)
{
    g.setColour(juce::Colour(0xff414044));
    g.fillEllipse(x-diameter*.5f,y-diameter*.5f,diameter,diameter);
    g.setColour(juce::Colour(0xff171719));
    g.fillEllipse(x-3,y-3,6,6);
    g.setColour(juce::Colour(0xff292a2d));
    g.drawLine(x-2,y+2,x+2,y-2,1);
}
void switchCap(juce::Graphics& g,float x,float cy,bool active)
{
    auto box=juce::Rectangle<float>(45,42).withCentre({x,cy});
    g.setColour(juce::Colours::black);g.fillRoundedRectangle(box.expanded(1.7f).translated(0,2),1.5f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4b4f),x,box.getY(),
                                          juce::Colour(0xff141518),x,box.getBottom(),false));
    g.fillRoundedRectangle(box,1.5f);
    g.setColour(juce::Colour(0xff26272b));g.fillRect(box.withTrimmedTop(18).reduced(4,2));
    lamp(g,x,cy-13,red,active,5.5f);
}
void dial(juce::Graphics& g,float x,float y)
{
    for(int i=0;i<10;++i)
    {
        const float a=i*(2*pi/11.f);
        label(g,juce::String(i),x+32*std::sin(a),y-32*std::cos(a),12,13,11);
    }
}
void box(juce::Graphics& g,float x,float y,float w,float h)
{
    g.setColour(juce::Colour(0xffdddad6));g.drawRect(x,y,w,h,1.65f);
}
void number(juce::Graphics& g,const juce::String& text,float x,float y,int maxDigits)
{
    constexpr std::array<int,10> masks{63,6,91,79,102,109,125,7,127,111};
    const std::array<juce::Rectangle<float>,7> segments {{
        {3,0,11,2},{14,2,2,11},{14,15,2,11},{3,26,11,2},
        {1,15,2,11},{1,2,2,11},{3,13,11,2}
    }};
    int at=0;
    for(auto c:text)
    {
        if(c=='.')
        {
            g.setColour(red);g.fillEllipse(x+at*20-3,y+26,2.5f,2.5f);continue;
        }
        if(at>=maxDigits) break;
        const int mask=c>='0'&&c<='9'?masks[c-'0']:c=='-'?64:c=='+'?64:c=='t'?120:0;
        for(size_t i=0;i<segments.size();++i)
        {
            g.setColour(mask&(1<<i)?red:juce::Colour(0xff251619));
            g.fillRoundedRectangle(segments[i].translated(x+at*20,y),.8f);
        }
        if(c=='+')
        {
            g.setColour(red);g.fillRoundedRectangle(x+at*20+7.5f,y+9,2,10,.7f);
        }
        ++at;
    }
}
}

Panel::Panel(Processor& p):AudioProcessorEditor(p),owner(p)
{
    setWantsKeyboardFocus(true);
    knob(input,119,154);knob(output,218,154);knob(pot,302,54);
    using Attach=juce::AudioProcessorValueTreeState::SliderAttachment;
    attachments[0]=std::make_unique<Attach>(owner.state,params[gainIn].id,input);
    attachments[1]=std::make_unique<Attach>(owner.state,params[gainOut].id,output);
    pot.setRange(0,1,.001);
    pot.setTooltip("Controls the highlighted parameter when POT is lit");
    pot.onValueChange=[this]
    {
        if(!potEnabled || syncPot) return;
        const auto limits=range();
        change(std::lerp(limits[0],limits[1],static_cast<float>(pot.getValue())));
    };
    button("Pot",{278,123,46,52},[this]{potEnabled=!potEnabled && focus<=high;});
    for(size_t i=0;i<7;++i)
    {
        const auto field=static_cast<Focus>(i);
        const std::array<const char*,7> names {"Program","Predelay","Decay time","Decay low","Decay high","Store save","Control recall"};
        button(names[i],{controls[i]-22,124,45,49},[this,field]{focusOn(field);});
    }
    button("Nudge down",{controls[7]-22,124,45,49},[this]{nudge(-1);},true);
    button("Nudge up",{controls[8]-22,124,45,49},[this]{nudge(1);},true);
    for(int i=0;i<16;++i)
    {
        const int x=962+(i%4)*33,y=50+(i/4)*33;
        button(juce::String::charToString(keyLabels[i]),{x-13,y-11,26,22},[this,i]{key(keyLabels[i]);});
    }
    button("Power",{1111,34,43,46},[this]{owner.set(power,owner.get(power)<.5f?1.f:0.f);});
    library.setTextWhenNothingSelected("Factory presets");
    for(size_t i=0;i<presets.size();++i) library.addItem(presets[i].name,int(i+1));
    library.onChange=[this]
    {
        if(library.getSelectedId()>0)
        {
            owner.setCurrentProgram(library.getSelectedId());
            echoSide=0; edited=true; focusOn(prog);
        }
    };
    addAndMakeVisible(library);library.setBounds(12,faceHeight+7,252,25);
    wet.setSliderStyle(juce::Slider::LinearHorizontal);
    wet.setTextBoxStyle(juce::Slider::TextBoxRight,false,45,23);
    wet.setTextValueSuffix("%");
    wet.setTooltip("Wet/dry blend. Use 100% wet on a send.");
    addAndMakeVisible(wet);wet.setBounds(303,faceHeight+7,156,25);
    attachments[2]=std::make_unique<Attach>(owner.state,params[mix].id,wet);
    setSize(width,faceHeight+39);
    startTimerHz(24);
}

Panel::~Panel()
{
    stopTimer();
    for(auto* s:{&input,&output,&pot}) s->setLookAndFeel(nullptr);
}

void Panel::button(const juce::String& name,juce::Rectangle<int> area,std::function<void()> fire,bool repeat)
{
    auto b=std::make_unique<Button>(name);
    b->setTooltip(name);b->setWantsKeyboardFocus(false);b->setBounds(area);
    b->onClick=[this,fire=std::move(fire)]{grabKeyboardFocus();fire();repaint();};
    if(repeat) b->setRepeatSpeed(360,90,45);
    addAndMakeVisible(*b);
    hotspots.push_back(std::move(b));
}

void Panel::knob(juce::Slider& s,int x,int y)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setRotaryParameters(2*pi,2*pi+20*pi/11,true);
    s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    s.setLookAndFeel(&knobs);s.setBounds(x-24,y-24,48,48);addAndMakeVisible(s);
}

void Panel::Knobs::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float p,float begin,float end,juce::Slider&)
{
    const float cx=x+w*.5f,cy=y+h*.5f,r=std::min(w,h)*.46f;
    auto body=juce::Rectangle<float>(r*2,r*2).withCentre({cx,cy});
    g.setColour(juce::Colour(0xff070709));g.fillEllipse(body.expanded(2).translated(2,2.5f));
    for(int i=0;i<96;++i)
    {
        float a=i*2*pi/96.f;
        juce::Path wedge;wedge.addPieSegment(body.getX(),body.getY(),body.getWidth(),body.getHeight(),a,a+2*pi/95.5f,0);
        const auto b=.61f+.2f*std::cos(a*3+.6f)+.16f*std::sin(a*8);
        g.setColour(juce::Colour::fromFloatRGBA(b,b,b*.97f,1));g.fillPath(wedge);
    }
    auto inner=body.reduced(r*.18f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff8f8f3),cx-17,cy-13,
                                          juce::Colour(0xff7c7d87),cx+17,cy+15,false));
    g.fillEllipse(inner);g.setColour(juce::Colour(0xffeeece4));g.drawEllipse(inner,.8f);
    const auto a=begin+p*(end-begin);
    g.setColour(juce::Colour(0xff15151a));
    g.drawLine(cx+std::sin(a)*r*.68f,cy-std::cos(a)*r*.68f,cx+std::sin(a)*r*.98f,cy-std::cos(a)*r*.98f,2);
}

Param Panel::parameter() const
{
    const bool b=owner.currentAlgorithm()==echo && echoSide==1;
    switch(focus)
    {
        case pre:return b?preB:preA;
        case decay:return b?decayB:decayA;
        case low:return b?lowB:lowA;
        case high:return b?highB:highA;
        default:return algorithm;
    }
}

std::array<float,2> Panel::range() const
{
    const auto& sp=programs[static_cast<size_t>(owner.currentAlgorithm())];
    switch(focus)
    {
        case prog:return {1,18};
        case pre:return {0,float(parameter()==preB?sp.maxB:sp.maxPre)};
        case decay:return {0,sp.maxDecay};
        case low:return sp.low?std::array<float,2>{-9,9}:std::array<float,2>{0,0};
        case high:return sp.high?std::array<float,2>{-9,9}:std::array<float,2>{0,0};
        default:return {1,9};
    }
}

void Panel::focusOn(Focus f)
{
    focus=f;pending.clear();error=false;potEnabled=false;
}

void Panel::change(float v)
{
    const auto limits=range();
    if(v<limits[0]||v>limits[1]) {error=true;return;}
    if(focus==prog)
    {
        owner.selectAlgorithm(int(std::round(v))-1);echoSide=0;edited=false;
    }
    else if(focus==save) {owner.save(int(v));focusOn(prog);edited=true;}
    else if(focus==recall)
    {
        if(owner.recall(int(v))) {focusOn(prog);echoSide=0;edited=true;}
        else error=true;
    }
    else {owner.set(parameter(),v);edited=true;}
}

void Panel::nudge(int amount)
{
    error=false;pending.clear();nudgeLamp=amount;nudgeTicks=4;
    const auto limits=range();
    if(focus>=save)
    {
        pending=juce::String(juce::jlimit(1,9,owner.previousSlot()+amount));
        return;
    }
    const auto mode=owner.currentAlgorithm();
    const float step=focus==decay?.1f:focus==pre && mode!=chorus?10.f:1.f;
    change(std::clamp(owner.value(parameter())+amount*step,limits[0],limits[1]));
}

void Panel::key(char c)
{
    if(c=='A'||c=='B')
    {
        if(owner.currentAlgorithm()==echo) {echoSide=c=='B'?1:0;pending.clear();error=false;}
        return;
    }
    if(c=='#')
    {
        error=false;
        if(pending.containsAnyOf("0123456789")) change(static_cast<float>(pending.getDoubleValue()));
        else if(focus>=save) change(static_cast<float>(owner.previousSlot()));
        pending.clear();return;
    }
    if(c=='*')
    {
        if(pending.isEmpty() && (focus==low||focus==high)) pending="-";
        else if(focus==decay && !pending.containsChar('.')) pending+=".";
        return;
    }
    if(c>='0'&&c<='9')
    {
        if(error) {pending.clear();error=false;}
        if(pending.length()<6) pending+=juce::String::charToString(c);
    }
}

bool Panel::keyPressed(const juce::KeyPress& k)
{
    const auto c=juce::CharacterFunctions::toUpperCase(k.getTextCharacter());
    if(k==juce::KeyPress::upKey) nudge(1);
    else if(k==juce::KeyPress::downKey) nudge(-1);
    else if(k==juce::KeyPress::returnKey) key('#');
    else if(k==juce::KeyPress::escapeKey) {pending.clear();error=false;}
    else if(k==juce::KeyPress::backspaceKey) pending=pending.dropLastCharacters(1);
    else if((c>='0'&&c<='9')||(c>='A'&&c<='D')||c=='#'||c=='*'||c=='.'||c=='-')
        key(c=='.'||c=='-'?'*':static_cast<char>(c));
    else return false;
    repaint();return true;
}

void Panel::timerCallback()
{
    if(nudgeTicks>0) --nudgeTicks;
    if(!pot.isMouseButtonDown() && focus<=high)
    {
        const auto limits=range();
        syncPot=true;
        pot.setValue(limits[1]>limits[0]?(owner.value(parameter())-limits[0])/(limits[1]-limits[0]):0,
                     juce::dontSendNotification);
        syncPot=false;
    }
    if(library.getSelectedId()!=owner.getCurrentProgram())
        library.setSelectedId(owner.getCurrentProgram(),juce::dontSendNotification);
    repaint();
}

void Panel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff19191c));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff272629),0,0,
                                          juce::Colour(0xff101013),0,faceHeight,false));
    g.fillRect(0,0,width,faceHeight);
    for(float x:{24.f,1220.f}) for(float y:{16.f,211.f})
    {
        auto hole=juce::Rectangle<float>(32,22).withCentre({x,y});
        g.setColour(juce::Colour(0xff08080b));g.fillRoundedRectangle(hole,10);
        g.setColour(juce::Colour(0xff6a6765));g.drawRoundedRectangle(hole.reduced(.5f),10,.8f);
    }
    for(float x:{68.f,1176.f}) {screw(g,x,23,24);screw(g,x,204,24);}
    box(g,85,4,171,86);box(g,85,95,171,98);box(g,262,4,81,189);
    box(g,349,4,567,189);box(g,922,4,177,189);box(g,1105,4,52,189);
    box(g,85,199,1072,22);
    label(g,"model rmx 16 : digital reverberation system",621,210,940,21,16);
    screw(g,98,106,13);screw(g,271,180,15);screw(g,359,23,16);
    screw(g,901,23,16);screw(g,901,173,16);screw(g,1142,112,13);
    dial(g,119,154);dial(g,218,154);dial(g,302,54);
    label(g,"in",123,108,37,14);label(g,"out",220,108,41,14);
    label(g,"o/flo",169,114,42,14);label(g,"pot",301,113,46,14);
    const bool on=owner.get(power)>.5f;
    const auto peak=owner.peak.load(std::memory_order_relaxed);
    lamp(g,169,128,red,on&&peak>=1.f,12);
    lamp(g,169,145,red,on&&peak>.79f,12);
    lamp(g,169,162,juce::Colour(0xffffa728),on&&peak>.25f,12);
    lamp(g,169,179,juce::Colour(0xff39ba57),on&&peak>.025f,12);
    switchCap(g,302,149,on&&potEnabled);
    const std::array<const char*,9> captions {"program","predelay","decay time","low","high","save","recall","down","up"};
    label(g,"decay filter",618,105,99,13);label(g,"store control",730,105,100,13);
    label(g,"nudge",839,105,81,13);
    for(size_t i=0;i<controls.size();++i)
    {
        label(g,captions[i],float(controls[i]),i<3?113.f:119.f,69,17);
        const bool active=i<7?static_cast<int>(i)==int(focus):nudgeTicks>0&&nudgeLamp==(i==7?-1:1);
        switchCap(g,float(controls[i]),150,on&&active);
    }

    auto bezel=juce::Rectangle<float>(521,49).withPosition(371,29);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff100b0c),371,29,
                                          juce::Colour(0xff221418),371,78,false));
    g.fillRoundedRectangle(bezel,1.5f);
    if(on)
    {
        const auto mode=owner.currentAlgorithm();
        const bool b=mode==echo&&echoSide==1;
        const auto& spec=programs[static_cast<size_t>(mode)];
        const auto entry=[this](Focus f,juce::String s)
        {return focus==f && pending.isNotEmpty()?pending:s;};
        number(g,focus>=save?entry(focus,juce::String(owner.previousSlot()))
                            :entry(prog,edited?"t":juce::String(mode+1)),378,40,2);
        number(g,entry(pre,juce::String(int(owner.value(b?preB:preA)))),425,40,4);
        number(g,entry(decay,juce::String(owner.value(b?decayB:decayA),1)),513,40,2);
        number(g,spec.low?entry(low,juce::String::formatted("%+d",int(owner.value(b?lowB:lowA)))):"",568,40,2);
        number(g,spec.high?entry(high,juce::String::formatted("%+d",int(owner.value(b?highB:highA)))):"",621,40,2);
        g.setFont(juce::Font(juce::FontOptions("Courier New",23,juce::Font::bold)));
        g.setColour(red);
        const juce::String name=mode==echo?(b?"ECHO B":"ECHO A"):spec.name;
        g.drawText(error?"ERROR":name,680,37,202,35,juce::Justification::centredLeft);
    }

    g.setColour(juce::Colour(0xff323238));g.fillRoundedRectangle(931,15,159,172,7);
    g.setColour(juce::Colour(0xff1d1e24));g.fillRoundedRectangle(935,19,151,165,5);
    for(int i=0;i<16;++i)
    {
        const auto cx=962.f+(i%4)*33.f,cy=50.f+(i/4)*33.f;
        auto keybox=juce::Rectangle<float>(22,18).withCentre({cx,cy});
        g.setColour(juce::Colours::black);g.fillRoundedRectangle(keybox.translated(1,2),2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfffbf0cb),cx,cy-8,
                                              juce::Colour(0xffc6b793),cx,cy+10,false));
        g.fillRoundedRectangle(keybox,1.3f);
        label(g,juce::String::charToString(keyLabels[i]),cx,cy,20,16,11,juce::Colour(0xff191515));
    }
    label(g,"on",1132,33,32,13);label(g,"off",1132,77,32,13);
    lamp(g,1131,55,juce::Colour(0xff77716d),true,26);
    g.setColour(juce::Colour(0xffc2bbb1));
    g.drawLine(1131,55,1138,on?47.f:64.f,5.f);
    lamp(g,1132,152,red,on,12);
    label(g,"power",1131,183,50,13);
    label(g,"Mix",282,faceHeight+19,38,18,12);
}
}
