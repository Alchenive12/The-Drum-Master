from PIL import Image, ImageDraw, ImageFont
S=2
F=lambda n:"/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"
def font(px): return ImageFont.truetype(F(0), int(px*S))
can=Image.open("bg2x.png").convert("RGBA")
def put(name,x,y,w,h,rot=0):
    im=Image.open(name).convert("RGBA").resize((int(w*S),int(h*S)),Image.LANCZOS)
    if rot: im=im.rotate(-rot,resample=Image.BICUBIC,center=(im.width/2,im.height/2))
    can.alpha_composite(im,(int(x*S),int(y*S)))
# the mask hanging in from the top
m=Image.open("mask2x.png").convert("RGBA"); sc=0.62
mw,mh=int(300*sc*S),int(380*sc*S); m=m.resize((mw,mh),Image.LANCZOS)
can.alpha_composite(m,(int((550-150*sc)*S), int((92-380*sc)*S)) if False else (int((550-150*sc)*S),0), (0,int((380*sc-92)*S)))
dr=ImageDraw.Draw(can)
gold=(201,164,92,255); bone=(230,220,198,255)
dr.text((22*S,12*S),"THE DRUM MASTER",font=font(22),fill=gold)
for label,x,w,st in (("LISTEN",701,86,"on"),("+ STRING",795,94,"off"),("PUPPET SHOW",897,116,"hover")):
    put(f"button_{st}@2x.png",x,12,w,30)
    f=font(12.5); tw=dr.textlength(label,font=f)
    dr.text(((x+w/2)*S-tw/2,19.5*S),label,font=f,fill=bone if st!="off" else gold)
put("button_off@2x.png",1021,12,34,30); put("icon_power@2x.png",1026,15,24,24)
put("button_off@2x.png",1061,12,30,30); put("icon_help@2x.png",1064,15,24,24)
put("panel@2x.png",30,532,1040,146)
import math
def knob(cx,cy,d,val,bip=False):
    r=d/2; start,end=-135,135; ang=start+val*(end-start)
    # value arc as the code draws it
    box=[(cx-r)*S,(cy-r)*S,(cx+r)*S,(cy+r)*S]
    dr.arc(box,start-90,end-90,fill=(20,16,22,255),width=int(3.5*S))
    a0=0 if bip else start
    lo,hi=min(a0,ang),max(a0,ang)
    if hi>lo: dr.arc(box,lo-90,hi-90,fill=(176,31,42,255),width=int(3.5*S))
    kd=d*0.78; put("knob_body@2x.png",cx-kd/2,cy-kd/2,kd,kd)
    put("knob_pointer@2x.png",cx-kd/2,cy-kd/2,kd,kd,ang)
groups=[("KICK",146,[("Volume",80,.6),("Punch",146,.5),("Weight",212,.5)]),
        ("SNARE",366,[("Volume",300,.62),("Snap",366,.5),("Fat",432,.5)]),
        ("HATS",586,[("Volume",520,.66),("Tone",586,.5),("Tame",652,.2)]),
        ("GLOBAL",925,[("Dry/Wet",813,1.0),("Output",869,.62),("Sensitivity",925,.7),("Punch",981,.3),("Clip",1037,.2)])]
for title,tx,ks in groups:
    f=font(14); tw=dr.textlength(title,font=f); dr.text((tx*S-tw/2,541*S),title,font=f,fill=gold)
    for lab,x,v in ks:
        d=58 if title!="GLOBAL" else 46
        knob(x,596,d,v)
        f2=font(12); tw=dr.textlength(lab,font=f2); dr.text((x*S-tw/2,629*S),lab,font=f2,fill=gold)
for x in (262,482,772): dr.line([(x*S,545*S),(x*S,667*S)],fill=(125,102,54,160),width=S)
put("button_on@2x.png",700,594,58,26); f=font(11); tw=dr.textlength("LINK",font=f); dr.text((729*S-tw/2,600*S),"LINK",font=f,fill=bone)
can.convert("RGB").save("mockup@2x.png"); can.convert("RGB").resize((1100,688),Image.LANCZOS).save("mockup.png")
