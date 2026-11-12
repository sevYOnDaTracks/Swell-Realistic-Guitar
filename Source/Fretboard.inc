class Fretboard final:public Component{
public:
    explicit Fretboard(Processor& p):proc(p){setMouseCursor(MouseCursor::PointingHandCursor);}
    float left()const{return 46.f;}
    float right()const{return (float)getWidth()-13;}
    float fretX(int fret)const{
        const float n=(float)fret/12.f;
        return left()+(right()-left())*(1-std::exp(-.56f*n))/(1-std::exp(-.56f));
    }
    float stringY(int string)const{return 10.f+(5-string)*8.7f;}
    int base()const{
        int hi=-1;for(auto& v:proc.frets)hi=std::max(hi,v.load());
        int top=std::max(hi,Processor::handLow(proc.hand.load())+Processor::handSpan-1);
        return top>12?std::min(18,top-10):0;
    }
    // Orange bar: where the fretting hand currently sits on the neck.
    void drawHand(Graphics& g,int first)const{
        const int lo=Processor::handLow(proc.hand.load()),hi=lo+Processor::handSpan-1;
        int a=std::max(0,lo-1-first),b=std::min(12,hi-first);if(b<=a)return;
        float x0=fretX(a)+1.5f,x1=fretX(b)-1.5f;
        const Colour orange(0xffff6a1f);
        g.setColour(orange.withAlpha(.10f));g.fillRect(x0,3.f,x1-x0,53.f);
        g.setColour(orange.withAlpha(.35f));g.fillRoundedRectangle(x0-1,-.5f,x1-x0+2,5.f,2.f);
        g.setColour(orange);g.fillRoundedRectangle(x0,0.f,x1-x0,3.f,1.5f);
    }
    void paint(Graphics& g)override{
        auto b=getLocalBounds().toFloat();g.setColour(Colour(0xff161411));g.fillRoundedRectangle(b,9);
        auto neck=Rectangle<float>(left(),3,right()-left(),53);
        g.setGradientFill(ColourGradient(Colour(0xff463229),neck.getTopLeft(),Colour(0xff2c2421),neck.getBottomLeft(),false));g.fillRect(neck);
        g.setColour(Colour(0xff5c4436));
        for(int i=0;i<15;++i){
            Path p;p.startNewSubPath(left(),5+i*3.4f);
            p.cubicTo(getWidth()*.3f,5+i*3.4f,getWidth()*.59f,8+i*3.4f,right(),5+i*3.4f);
            g.strokePath(p,PathStrokeType(.5f));
        }
        int first=base();
        for(int i=0;i<=12;++i){
            float x=fretX(i);
            g.setColour(i==0&&!first?Colour(0xffdacbb1):Colour(0xff8c8173));
            g.fillRect(x-1,neck.getY(),(i==0&&!first)?4.f:2.f,neck.getHeight());
            if(i>0){int n=i+first;float mid=(x+fretX(i-1))*.5f;
                if(n%12==0){for(float yy:{18.f,37.f}){g.setColour(Colour(0xffa39173));g.fillEllipse(mid-3.2f,yy,6.4f,6.4f);}}
                else if(n%12==3||n%12==5||n%12==7||n%12==9){g.setColour(Colour(0xffa39173));g.fillEllipse(mid-3,26,6,6);}
                if(i==1||n%12==0||n%12==3||n%12==5||n%12==7||n%12==9)
                    word(g,String(n),{(int)mid-13,55,26,16},11,muted,false,Justification::centred);
            }
        }
        drawHand(g,first);
        const char* names[]={"E","A","D","G","B","e"};
        for(int s=0;s<6;++s){
            float yy=stringY(s),th=.75f+(5-s)*.25f;
            g.setColour(Colours::black.withAlpha(.5f));g.drawHorizontalLine((int)yy+2,left(),right());
            g.setColour(s<3?Colour(0xffc4996f):Colour(0xffd1c9b8));g.fillRect(left(),yy-th*.5f,right()-left(),th);
            word(g,names[s],{7,(int)yy-8,30,16},12,muted,true,Justification::centred);
            int n=proc.frets[s].load();if(n<0 || n<first || n>first+12)continue;
            int rel=n-first;float xx=rel?(fretX(rel-1)+fretX(rel))*.5f:left()-10;
            g.setColour(sage);g.fillEllipse(xx-6.5f,yy-6.5f,13,13);g.setColour(night);g.drawEllipse(xx-3,yy-3,6,6,1.2f);
        }
    }
    void mouseDown(const MouseEvent& e)override{fingerBase=base();trigger(e.position);}
    void mouseDrag(const MouseEvent& e)override{trigger(e.position);}
    void mouseUp(const MouseEvent&)override{off();}
    ~Fretboard()override{off();}
private:
    Processor& proc;int sounding=-1,fingerBase=0;
    void off(){if(sounding>=0){proc.keyboard.noteOff(1,sounding,0);sounding=-1;}}
    void trigger(Point<float> p){
        if(p.y<2 || p.y>57 || p.x>right()){off();return;}
        int string=5-jlimit(0,5,roundToInt((p.y-stringY(5))/8.7f));int f=0;
        while(f<12 && p.x>fretX(f))++f;
        int note=tuning[string]+f+fingerBase;if(note==sounding)return;
        off();proc.keyboard.noteOn(1,note,.77f);sounding=note;
    }
};
