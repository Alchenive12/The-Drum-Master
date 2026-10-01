from lib import *
rnd=random.Random(3)
def blurs(*rs): return "".join(f'<filter id="b{r}" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="{r}"/></filter>' for r in rs)
BRASS='''<linearGradient id="brass" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#d9be86"/><stop offset="0.3" stop-color="#a68650"/><stop offset="0.65" stop-color="#6a5530"/><stop offset="1" stop-color="#3a2e1a"/></linearGradient>'''
def screw(x,y,r,rot,uid):
    return (f'<circle cx="{x}" cy="{y+0.8}" r="{r}" fill="#000" fill-opacity="0.6" filter="url(#b1)"/>'
            f'<circle cx="{x}" cy="{y}" r="{r}" fill="url(#screw{uid})"/>'
            f'<line x1="{x-r*0.8}" y1="{y}" x2="{x+r*0.8}" y2="{y}" stroke="#14100a" stroke-width="{r*0.35}" transform="rotate({rot} {x} {y})"/>'
            f'<line x1="{x-r*0.8}" y1="{y+r*0.2}" x2="{x+r*0.8}" y2="{y+r*0.2}" stroke="#e8d8b0" stroke-opacity="0.35" stroke-width="{r*0.12}" transform="rotate({rot} {x} {y})"/>')
def screwgrad(uid): return f'<radialGradient id="screw{uid}" cx="0.35" cy="0.3" r="0.8"><stop offset="0" stop-color="#cdb98c"/><stop offset="0.6" stop-color="#6d5c3a"/><stop offset="1" stop-color="#2b2416"/></radialGradient>'

# ================= knob body 200x200
d=[blurs(1,2,3,6), noise("pat","0.08",3,4,"#3d5a45","2.0","-1.0",0,0,200,200), noise("scr","0.9 0.02",2,8,"#9aa4b0","2.2","-1.6",0,0,200,200),
   noise("dust","0.5",2,12,"#c9c2b0","2.0","-1.45",0,0,200,200),
   '<radialGradient id="bake" cx="78" cy="70" r="110" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#4a4450"/><stop offset="0.4" stop-color="#1d1a21"/><stop offset="1" stop-color="#050406"/></radialGradient>',
   '<linearGradient id="ringG" x1="40" y1="20" x2="160" y2="185" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#e2cb96"/><stop offset="0.35" stop-color="#9c7d48"/><stop offset="0.7" stop-color="#4f3f22"/><stop offset="1" stop-color="#2a2013"/></linearGradient>',
   '<linearGradient id="bevel" x1="50" y1="40" x2="150" y2="160" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#fff2d0" stop-opacity="0.8"/><stop offset="0.5" stop-color="#fff2d0" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="0.7"/></linearGradient>',
   '<clipPath id="ringC"><circle cx="100" cy="100" r="94"/></clipPath><clipPath id="faceC"><circle cx="100" cy="100" r="70"/></clipPath>']
k=[]
k.append('<circle cx="104" cy="108" r="94" fill="#000" fill-opacity="0.7" filter="url(#b6)"/>')
k.append('<circle cx="100" cy="100" r="94" fill="url(#ringG)"/>')
kn='<g clip-path="url(#ringC)">'
for i in range(120):   # knurling
    a=2*math.pi*i/120; c,s_=math.cos(a),math.sin(a)
    light=0.5+0.5*math.cos(a+2.4)
    kn+=f'<line x1="{100+76*c:.1f}" y1="{100+76*s_:.1f}" x2="{100+96*c:.1f}" y2="{100+96*s_:.1f}" stroke="#000" stroke-opacity="0.45" stroke-width="1.6"/>'
    kn+=f'<line x1="{100+76*math.cos(a+0.02):.1f}" y1="{100+76*math.sin(a+0.02):.1f}" x2="{100+96*math.cos(a+0.02):.1f}" y2="{100+96*math.sin(a+0.02):.1f}" stroke="#fff0c8" stroke-opacity="{0.08+0.3*light:.2f}" stroke-width="0.8"/>'
kn+='<rect width="200" height="200" filter="url(#pat)" opacity="0.8"/></g>'
k.append(kn)
k.append('<circle cx="100" cy="100" r="93" fill="none" stroke="url(#bevel)" stroke-width="2"/>')
k.append('<circle cx="100" cy="100" r="76" fill="#1a140b"/>')
k.append('<circle cx="100" cy="100" r="74" fill="none" stroke="#000" stroke-opacity="0.8" stroke-width="5" filter="url(#b2)"/>')
k.append('<circle cx="100" cy="100" r="70" fill="url(#bake)"/>')
f='<g clip-path="url(#faceC)">'
for r in range(8,70,3): f+=f'<circle cx="100" cy="100" r="{r}" fill="none" stroke="#ffffff" stroke-opacity="{0.015+0.02*(r%2):.3f}" stroke-width="0.6"/>'
f+='<rect width="200" height="200" filter="url(#scr)" opacity="0.35"/><rect width="200" height="200" filter="url(#dust)" opacity="0.5"/>'
f+='<ellipse cx="74" cy="62" rx="34" ry="18" fill="#cfe0f0" fill-opacity="0.22" transform="rotate(-35 74 62)" filter="url(#b6)"/>'
f+='<ellipse cx="68" cy="58" rx="12" ry="5" fill="#ffffff" fill-opacity="0.35" transform="rotate(-35 68 58)" filter="url(#b2)"/>'
f+='<circle cx="100" cy="100" r="70" fill="none" stroke="#000" stroke-opacity="0.6" stroke-width="10" filter="url(#b6)"/></g>'
k.append(f)
k.append('<path d="M45 78 A60 60 0 0 1 90 42" stroke="#cfe0f0" stroke-opacity="0.35" stroke-width="1.2" fill="none"/>')
open("knob_body.svg","w").write(svg(200,200,"\n".join(k),"\n".join(d)))

# ================= knob pointer 200x200 (rotate around 100,100): an ivory inlay set into the bakelite
d=[blurs(1),'<linearGradient id="iv" x1="0" y1="36" x2="0" y2="96" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#f2e9d6"/><stop offset="1" stop-color="#a99b82"/></linearGradient>']
p=['<path d="M100 34 C105 34 107 38 107 43 L104.5 94 C104.5 97 95.5 97 95.5 94 L93 43 C93 38 95 34 100 34 Z" fill="#000" fill-opacity="0.8" transform="translate(0 1.5)" filter="url(#b1)"/>',
   '<path d="M100 34 C105 34 107 38 107 43 L104.5 94 C104.5 97 95.5 97 95.5 94 L93 43 C93 38 95 34 100 34 Z" fill="url(#iv)"/>',
   '<path d="M101 50 L99 62 L101.5 70" stroke="#5e5345" stroke-width="0.6" fill="none"/>',
   '<circle cx="100" cy="44" r="4.2" fill="#5a0f16"/><circle cx="100" cy="44" r="3" fill="#b01f2a"/><circle cx="99" cy="43" r="1" fill="#ff9b9b" fill-opacity="0.6"/>',
   '<circle cx="100" cy="100" r="9" fill="#000" fill-opacity="0.6" filter="url(#b1)"/>',
   '<circle cx="100" cy="100" r="7" fill="url(#iv)"/><circle cx="100" cy="100" r="2" fill="#3a3126"/>']
open("knob_pointer.svg","w").write(svg(200,200,"\n".join(p),"\n".join(d)))

# ================= plaque buttons 120x34
def plaque(state):
    d=[blurs(1,2,4),BRASS,screwgrad("a"),noise("tar","0.12",3,21,"#2f4a3a","2.0","-1.05",0,0,120,34),
       noise("en","0.7",2,22,"#ffffff","2.0","-1.6",0,0,120,34)]
    inner={"off":("#1d1a20","#08070a"),"hover":("#2a2430","#0b0a0e"),"on":("#6e1219","#1e0306")}[state]
    d.append(f'<linearGradient id="enamel" x1="0" y1="6" x2="0" y2="28" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="{inner[0]}"/><stop offset="1" stop-color="{inner[1]}"/></linearGradient>')
    d.append('<clipPath id="pc"><rect x="1" y="1" width="118" height="32" rx="4"/></clipPath>')
    p=['<rect x="1" y="2.5" width="118" height="32" rx="4" fill="#000" fill-opacity="0.7" filter="url(#b2)"/>',
       '<rect x="1" y="1" width="118" height="32" rx="4" fill="url(#brass)"/>',
       f'<rect x="1" y="1" width="118" height="32" rx="4" filter="url(#tar)" clip-path="url(#pc)" opacity="{0.55 if state!="hover" else 0.3}"/>',
       '<rect x="1.5" y="1.5" width="117" height="31" rx="3.6" fill="none" stroke="#fff2cf" stroke-opacity="0.45" stroke-width="0.8"/>',
       '<rect x="13" y="5" width="94" height="24" rx="2.5" fill="#000" fill-opacity="0.8"/>',
       '<rect x="14" y="6" width="92" height="22" rx="2" fill="url(#enamel)"/>',
       '<rect x="14" y="6" width="92" height="22" rx="2" filter="url(#en)" opacity="0.25"/>',
       '<rect x="14" y="6" width="92" height="3" fill="#000" fill-opacity="0.6" filter="url(#b1)"/>',
       '<line x1="14" y1="28.5" x2="106" y2="28.5" stroke="#fff2cf" stroke-opacity="0.35" stroke-width="0.7"/>']
    if state=="on":
        p.insert(5,'<rect x="10" y="3" width="100" height="28" rx="4" fill="#ff2a3a" fill-opacity="0.45" filter="url(#b4)"/>')
        p.append('<ellipse cx="60" cy="17" rx="40" ry="9" fill="#ff3b45" fill-opacity="0.35" filter="url(#b4)"/>')
    if state=="hover":
        p.append('<ellipse cx="40" cy="4" rx="40" ry="5" fill="#ffe9b8" fill-opacity="0.35" filter="url(#b2)"/>')
    p.append(screw(7,17,3.4,35,"a")); p.append(screw(113,17,3.4,-70,"a"))
    open(f"button_{state}.svg","w").write(svg(120,34,"\n".join(p),"\n".join(d)))
for st in ("off","hover","on"): plaque(st)

# ================= lever 80x140
def lever(on):
    d=[blurs(1,2,3,6),BRASS,screwgrad("b"),noise("tarL","0.15",3,31,"#2f4a3a","2.0","-1.0",0,0,80,140),
       '<linearGradient id="steel" x1="35" y1="0" x2="45" y2="0" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#3a3f46"/><stop offset="0.35" stop-color="#c9d2dc"/><stop offset="0.6" stop-color="#6b7480"/><stop offset="1" stop-color="#22262b"/></linearGradient>',
       '<radialGradient id="ball" cx="35" cy="40" r="16" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#f6eedf"/><stop offset="0.6" stop-color="#c2b59a"/><stop offset="1" stop-color="#5e5446"/></radialGradient>',
       '<radialGradient id="lampOn" cx="38" cy="19" r="9" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#f2ffef"/><stop offset="0.35" stop-color="#8fd18a"/><stop offset="1" stop-color="#1f5a24"/></radialGradient>',
       '<radialGradient id="lampOff" cx="38" cy="19" r="9" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#5b6b58"/><stop offset="0.5" stop-color="#1f2a1e"/><stop offset="1" stop-color="#070a07"/></radialGradient>',
       '<clipPath id="plC"><path d="M14 92 H66 L72 104 V128 L66 136 H14 L8 128 V104 Z"/></clipPath>']
    ang=-28 if on else 28
    p=[]
    if on: p.append('<circle cx="40" cy="22" r="16" fill="#8fd18a" fill-opacity="0.45" filter="url(#b6)"/>')
    p+=['<circle cx="40" cy="22" r="10" fill="url(#brass)"/>',
        f'<circle cx="40" cy="22" r="7" fill="url(#{"lampOn" if on else "lampOff"})"/>',
        '<ellipse cx="38" cy="19" rx="2.5" ry="1.6" fill="#fff" fill-opacity="0.7"/>',
        '<path d="M14 94 H66 L72 106 V130 L66 138 H14 L8 130 V106 Z" fill="#000" fill-opacity="0.7" filter="url(#b3)"/>',
        '<path d="M14 92 H66 L72 104 V128 L66 136 H14 L8 128 V104 Z" fill="url(#brass)"/>',
        '<rect width="80" height="140" filter="url(#tarL)" clip-path="url(#plC)" opacity="0.6"/>',
        '<path d="M14 92.6 H66 L71.4 104" stroke="#fff2cf" stroke-opacity="0.5" fill="none" stroke-width="0.9"/>',
        '<rect x="30" y="100" width="20" height="28" rx="4" fill="#050403"/>',
        '<rect x="30" y="100" width="20" height="5" rx="2" fill="#000" filter="url(#b1)"/>',
        screw(15,123,3,20,"b"), screw(65,123,3,-50,"b"), screw(15,103,2.6,80,"b"), screw(65,103,2.6,10,"b"),
        f'<g transform="rotate({ang} 40 112)">',
        '<rect x="38" y="52" width="8" height="66" rx="3" fill="#000" fill-opacity="0.5" filter="url(#b2)"/>',
        '<rect x="36" y="48" width="8" height="66" rx="3" fill="url(#steel)"/>',
        '<circle cx="42" cy="49" r="13" fill="#000" fill-opacity="0.5" filter="url(#b2)"/>',
        '<circle cx="40" cy="46" r="13" fill="url(#ball)"/>',
        '<path d="M33 38 L37 44 L35 50 L39 54" stroke="#3a3126" stroke-width="0.8" fill="none"/>',
        '<ellipse cx="35" cy="40" rx="4" ry="2.4" fill="#fff" fill-opacity="0.55" filter="url(#b1)"/>',
        '</g>',
        '<circle cx="40" cy="112" r="7" fill="url(#brass)" stroke="#1a140b" stroke-width="1"/>',
        '<circle cx="38.5" cy="110.5" r="2" fill="#fff2cf" fill-opacity="0.5"/>']
    open(f"lever_{'on' if on else 'off'}.svg","w").write(svg(120,140,'<g transform="translate(20 0)">'+"\n".join(p)+'</g>',"\n".join(d)))
lever(False); lever(True)

# ================= quick-mix panel 1040x146 (the board under the knobs; code draws labels and dividers)
W,H=1040,146
d=[blurs(1,2,4,8),BRASS,screwgrad("c"),noise("grain","0.002 0.12",3,41,"#000000","2.0","-0.75",0,0,W,H),
   noise("grainL","0.003 0.2",2,43,"#b08a5a","1.6","-0.95",0,0,W,H), noise("dustP","0.6",2,47,"#c9c2b0","2.0","-1.5",0,0,W,H),
   noise("tarP","0.1",3,49,"#2f4a3a","2.0","-1.0",0,0,W,H),
   f'<linearGradient id="woodP" x1="0" y1="0" x2="0" y2="{H}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#2a1c13"/><stop offset="1" stop-color="#120b07"/></linearGradient>',
   f'<clipPath id="panC"><rect x="2" y="2" width="{W-4}" height="{H-4}" rx="6"/></clipPath>']
p=[f'<rect x="2" y="6" width="{W-4}" height="{H-4}" rx="6" fill="#000" fill-opacity="0.8" filter="url(#b8)"/>',
   f'<rect x="2" y="2" width="{W-4}" height="{H-4}" rx="6" fill="url(#woodP)"/>',
   f'<g clip-path="url(#panC)"><rect width="{W}" height="{H}" filter="url(#grainL)" opacity="0.35"/><rect width="{W}" height="{H}" filter="url(#grain)" opacity="0.7"/>']
for _ in range(50):
    x=rnd.uniform(10,W-10); y=rnd.uniform(10,H-10); L=rnd.uniform(8,40)
    p.append(f'<line x1="{x:.1f}" y1="{y:.1f}" x2="{x+L:.1f}" y2="{y+rnd.uniform(-2,2):.1f}" stroke="#c9b08a" stroke-opacity="{rnd.uniform(0.04,0.12):.2f}" stroke-width="0.6"/>')
p.append(f'<rect width="{W}" height="{H}" filter="url(#dustP)" opacity="0.35"/>')
p.append(f'<rect x="0" y="0" width="{W}" height="18" fill="#a9bccf" fill-opacity="0.06" filter="url(#b4)"/>')
p.append(f'<rect x="2" y="2" width="{W-4}" height="{H-4}" rx="6" fill="none" stroke="#000" stroke-opacity="0.8" stroke-width="16" filter="url(#b8)"/></g>')
p.append(f'<rect x="3" y="3" width="{W-6}" height="{H-6}" rx="5.5" fill="none" stroke="url(#brass)" stroke-width="3"/>')
p.append(f'<rect x="7" y="7" width="{W-14}" height="{H-14}" rx="4" fill="none" stroke="#000" stroke-opacity="0.6" stroke-width="1"/>')
p.append(f'<line x1="9" y1="4.5" x2="{W-9}" y2="4.5" stroke="#fff2cf" stroke-opacity="0.35" stroke-width="0.8"/>')
for (cx,cy,sx,sy) in ((0,0,1,1),(W,0,-1,1),(0,H,1,-1),(W,H,-1,-1)):   # brass corner brackets
    br=f"M{cx} {cy} L{cx+sx*46} {cy} L{cx+sx*46} {cy+sy*10} L{cx+sx*10} {cy+sy*10} L{cx+sx*10} {cy+sy*46} L{cx} {cy+sy*46} Z"
    p.append(f'<path d="{br}" fill="#000" fill-opacity="0.6" transform="translate(1 2)" filter="url(#b2)"/>')
    p.append(f'<path d="{br}" fill="url(#brass)"/>')
    cid=f"brC{int(cx)}{int(cy)}"; d.append(f'<clipPath id="{cid}"><path d="{br}"/></clipPath>')
    p.append(f'<rect width="{W}" height="{H}" filter="url(#tarP)" clip-path="url(#{cid})" opacity="0.5"/>')
    p.append(screw(cx+sx*30,cy+sy*5,2.8,rnd.uniform(0,180),"c")); p.append(screw(cx+sx*5,cy+sy*30,2.8,rnd.uniform(0,180),"c"))
open("panel.svg","w").write(svg(W,H,"\n".join(p),"\n".join(d)))

# ================= engraved brass icons 48x48
def icon(name, shape):
    d=[blurs(1),'<linearGradient id="ig" x1="0" y1="6" x2="0" y2="42" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#ecd49a"/><stop offset="0.5" stop-color="#b08d52"/><stop offset="1" stop-color="#6a5530"/></linearGradient>']
    p=[f'<g transform="translate(0.8 1.6)" stroke="#000" stroke-opacity="0.8" fill="none" filter="url(#b1)">{shape("#000")}</g>',
       f'<g stroke="url(#ig)" fill="none">{shape("url(#ig)")}</g>',
       f'<g transform="translate(-0.4 -0.5)" stroke="#fff6dc" stroke-opacity="0.35" fill="none" stroke-width="0.8">{shape("none",thin=True)}</g>']
    open(f"icon_{name}.svg","w").write(svg(48,48,"\n".join(p),"\n".join(d)))
def power(fill,thin=False):
    w=1 if thin else 3.4
    return f'<path d="M15 14 A14 14 0 1 0 33 14" stroke-width="{w}" stroke-linecap="round"/><line x1="24" y1="7" x2="24" y2="24" stroke-width="{w}" stroke-linecap="round"/>'
def helpi(fill,thin=False):
    w=1 if thin else 3.4
    dot='' if thin else f'<circle cx="24" cy="38" r="2.6" fill="{fill}" stroke="none"/>'
    return f'<path d="M17 17 C17 9 31 9 31 17 C31 23 24 23 24 30" stroke-width="{w}" stroke-linecap="round"/>'+dot
def back(fill,thin=False):
    w=1 if thin else 3.4
    return f'<path d="M28 10 L14 24 L28 38" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"/><line x1="14" y1="24" x2="38" y2="24" stroke-width="{w}" stroke-linecap="round"/>'
icon("power",power); icon("help",helpi); icon("back",back)
print("ok")
