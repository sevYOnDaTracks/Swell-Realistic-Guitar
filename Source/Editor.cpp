#include "Processor.h"
#include <BinaryData.h>
namespace guitar {
using namespace juce;
namespace {
const Colour black(0xff0e0d0c), slate(0xff1b1917), ink(0xffebe7df), muted(0xffb6afa4),
             gold(0xffd9b371), amber(0xffb68449), rule(0xff484139), sage(0xffe6bd77), night(0xff121110);
String u(const char* s){return String::fromUTF8(s);}
// Swell brand type: Geist for the interface, Unbounded for the wordmark (as on swell-audio.fr).
Typeface::Ptr brandFace(int which){
    static Typeface::Ptr faces[4]={
        Typeface::createSystemTypefaceFor(BinaryData::geist_regular_ttf,BinaryData::geist_regular_ttfSize),
        Typeface::createSystemTypefaceFor(BinaryData::geist_semibold_ttf,BinaryData::geist_semibold_ttfSize),
        Typeface::createSystemTypefaceFor(BinaryData::unbounded_medium_ttf,BinaryData::unbounded_medium_ttfSize),
        Typeface::createSystemTypefaceFor(BinaryData::unbounded_extrabold_ttf,BinaryData::unbounded_extrabold_ttfSize)};
    return faces[which];
}
Font face(float size,bool bold=false){return Font(FontOptions(brandFace(bold?1:0)).withHeight(size));}
Font display(float size,bool heavy){return Font(FontOptions(brandFace(heavy?3:2)).withHeight(size));}
void word(Graphics& g,String s,Rectangle<int> r,float size,Colour c,bool bold=false,Justification j=Justification::centredLeft){
    g.setFont(face(size,bold));g.setColour(c);g.drawText(s,r,j,true);
}
void smallCaps(Graphics& g,String s,Rectangle<int> b,Colour c){
    auto f=face(10,true);f.setExtraKerningFactor(.16f);g.setColour(c);g.setFont(f);g.drawText(s,b,Justification::centredLeft);
}
void panel(Graphics& g,Rectangle<float> b,float radius=7.f){
    g.setGradientFill(ColourGradient(Colour(0xff211e1a),b.getTopLeft(),Colour(0xff11100f),b.getBottomRight(),false));
    g.fillRoundedRectangle(b,radius);g.setColour(rule.withAlpha(.8f));g.drawRoundedRectangle(b.reduced(.5f),radius,1);
}
void line(Graphics& g,float x,float y,float xx,float yy,float w=1.35f){g.drawLine(x,y,xx,yy,w);}
void glyph(Graphics& g,int kind,float x,float y,float scale,Colour colour){
    Graphics::ScopedSaveState ss(g);g.addTransform(AffineTransform::scale(scale).translated(x,y));g.setColour(colour);
    Path p;
    switch(kind){
        case 0: // amplifier
            g.drawRoundedRectangle(1,3,19,14,2,1.4f);line(g,2,8,19,8);for(int i=0;i<3;++i)g.fillEllipse(4+i*4,5,1.5f,1.5f);break;
        case 1: p.startNewSubPath(0,10);p.lineTo(5,10);p.lineTo(8,3);p.lineTo(11,16);p.lineTo(14,7);p.lineTo(17,10);p.lineTo(21,10);break;
        case 2: g.drawRoundedRectangle(2,2,17,16,2,1.4f);for(int i=0;i<3;++i){float y1=4.f+(i%2)*3;line(g,6+i*4,y1,6+i*4,15);line(g,4+i*4,y1+3,8+i*4,y1+3);}break;
        case 3: for(int j=0;j<3;++j){p.startNewSubPath(0,5+j*4);p.cubicTo(8,0+j*4,11,10+j*4,21,5+j*4);}break;
        case 4: for(int i=0;i<3;++i){p.startNewSubPath(3+i*6,5-i);p.quadraticTo(9+i*6,10,3+i*6,15+i);}break;
        case 5: p.startNewSubPath(10,1);p.lineTo(19,5);p.lineTo(19,14);p.lineTo(10,19);p.lineTo(1,14);p.lineTo(1,5);p.closeSubPath();p.startNewSubPath(1,5);p.lineTo(10,10);p.lineTo(19,5);p.startNewSubPath(10,10);p.lineTo(10,19);break;
        case 6: p.startNewSubPath(1,5);p.lineTo(7,5);p.lineTo(9,7);p.lineTo(20,7);p.lineTo(20,16);p.lineTo(1,16);p.closeSubPath();break;
        case 7: g.drawRoundedRectangle(1,4,19,12,1.5f,1.2f);for(int i=0;i<5;++i)line(g,4+i*3,5,4+i*3,i%3==2?15:11,1);break;
        case 8: p.startNewSubPath(2,17);p.lineTo(11,5);p.lineTo(16,3);p.lineTo(18,4);p.lineTo(14,9);p.lineTo(4,18);p.closeSubPath();break;
        case 9: for(int i=0;i<3;++i){line(g,1,4+i*5,19,4+i*5);g.fillEllipse(4+(i%2)*6,2+i*5,4,4);}break;
    }
    g.strokePath(p,PathStrokeType(1.35f,PathStrokeType::curved,PathStrokeType::rounded));
}
class Skin final:public LookAndFeel_V4{
public:
    Skin(){
        setColour(ComboBox::backgroundColourId,slate);setColour(ComboBox::textColourId,ink);setColour(ComboBox::outlineColourId,rule);setColour(ComboBox::arrowColourId,gold);
        setColour(PopupMenu::backgroundColourId,slate);setColour(PopupMenu::textColourId,ink);
        setColour(PopupMenu::highlightedBackgroundColourId,Colour(0xff5e472d));setColour(PopupMenu::highlightedTextColourId,ink);
        setColour(TooltipWindow::backgroundColourId,slate);setColour(TooltipWindow::textColourId,ink);
        setColour(Slider::trackColourId,amber);setColour(Slider::thumbColourId,gold);setColour(Slider::backgroundColourId,rule);
    }
    Font getComboBoxFont(ComboBox&)override{return face(13);}
    Typeface::Ptr getTypefaceForFont(const Font& f)override{return brandFace(f.isBold()?1:0);}
    Font getPopupMenuFont()override{return face(14);}
    void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float v,float a,float b,Slider& s)override{
        auto c=Point<float>(x+w*.5f,y+h*.5f);float r=std::min(w,h)*.43f;
        auto disk=Rectangle<float>(2*r,2*r).withCentre(c);
        g.setColour(black.withAlpha(.85f));g.fillEllipse(disk.translated(0,2).expanded(2));
        auto ring=[&](float from,float to,Colour col,float width){Path p;p.addCentredArc(c.x,c.y,r+1,r+1,0,from,to,true);g.setColour(col);g.strokePath(p,PathStrokeType(width,PathStrokeType::curved,PathStrokeType::rounded));};
        ring(a,b,Colour(0xff5d5850),2.5f);if(v>.002f)ring(a,a+(b-a)*v,s.isEnabled()?gold:rule,2.5f);
        g.setGradientFill(ColourGradient(Colour(0xffa39a87),disk.getTopLeft(),Colour(0xff161412),disk.getBottomRight(),false));g.fillEllipse(disk.reduced(2));
        g.setGradientFill(ColourGradient(Colour(0xff3b3832),disk.getTopLeft(),Colour(0xff11100f),disk.getBottomRight(),false));g.fillEllipse(disk.reduced(4));
        float angle=a+(b-a)*v;g.setColour(ink);g.drawLine({c.getPointOnCircumference(r-8,angle),c.getPointOnCircumference(r-4.5f,angle)},1.8f);
    }
};
class Cell final:public Button{
public:
    enum Style{library,articulation,fx,small,icon,arrow};
    Cell(String label,Style style,int kind=-1):Button(label),style(style),kind(kind){setMouseCursor(MouseCursor::PointingHandCursor);}
    void paintButton(Graphics& g,bool hover,bool down)override{
        auto b=getLocalBounds().toFloat().reduced(.6f);bool active=getToggleState();
        if(style==icon){if(hover){g.setColour(rule.withAlpha(.5f));g.fillRoundedRectangle(b,5);}glyph(g,kind,b.getCentreX()-10,b.getCentreY()-10,1.f,hover?gold:muted);return;}
        if(style==arrow){float cx=b.getCentreX(),cy=b.getCentreY();float side=kind<0?-1.f:1.f;g.setColour(hover?gold:muted);Path p;p.startNewSubPath(cx-3*side,cy-5);p.lineTo(cx+2*side,cy);p.lineTo(cx-3*side,cy+5);g.strokePath(p,PathStrokeType(1.8f));return;}
        if(active){
            g.setGradientFill(ColourGradient(Colour(style==fx?0xff3c3023:0xff84653b),b.getTopLeft(),Colour(style==fx?0xff201c17:0xff443525),b.getBottomRight(),false));
            g.fillRoundedRectangle(b,4);g.setColour(amber.withAlpha(.8f));g.drawRoundedRectangle(b,4,.8f);
        }else if(style!=library || hover){
            g.setColour(hover||down?Colour(0xff34302a):Colour(0xff1c1b19));g.fillRoundedRectangle(b,4);
            g.setColour(rule.withAlpha(.8f));g.drawRoundedRectangle(b,4,.8f);
        }
        if(style==library){
            word(g,getName().upToFirstOccurrenceOf("|",false,false),{10,0,25,getHeight()},11,active?gold:muted,true);
            word(g,getName().fromFirstOccurrenceOf("|",false,false),{40,0,getWidth()-49,getHeight()},13,active?ink:muted);
            if(active){g.setColour(gold);g.fillEllipse((float)getWidth()-10,getHeight()*.5f-1.6f,3.2f,3.2f);}
        }else if(style==fx){
            glyph(g,kind,b.getCentreX()-10,b.getY()+11,1.f,active?gold:muted);
            word(g,getName(),{0,31,getWidth(),20},13,active?ink:muted,false,Justification::centred);
            g.setColour(active?gold:rule);g.fillEllipse(b.getCentreX()-2,b.getBottom()-8,4,4);
        }else word(g,getName(),getLocalBounds(),style==articulation?11.8f:13,active?ink:muted,false,Justification::centred);
    }
private:Style style;int kind;
};
class Keys final:public MidiKeyboardComponent{
public:
    explicit Keys(MidiKeyboardState& s):MidiKeyboardComponent(s,horizontalKeyboard){
        setAvailableRange(36,96);setLowestVisibleKey(36);setScrollButtonsVisible(false);setBlackNoteLengthProportion(.64f);
        setBlackNoteWidthProportion(.64f);setVelocity(.85f,false);setWantsKeyboardFocus(false);
    }
    // Key-switch zones, coloured like the Efimov keyboard: green = playing position, red = articulations.
    static Colour zone(int note){return note>=88?Colour(0xff9fd08a):note<40?Colour(0xffd99a8c):Colour();}
    void drawWhiteNote(int note,Graphics& g,Rectangle<float> b,bool down,bool over,Colour,Colour)override{
        g.setColour(black);g.fillRect(b);b=b.reduced(.65f,0);
        Colour hi=down?Colour(0xffc09b62):Colour(0xffeae6de),lo=down?Colour(0xff997446):Colour(0xffbdb8ad);
        if(!down && !zone(note).isTransparent()){hi=hi.interpolatedWith(zone(note),.55f);lo=lo.interpolatedWith(zone(note),.55f);}
        if(over)hi=hi.brighter(.1f);
        g.setGradientFill(ColourGradient(lo,b.getTopLeft(),hi,b.getBottomLeft(),false));g.fillRoundedRectangle(b,1.5f);
        g.setColour(Colours::black.withAlpha(.18f));g.fillRect(b.removeFromBottom(3));
    }
    void drawBlackNote(int note,Graphics& g,Rectangle<float> b,bool down,bool over,Colour)override{
        g.setColour(Colours::black.withAlpha(.45f));g.fillRoundedRectangle(b.translated(1.6f,2.5f),2);
        g.setColour(Colour(0xff090909));g.fillRoundedRectangle(b,2);
        auto top=b.reduced(1.7f,.5f).withTrimmedBottom(4);
        Colour base=Colour(0xff171615),shine=over?rule:Colour(0xff393733);
        if(!zone(note).isTransparent()){base=base.interpolatedWith(zone(note),.35f);shine=shine.interpolatedWith(zone(note),.45f);}
        g.setGradientFill(ColourGradient(down?amber:base,top.getTopLeft(),shine,top.getBottomRight(),false));g.fillRoundedRectangle(top,1);
        g.setColour(Colour(0xff59544c));g.drawHorizontalLine((int)top.getBottom(),top.getX()+1,top.getRight()-1);
    }
};
class Wheel final:public Slider{
public:
    explicit Wheel(bool centre):spring(centre){setSliderStyle(LinearVertical);setTextBoxStyle(NoTextBox,false,0,0);}
    void mouseUp(const MouseEvent& e)override{if(spring)setValue(0,sendNotificationSync);Slider::mouseUp(e);}
    void paint(Graphics& g)override{
        auto b=getLocalBounds().toFloat().reduced(3,1);g.setColour(Colours::black);g.fillRoundedRectangle(b,4);
        auto metal=b.reduced(3,2);ColourGradient grad(Colour(0xff272622),metal.getTopLeft(),Colour(0xff141310),metal.getBottomLeft(),false);
        grad.addColour(.4,Colour(0xff625e55));grad.addColour(.56,Colour(0xff444039));g.setGradientFill(grad);g.fillRoundedRectangle(metal,3);
        for(float y=metal.getY()+3;y<metal.getBottom()-2;y+=4){g.setColour(Colours::black.withAlpha(.6f));line(g,metal.getX()+1,y,metal.getRight()-1,y,.9f);g.setColour(ink.withAlpha(.18f));line(g,metal.getX()+1,y+1,metal.getRight()-1,y+1,.6f);}
        float yy=(float)(metal.getBottom()-3-(metal.getHeight()-6)*valueToProportionOfLength(getValue()));g.setColour(gold);g.fillRect(metal.getX()+2,yy,metal.getWidth()-4,1.6f);
    }
private:bool spring;
};
#include "Fretboard.inc"
struct Preset{const char* name;int model,mode,art,stroke,fx;float tone,width,room,echo,spread,release,attack;};
const std::array<Preset,9> presets {{
    {"Amber Clean",2,0,0,0,2,84,100,16,16,12,120,2},
    {"Sunset Strum",2,1,0,2,35,79,114,18,16,14,170,2},
    {"Le Jazz Guitar Afro",2,0,6,0,50,42.2f,36,22,13.4f,12,230,8},
    {"Cedar Air",0,0,0,0,34,89,110,15,16,12,170,2},
    {"Steel Strings",0,1,0,2,38,84,110,12,16,17,130,2},
    {"Nylon Close",1,0,0,0,2,69,98,16,16,12,145,3},
    {"Bossa Room",1,1,0,1,34,72,109,21,16,11,145,3},
    {"Copper Twang",3,0,0,0,7,94,93,16,16,12,90,2},
    {"Midnight Rhythm",3,1,1,2,46,75,124,14,16,9,90,2}
}};
const char* effects[]={"ampOn","eqOn","compOn","chorusOn","echoOn","roomOn"};
const char* macroIds[]={"attack","release","tone","width","echo","room","output"};
const char* macroLabels[]={"Attaque","Relâchement","Brillance","Largeur","Delay","Réverb","Sortie"};
class Editor final:public AudioProcessorEditor,private Timer{
    using SA=AudioProcessorValueTreeState::SliderAttachment;
    using CA=AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA=AudioProcessorValueTreeState::ButtonAttachment;
public:
    explicit Editor(Processor& p):AudioProcessorEditor(p),proc(p),board(p),keys(p.keyboard),pitch(true),mod(false){
        setLookAndFeel(&skin);setOpaque(true);
        atlas=ImageCache::getFromMemory(BinaryData::studio_jpg,BinaryData::studio_jpgSize);
        logo=ImageCache::getFromMemory(BinaryData::swell_mark_png,BinaryData::swell_mark_pngSize);
        const char* names[]={"Acier acoustique","Classique nylon","Double bobinage","Simple bobinage"};
        for(int i=0;i<4;++i){modelsUI[i]=cell(models[i]+"|"+u(names[i]),Cell::library);modelsUI[i]->onClick=[this,i]{set("model",(float)i);};}
        for(int i=0;i<9;++i){artsUI[i]=cell(arts[i],Cell::articulation);artsUI[i]->onClick=[this,i]{set("art",(float)i);};}
        const char* fxLabels[]={"Amp","EQ","Comp","Chorus","Delay","Réverb"};
        for(int i=0;i<6;++i){
            fxUI[i]=cell(u(fxLabels[i]),Cell::fx,i);fxUI[i]->setClickingTogglesState(true);
            toggles[i]=std::make_unique<BA>(proc.params,effects[i],*fxUI[i]);
        }
        for(int i=0;i<7;++i){
            auto& s=knobs[i];s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(Slider::NoTextBox,false,0,0);
            s.setRotaryParameters(MathConstants<float>::pi*1.21f,MathConstants<float>::pi*2.79f,true);
            s.onDragStart=[this,i]{if(i==2)set("eqOn",1);if(i==4)set("echoOn",1);if(i==5)set("roomOn",1);};
            s.setTooltip(u(macroLabels[i])+u("  ·  ")+proc.params.getParameter(macroIds[i])->getName(60));addAndMakeVisible(s);knobAttachments[i]=std::make_unique<SA>(proc.params,macroIds[i],s);
        }
        preset.setColour(ComboBox::backgroundColourId,Colours::transparentBlack);preset.setColour(ComboBox::outlineColourId,Colours::transparentBlack);
        addAndMakeVisible(preset);for(int i=0;i<(int)presets.size();++i)preset.addItem(presets[i].name,i+1);
        preset.setTooltip(u("Choisir un préréglage"));preset.onChange=[this]{if(preset.getSelectedId()>0)usePreset(preset.getSelectedId()-1);};
        back=cell("Précédent",Cell::arrow,-1);next=cell("Suivant",Cell::arrow,1);back->setTooltip(u("Préréglage précédent"));next->setTooltip(u("Préréglage suivant"));
        back->onClick=[this]{usePreset((current+8)%9);};next->onClick=[this]{usePreset((current+1)%9);};
        folder=cell("Bibliothèque",Cell::icon,6);folder->setTooltip(u("Choisir le dossier des échantillons"));folder->onClick=[this]{chooseLibrary();};
        view=cell("Clavier / Manche",Cell::icon,7);view->setTooltip(u("Alterner clavier et manche jouables"));view->onClick=[this]{showBoard=!showBoard;resized();repaint();};
        handNoise=cell(u("Bruits de main"),Cell::small);handNoise->setClickingTogglesState(true);handNoise->setTooltip(u("Bruits de doigts : changement de position et relâchement des cordes"));handNoiseAttach=std::make_unique<BA>(proc.params,"handNoise",*handNoise);
        modeButton=cell("Strum",Cell::small);modeButton->setClickingTogglesState(true);modeButton->onClick=[this]{set("mode",modeButton->getToggleState()?1.f:0.f);};
        addAndMakeVisible(stroke);stroke.addItemList({"Down","Up","Alternate"},1);strokeAttach=std::make_unique<CA>(proc.params,"stroke",stroke);
        addAndMakeVisible(spread);spread.setSliderStyle(Slider::LinearHorizontal);spread.setTextBoxStyle(Slider::NoTextBox,false,0,0);
        spread.setTooltip(u("Espacement entre deux cordes"));spreadAttach=std::make_unique<SA>(proc.params,"spread",spread);
        addAndMakeVisible(keys);addChildComponent(board);addAndMakeVisible(pitch);addAndMakeVisible(mod);
        wheels[0]=std::make_unique<SA>(proc.params,"wheel",pitch);wheels[1]=std::make_unique<SA>(proc.params,"mod",mod);
        pitch.setTooltip("Pitch bend");mod.setTooltip("Vibrato");
        setSize(1040,554);resized();timerCallback();startTimerHz(15);
    }
    ~Editor()override{stopTimer();setLookAndFeel(nullptr);}
    void paint(Graphics& g)override{
        g.fillAll(black);
        if(atlas.isValid()){
            const int w=atlas.getWidth(),h=atlas.getHeight(),row=proc.selection()/2;
            int top=h*row/4+2,bottom=h*(row+1)/4-2;
            g.setImageResamplingQuality(Graphics::highResamplingQuality);
            g.setOpacity(1);g.drawImage(atlas,83,41,957,264,0,top,w,bottom-top);
        }
        ColourGradient header(black,0,0,black.withAlpha(0.f),0,110,false);header.addColour(.54,black.withAlpha(.92f));g.setGradientFill(header);g.fillRect(0,0,1040,113);
        ColourGradient leftShade(black,0,0,black.withAlpha(0.f),301,0,false);leftShade.addColour(.53,black.withAlpha(.93f));g.setGradientFill(leftShade);g.fillRect(0,0,310,345);
        ColourGradient lower(black.withAlpha(0.f),0,195,black,0,309,false);lower.addColour(.48,black.withAlpha(.41f));g.setGradientFill(lower);g.fillRect(0,192,1040,153);
        if(logo.isValid())g.drawImage(logo,Rectangle<float>(22,15,36,33),RectanglePlacement::centred);
        auto mark=display(22.f,true);mark.setExtraKerningFactor(.12f);g.setFont(mark);g.setColour(ink);
        g.drawText("SWELL",68,12,120,38,Justification::centredLeft);float a=GlyphArrangement::getStringWidth(mark,"SWELL");
        g.setFont(display(15.f,false));g.setColour(gold);g.drawText("Realistic guitar",(int)(68+a+12),14,220,38,Justification::centredLeft);
        smallCaps(g,u("GUITARES ACOUSTIQUES & ÉLECTRIQUES"),{25,54,319,18},muted);
        panel(g,{412,15,291,37},17);
        bool ready=proc.loaded.load()==proc.selection();g.setColour(proc.failed?Colour(0xffde8d71):ready?gold:muted);
        g.fillEllipse(818,30,5,5);
        String state=proc.failed?u("Dossier à choisir"):ready?(proc.missing.load()?u("Banque incomplète"):u("Banque prête")):("Chargement "+String(proc.progress.load())+" %");
        word(g,state,{831,19,111,27},12,muted);
        g.setColour(Colour(0xee151413));g.fillRoundedRectangle(12,88,171,171,7);g.setColour(rule.withAlpha(.65f));g.drawRoundedRectangle(12.5f,88.5f,170,170,7,1);
        glyph(g,6,24,99,.85f,muted);smallCaps(g,u("BIBLIOTHÈQUE"),{50,96,126,22},ink);g.setColour(rule);g.drawHorizontalLine(123,23,171);
        for(int i=0;i<7;++i){
            int x=203+i*118;word(g,u(macroLabels[i]),{x,299,105,20},13,ink,false,Justification::centred);
            float v=param(macroIds[i]);String val=String(v,(i==6||i==0)?1:0)+(i<2?" ms":i==6?" dB":" %");
            word(g,val,{x,318,105,16},11,muted,false,Justification::centred);
        }
        section(g,{15,348,296,125},8,"Articulations");section(g,{321,348,431,125},3,u("Chaîne d’effets"));
        section(g,{762,348,263,125},9,"Performance");
        for(int i=0;i<5;++i){float x=396.f+i*68;g.setColour(amber.withAlpha(.65f));line(g,x,410,x+3,413);line(g,x+3,413,x,416);}
        word(g,u("Jeu en accords"),{776,382,121,23},13,ink);
        word(g,u("Sens du coup"),{776,412,104,21},12,muted);word(g,u("Espacement"),{776,440,93,19},12,muted);
        word(g,String(param("spread"),1)+" ms",{956,440,57,19},11,gold,false,Justification::centredRight);
        g.setColour(rule);g.drawHorizontalLine(483,16,1024);
        word(g,"Pitch",{15,534,31,16},10.5f,muted,false,Justification::centred);word(g,"Mod",{51,534,31,16},10.5f,muted,false,Justification::centred);
    }
    void resized()override{
        if(!folder)return;
        back->setBounds(417,18,29,30);next->setBounds(670,18,29,30);preset.setBounds(449,20,215,26);
        folder->setBounds(944,17,33,32);view->setBounds(983,17,33,32);
        for(int i=0;i<4;++i)modelsUI[i]->setBounds(18,126+i*31,159,29);
        for(int i=0;i<7;++i)knobs[i].setBounds(225+i*118,235,61,66);
        for(int i=0;i<9;++i)artsUI[i]->setBounds(25+(i%3)*93,381+(i/3)*28,90,25);
        for(int i=0;i<6;++i)fxUI[i]->setBounds(334+i*68,385,59,69);
        modeButton->setBounds(925,380,83,25);handNoise->setBounds(898,353,110,21);stroke.setBounds(890,411,118,22);spread.setBounds(861,438,96,24);
        pitch.setBounds(19,491,24,43);mod.setBounds(55,491,24,43);
        keys.setBounds(101,491,923,55);keys.setKeyWidth(923.f/36.f);
        board.setBounds(95,486,930,67);board.setVisible(showBoard);keys.setVisible(!showBoard);
    }
private:
    void section(Graphics& g,Rectangle<float> b,int ic,const String& title){
        panel(g,b);glyph(g,ic,b.getX()+13,b.getY()+11,.86f,muted);word(g,title,{(int)b.getX()+38,(int)b.getY()+6,(int)b.getWidth()-52,25},15,ink);
    }
    std::unique_ptr<Cell> cell(String text,Cell::Style style,int icon=-1){
        auto b=std::make_unique<Cell>(text,style,icon);addAndMakeVisible(*b);return b;
    }
    float param(const char* id) const{return proc.params.getRawParameterValue(id)->load();}
    void set(const char* id,float v){
        auto* p=proc.params.getParameter(id);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();
    }
    void usePreset(int i){
        auto& p=presets[(size_t)i];current=i;set("model",p.model);set("mode",p.mode);set("art",p.art);set("stroke",p.stroke);
        set("tone",p.tone);set("width",p.width);set("room",p.room);set("echo",p.echo);set("spread",p.spread);set("release",p.release);set("attack",p.attack);
        for(int j=0;j<6;++j)set(effects[j],(float)((p.fx>>j)&1));timerCallback();
    }
    void chooseLibrary(){
        chooser=std::make_unique<FileChooser>(u("Dossier contenant AG, NG, LP et TC"),File(proc.getLibrary()),"");
        chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectDirectories,[safe=Component::SafePointer<Editor>(this)](const FileChooser& fc){if(safe && fc.getResult().isDirectory())safe->proc.setLibrary(fc.getResult());});
    }
    void timerCallback()override{
        int selected=proc.selection(),a=(int)param("art"),mask=0;
        for(int i=0;i<4;++i)modelsUI[i]->setToggleState(i==selected/2,dontSendNotification);
        for(int i=0;i<9;++i)artsUI[i]->setToggleState(a==i,dontSendNotification);
        for(int i=0;i<6;++i)if(param(effects[i])>.5f)mask|=1<<i;
        bool strum=selected%2;modeButton->setToggleState(strum,dontSendNotification);stroke.setEnabled(strum);spread.setEnabled(strum);
        int match=-1;
        for(int i=0;i<(int)presets.size();++i){
            const auto& p=presets[i];
            if(p.model*2+p.mode==selected && p.art==a && p.stroke==(int)param("stroke") && p.fx==mask && std::abs(p.tone-param("tone"))<.06f
              && std::abs(p.width-param("width"))<.6f && std::abs(p.room-param("room"))<.06f && std::abs(p.echo-param("echo"))<.06f
              && std::abs(p.spread-param("spread"))<.06f && std::abs(p.release-param("release"))<.6f && std::abs(p.attack-param("attack"))<.06f) match=i;
        }
        if(match>=0){current=match;preset.setSelectedId(match+1,dontSendNotification);}
        else{preset.setText(models[selected/2]+u(" · Réglage libre"),dontSendNotification);}
        if(showBoard)board.repaint();repaint();
    }
    Processor& proc;Skin skin;Image atlas,logo;Fretboard board;Keys keys;Wheel pitch,mod;
    std::array<std::unique_ptr<Cell>,4> modelsUI;std::array<std::unique_ptr<Cell>,9> artsUI;
    std::array<std::unique_ptr<Cell>,6> fxUI;std::array<std::unique_ptr<BA>,6> toggles;
    std::array<Slider,7> knobs;std::array<std::unique_ptr<SA>,7> knobAttachments;
    std::array<std::unique_ptr<SA>,2> wheels;
    std::unique_ptr<Cell> back,next,folder,view,modeButton,handNoise;std::unique_ptr<BA> handNoiseAttach;ComboBox preset,stroke;Slider spread;
    std::unique_ptr<CA> strokeAttach;std::unique_ptr<SA> spreadAttach;
    std::unique_ptr<FileChooser> chooser;TooltipWindow tips{this,600};int current=0;bool showBoard=false;
};
}
AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
}
