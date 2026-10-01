from lib import *
rnd = random.Random(7)
W,H = 1100,688; HZ=413; VP=(550,289)
WX,WY,WW,WH = 99,82,132,227
defs=[]; b=[]
# ---------- filters
defs.append(noise("wallgrain","0.9 0.6",3,3,"#000000","1.8","-0.75",0,0,W,HZ))
defs.append(noise("wallmottle","0.008 0.012",4,11,"#05070a","1.6","-0.55",0,0,W,HZ))
defs.append(noise("wallmottleL","0.01 0.006",3,19,"#6f7f92","1.2","-0.62",0,0,W,HZ))
# damp stains with tide-line rims
defs.append(f'''<filter id="stain" filterUnits="userSpaceOnUse" x="0" y="0" width="{W}" height="{HZ}" color-interpolation-filters="sRGB">
<feTurbulence type="fractalNoise" baseFrequency="0.0045 0.009" numOctaves="4" seed="5" result="n"/>
<feColorMatrix in="n" type="matrix" values="0 0 0 0 0.10 0 0 0 0 0.11 0 0 0 0 0.07 1 0 0 0 0" result="c"/>
<feComponentTransfer in="c"><feFuncA type="table" tableValues="0 0 0 0 0 0 0.05 0.35 0.22 0.5 0.62 0.7"/></feComponentTransfer></filter>''')
defs.append(noise("woodgrain","0.004 0.11",3,23,"#000000","1.9","-0.7",0,HZ,W,H-HZ))
defs.append(noise("woodgrainL","0.003 0.07",2,29,"#8c97a6","1.4","-0.85",0,HZ,W,H-HZ))
defs.append(noise("dirt","0.02 0.03",4,31,"#020203","1.7","-0.55",0,HZ,W,H-HZ))
defs.append(noise("glassgrime","0.03",4,41,"#2b3138","1.6","-0.5",WX,WY,WW,WH))
defs.append(noise("grain","0.85",2,51,"#ffffff","1.6","-1.05",0,0,W,H))
defs.append(noise("grainD","0.75",2,53,"#000000","1.6","-0.9",0,0,W,H))
defs.append(noise("haze","0.015 0.03",3,61,"#a9bccf","1.3","-0.6",0,0,W,H))
for r in (2,4,8,14,30): defs.append(f'<filter id="b{r}" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="{r}"/></filter>')
# gradients
defs.append(f'''<radialGradient id="wallLight" cx="230" cy="190" r="760" gradientUnits="userSpaceOnUse">
<stop offset="0" stop-color="#2f3946"/><stop offset="0.35" stop-color="{C["wall"]}"/><stop offset="0.8" stop-color="{C["wallDark"]}"/><stop offset="1" stop-color="#05070a"/></radialGradient>
<linearGradient id="floorG" x1="0" y1="{HZ}" x2="0" y2="{H}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#07080a"/><stop offset="0.5" stop-color="#14171c"/><stop offset="1" stop-color="#1d2026"/></linearGradient>
<linearGradient id="sky" x1="0" y1="{WY}" x2="0" y2="{WY+WH}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#3c4f66"/><stop offset="0.6" stop-color="#1c2737"/><stop offset="1" stop-color="#0e141d"/></linearGradient>
<radialGradient id="moonGlow" cx="205" cy="112" r="90" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#e8f0f8" stop-opacity="0.95"/><stop offset="0.12" stop-color="#c9d6e4" stop-opacity="0.7"/><stop offset="0.45" stop-color="#8ea3ba" stop-opacity="0.18"/><stop offset="1" stop-color="#8ea3ba" stop-opacity="0"/></radialGradient>
<linearGradient id="shaft" x1="160" y1="{WY+WH}" x2="330" y2="{H}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="{C["moon"]}" stop-opacity="0.24"/><stop offset="1" stop-color="{C["moon"]}" stop-opacity="0.05"/></linearGradient>
<linearGradient id="patch" x1="0" y1="560" x2="0" y2="{H}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="{C["moon"]}" stop-opacity="0.2"/><stop offset="1" stop-color="{C["moon"]}" stop-opacity="0.42"/></linearGradient>
<radialGradient id="vign" cx="550" cy="330" r="700" gradientUnits="userSpaceOnUse"><stop offset="0.45" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="0.85"/></radialGradient>
<linearGradient id="ceil" x1="0" y1="0" x2="0" y2="140" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#000" stop-opacity="0.85"/><stop offset="1" stop-color="#000" stop-opacity="0"/></linearGradient>
<linearGradient id="frameW" x1="{WX}" y1="0" x2="{WX+WW}" y2="0" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#2a2f36"/><stop offset="0.5" stop-color="#4d555f"/><stop offset="1" stop-color="#20252b"/></linearGradient>
<linearGradient id="base" x1="0" y1="{HZ-14}" x2="0" y2="{HZ}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#2b3038"/><stop offset="0.15" stop-color="#14171c"/><stop offset="1" stop-color="#07080a"/></linearGradient>
<radialGradient id="bronze" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#6e6a58"/><stop offset="0.5" stop-color="#47463a"/><stop offset="0.85" stop-color="#3a4436"/><stop offset="1" stop-color="#232821"/></radialGradient>
<radialGradient id="head" cx="0.42" cy="0.38" r="0.65"><stop offset="0" stop-color="#7f7a6d"/><stop offset="0.7" stop-color="#55524a"/><stop offset="1" stop-color="#2a2926"/></radialGradient>
<linearGradient id="shell" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#14161b"/><stop offset="0.35" stop-color="#3d4350"/><stop offset="0.55" stop-color="#2a2f39"/><stop offset="1" stop-color="#0c0d10"/></linearGradient>
<linearGradient id="chrome" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#3a3f46"/><stop offset="0.45" stop-color="#9aa4b0"/><stop offset="1" stop-color="#2a2e33"/></linearGradient>
<linearGradient id="cloth" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#5f5c55"/><stop offset="0.5" stop-color="#3d3b37"/><stop offset="1" stop-color="#1a1918"/></linearGradient>''')

# ---------- wall
b.append(f'<rect width="{W}" height="{HZ}" fill="url(#wallLight)"/>')
# wallpaper: wide faded bands with double pinstripes, uneven fading
x=0; i=0
while x < W:
    bw = 22
    op = 0.05 + 0.04*rnd.random()
    b.append(f'<rect x="{x:.1f}" y="0" width="{bw}" height="{HZ}" fill="#8a9bb0" fill-opacity="{op:.3f}"/>')
    b.append(f'<rect x="{x+bw+3:.1f}" y="0" width="1.2" height="{HZ}" fill="#9fb0c4" fill-opacity="{op*1.6:.3f}"/>')
    b.append(f'<rect x="{x+bw+7:.1f}" y="0" width="1.2" height="{HZ}" fill="#9fb0c4" fill-opacity="{op*1.6:.3f}"/>')
    b.append(f'<rect x="{x+bw+10:.1f}" y="0" width="{bw*0.5:.1f}" height="{HZ}" fill="#000" fill-opacity="0.12"/>')
    x += bw+22; i+=1
# seams between paper strips, some lifting
for sx in range(60, W, 176):
    b.append(f'<line x1="{sx}" y1="0" x2="{sx+rnd.uniform(-2,2):.1f}" y2="{HZ-14}" stroke="#000" stroke-opacity="0.35" stroke-width="1.2"/>')
    b.append(f'<line x1="{sx+1.5}" y1="0" x2="{sx+1.5}" y2="{HZ-14}" stroke="#a9bccf" stroke-opacity="0.05" stroke-width="1"/>')
b.append(f'<rect width="{W}" height="{HZ}" filter="url(#wallmottleL)" opacity="0.5"/>')
b.append(f'<rect width="{W}" height="{HZ}" filter="url(#wallmottle)"/>')
b.append(f'<rect width="{W}" height="{HZ}" filter="url(#stain)"/>')
b.append(f'<rect width="{W}" height="{HZ}" filter="url(#wallgrain)" opacity="0.35"/>')
# drips running down from stains
for _ in range(26):
    dx=rnd.uniform(20,W-20); dy=rnd.uniform(20,200); L=rnd.uniform(40,200)
    if WX-10<dx<WX+WW+10 and dy<WY+WH: continue
    w=rnd.uniform(1.5,4)
    b.append(f'<path d="M{dx:.1f} {dy:.1f} C{dx+rnd.uniform(-2,2):.1f} {dy+L*0.4:.1f} {dx+rnd.uniform(-3,3):.1f} {dy+L*0.7:.1f} {dx+rnd.uniform(-2,2):.1f} {dy+L:.1f}" stroke="#05070a" stroke-opacity="{rnd.uniform(0.15,0.35):.2f}" stroke-width="{w:.1f}" stroke-linecap="round" fill="none" filter="url(#b2)"/>')
# peeling wallpaper patches: plaster underneath, torn edge, curled lip
def peel(cx,cy,rw,rh,seed):
    r=random.Random(seed); ring=[]
    for k in range(22):
        a=2*math.pi*k/22; rr=1+r.uniform(-0.35,0.25)
        ring.append((cx+rw*rr*math.cos(a), cy+rh*rr*math.sin(a)))
    ring=jag(ring+[ring[0]],2.2,r,3)
    b.append(f'<path d="{d_of(ring,True)}" fill="#000" fill-opacity="0.5" transform="translate(2 3)" filter="url(#b2)"/>')
    b.append(f'<path d="{d_of(ring,True)}" fill="#1f2328"/>')
    b.append(f'<path d="{d_of(ring,True)}" fill="#3b4246" fill-opacity="0.35" transform="translate(-1.5 -1.5) scale(1)"/>')
    for _ in range(6):  # plaster cracks
        p=[(cx+r.uniform(-rw*0.6,rw*0.6), cy+r.uniform(-rh*0.6,rh*0.6))]
        for s in range(4): p.append((p[-1][0]+r.uniform(-9,9), p[-1][1]+r.uniform(-9,9)))
        b.append(f'<path d="{d_of(p)}" stroke="#14171a" stroke-width="0.8" fill="none" stroke-opacity="0.8"/>')
    lip=ring[int(len(ring)*0.55):int(len(ring)*0.85)]
    b.append(f'<path d="{d_of(lip)}" stroke="#8a9bb0" stroke-opacity="0.28" stroke-width="2" fill="none" stroke-linecap="round"/>')
peel(318,58,22,30,1); peel(1068,215,18,40,2); peel(720,22,30,14,3); peel(30,340,14,26,4)
# handprints smeared near the window (dark, faint)
def hand(x,y,rot,op):
    g=f'<g transform="translate({x} {y}) rotate({rot})" fill="#020304" fill-opacity="{op}" filter="url(#b2)"><ellipse cx="0" cy="0" rx="11" ry="13"/>'
    for fx,fy,l in ((-9,-14,15),(-3,-18,19),(3,-18,20),(9,-15,16)): g+=f'<rect x="{fx-2.3}" y="{fy-l}" width="4.6" height="{l}" rx="2.3"/>'
    g+='<rect x="10" y="-6" width="4.6" height="13" rx="2.3" transform="rotate(-40 12 0)"/></g>'
    b.append(g)
    b.append(f'<path d="M{x-6} {y+12} L{x-7} {y+60}" stroke="#020304" stroke-opacity="{op*0.6}" stroke-width="3" filter="url(#b2)"/>')
hand(262,236,12,0.38); hand(284,262,-8,0.28)
# window bloom on the wall
b.append(f'<rect x="{WX-30}" y="{WY-30}" width="{WW+60}" height="{WH+60}" fill="#7e93ab" fill-opacity="0.18" filter="url(#b30)"/>')
# ceiling darkness
b.append(f'<rect width="{W}" height="140" fill="url(#ceil)"/>')

# ---------- window
b.append(f'<rect x="{WX-12}" y="{WY-12}" width="{WW+24}" height="{WH+30}" fill="#07090c"/>')   # recess
b.append(f'<rect x="{WX}" y="{WY}" width="{WW}" height="{WH}" fill="url(#sky)"/>')
b.append(f'<rect x="{WX}" y="{WY}" width="{WW}" height="{WH}" fill="url(#moonGlow)"/>')
# bare branches outside
def branch(x,y,ang,L,w,depth,out):
    if depth==0 or L<3: return
    x2=x+L*math.cos(ang); y2=y+L*math.sin(ang)
    out.append(f'<line x1="{x:.1f}" y1="{y:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="#05080c" stroke-width="{w:.2f}" stroke-linecap="round"/>')
    for _ in range(2 if depth>2 else rnd.choice((1,2,2))):
        branch(x2,y2,ang+rnd.uniform(-0.7,0.7),L*rnd.uniform(0.6,0.8),w*0.65,depth-1,out)
br=[]
branch(WX-6,WY+WH*0.62,-0.55,46,5,7,br); branch(WX+WW+4,WY+30,3.5,40,4,6,br)
b.append(f'<clipPath id="glass"><rect x="{WX}" y="{WY}" width="{WW}" height="{WH}"/></clipPath>')
b.append(f'<g clip-path="url(#glass)" opacity="0.92">{"".join(br)}</g>')
b.append(f'<rect x="{WX}" y="{WY}" width="{WW}" height="{WH}" filter="url(#glassgrime)" opacity="0.8"/>')
# crack star in the upper-right pane
cx0,cy0=WX+WW*0.78, WY+WH*0.2
for k in range(9):
    a=rnd.uniform(0,2*math.pi); L=rnd.uniform(12,38)
    p=jag([(cx0,cy0),(cx0+L*math.cos(a),cy0+L*math.sin(a))],1.4,rnd,4)
    b.append(f'<path d="{d_of(p)}" stroke="#dfe8f2" stroke-opacity="0.35" stroke-width="0.6" fill="none" clip-path="url(#glass)"/>')
for rr in (6,13):
    b.append(f'<circle cx="{cx0}" cy="{cy0}" r="{rr}" stroke="#dfe8f2" stroke-opacity="0.25" stroke-width="0.7" fill="none" stroke-dasharray="5 4"/>')
# frame: muntins + sash, peeling paint
fr=f'<g fill="url(#frameW)">'
fr+=f'<rect x="{WX-6}" y="{WY-6}" width="{WW+12}" height="8"/><rect x="{WX-6}" y="{WY+WH-2}" width="{WW+12}" height="9"/>'
fr+=f'<rect x="{WX-6}" y="{WY-6}" width="8" height="{WH+12}"/><rect x="{WX+WW-2}" y="{WY-6}" width="8" height="{WH+12}"/>'
fr+=f'<rect x="{WX+WW/2-3}" y="{WY}" width="6" height="{WH}"/><rect x="{WX}" y="{WY+WH/2-3}" width="{WW}" height="6"/></g>'
b.append(fr)
for _ in range(30):
    px=rnd.choice([WX-6+rnd.uniform(0,8), WX+WW-2+rnd.uniform(0,8), WX+WW/2-3+rnd.uniform(0,6)]); py=rnd.uniform(WY,WY+WH)
    b.append(f'<ellipse cx="{px:.1f}" cy="{py:.1f}" rx="{rnd.uniform(0.8,2.2):.1f}" ry="{rnd.uniform(1.5,5):.1f}" fill="#11151a"/>')
b.append(f'<rect x="{WX-8}" y="{WY+WH+5}" width="{WW+16}" height="7" fill="#3a414b"/><rect x="{WX-8}" y="{WY+WH+11}" width="{WW+16}" height="4" fill="#000" fill-opacity="0.6" filter="url(#b2)"/>')
b.append(f'<line x1="{WX-8}" y1="{WY+WH+5}" x2="{WX+WW+8}" y2="{WY+WH+5}" stroke="#a9bccf" stroke-opacity="0.4"/>')

# ---------- floor
b.append(f'<rect y="{HZ}" width="{W}" height="{H-HZ}" fill="url(#floorG)"/>')
t=(HZ-VP[1])/(H-VP[1])
edges=[VP[0]+i*W*0.05 for i in range(-26,27)]
for i,(e1,e2) in enumerate(zip(edges,edges[1:])):
    a1=VP[0]+(e1-VP[0])*t; a2=VP[0]+(e2-VP[0])*t
    tone=rnd.uniform(-0.06,0.05)
    col="#ffffff" if tone>0 else "#000000"
    b.append(f'<polygon points="{a1:.1f},{HZ} {a2:.1f},{HZ} {e2:.1f},{H} {e1:.1f},{H}" fill="{col}" fill-opacity="{abs(tone):.3f}"/>')
    # butt joints in perspective
    for _ in range(rnd.choice((0,1,1))):
        f=rnd.uniform(0.15,0.95); y=HZ+(H-HZ)*f*f
        tt=(y-VP[1])/(H-VP[1]); xa=VP[0]+(e1-VP[0])*tt; xb=VP[0]+(e2-VP[0])*tt
        b.append(f'<line x1="{xa:.1f}" y1="{y:.1f}" x2="{xb:.1f}" y2="{y:.1f}" stroke="#000" stroke-opacity="0.7" stroke-width="{0.6+1.2*f:.2f}"/>')
    b.append(f'<line x1="{a1:.1f}" y1="{HZ}" x2="{e1:.1f}" y2="{H}" stroke="#000" stroke-opacity="0.75" stroke-width="1.4"/>')
    b.append(f'<line x1="{a1+0.8:.1f}" y1="{HZ}" x2="{e1+1.5:.1f}" y2="{H}" stroke="#a9bccf" stroke-opacity="0.05" stroke-width="1"/>')
b.append(f'<rect y="{HZ}" width="{W}" height="{H-HZ}" filter="url(#woodgrain)" opacity="0.55"/>')
b.append(f'<rect y="{HZ}" width="{W}" height="{H-HZ}" filter="url(#woodgrainL)" opacity="0.3"/>')
b.append(f'<rect y="{HZ}" width="{W}" height="{H-HZ}" filter="url(#dirt)" opacity="0.6"/>')
for _ in range(40):   # scratches
    x=rnd.uniform(0,W); y=rnd.uniform(HZ+40,H); L=rnd.uniform(10,50); a=rnd.uniform(-0.3,0.3)
    b.append(f'<line x1="{x:.1f}" y1="{y:.1f}" x2="{x+L*math.cos(a):.1f}" y2="{y+L*math.sin(a)*0.3:.1f}" stroke="#9aa6b4" stroke-opacity="{rnd.uniform(0.04,0.1):.2f}" stroke-width="0.7"/>')
# baseboard
b.append(f'<rect y="{HZ-14}" width="{W}" height="14" fill="url(#base)"/>')
b.append(f'<line x1="0" y1="{HZ-14}" x2="{W}" y2="{HZ-14}" stroke="#7c8a9a" stroke-opacity="0.25"/>')
b.append(f'<rect y="{HZ}" width="{W}" height="16" fill="#000" fill-opacity="0.55" filter="url(#b4)"/>')

# ---------- moonlight: shaft, haze and the window's print on the floor
shaft=f"{WX},{WY} {WX+WW},{WY} {WX+WW+W*0.26:.0f},{H} {WX+W*0.06:.0f},{H}"
b.append(f'<polygon points="{shaft}" fill="url(#shaft)" filter="url(#b8)"/>')
b.append(f'<clipPath id="shaftC"><polygon points="{shaft}"/></clipPath>')
b.append(f'<rect width="{W}" height="{H}" filter="url(#haze)" clip-path="url(#shaftC)" opacity="0.22"/>')
P=[(165,560),(300,560),(392,688),(205,688)]
b.append(f'<polygon points="{pts(P)}" fill="url(#patch)" filter="url(#b4)"/>')
mx1=((P[0][0]+P[1][0])/2,560); mx2=((P[2][0]+P[3][0])/2,688)
b.append(f'<line x1="{mx1[0]}" y1="{mx1[1]}" x2="{mx2[0]}" y2="{mx2[1]}" stroke="#000" stroke-opacity="0.45" stroke-width="6" filter="url(#b2)"/>')
b.append(f'<line x1="{(P[0][0]+P[3][0])/2+4}" y1="616" x2="{(P[1][0]+P[2][0])/2-4}" y2="616" stroke="#000" stroke-opacity="0.45" stroke-width="7" filter="url(#b2)"/>')
b.append(f'<ellipse cx="290" cy="640" rx="60" ry="120" fill="{C["moon"]}" fill-opacity="0.07" filter="url(#b14)"/>')
b.append(f'<ellipse cx="170" cy="470" rx="40" ry="60" fill="{C["moon"]}" fill-opacity="0.06" filter="url(#b14)"/>')
# motes frozen in the beam (code animates its own on top)
for _ in range(90):
    yy=rnd.uniform(WY+WH,H); f=(yy-WY)/(H-WY)
    xl=WX+(WX+W*0.06-WX)*f; xr=WX+WW+(W*0.26)*f; xx=rnd.uniform(xl,xr)
    b.append(f'<circle cx="{xx:.1f}" cy="{yy:.1f}" r="{rnd.uniform(0.4,1.3):.2f}" fill="#dfe8f2" fill-opacity="{rnd.uniform(0.1,0.5):.2f}"/>')

# ---------- the old drum set in the back corner
s=1.0; BX,BY=960,410
def S(x,y): return BX+x*s, BY+y*s
d=[]
d.append(f'<ellipse cx="{BX}" cy="{BY+4}" rx="{150*s}" ry="{14*s}" fill="#000" fill-opacity="0.7" filter="url(#b8)"/>')
def stand(x,top):
    x0,y0=S(x,0); _,yt=S(x,top)
    d.append(f'<line x1="{x0}" y1="{y0}" x2="{x0}" y2="{yt}" stroke="#5b636d" stroke-width="{2.6*s}"/>')
    for dx in (-16,16,0): d.append(f'<line x1="{x0}" y1="{y0-14*s}" x2="{x0+dx*s}" y2="{y0+(3 if dx else 5)*s}" stroke="#4a525c" stroke-width="{1.8*s}"/>')
def cymbal(x,y,w,tilt,crack=False):
    cx,cy=S(x,y); rx=w*s/2; ry=rx*tilt
    d.append(f'<ellipse cx="{cx}" cy="{cy+3}" rx="{rx}" ry="{ry}" fill="#000" fill-opacity="0.5" filter="url(#b2)"/>')
    d.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="url(#bronze)"/>')
    for k in range(1,7): d.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{rx*k/7}" ry="{ry*k/7}" fill="none" stroke="#000" stroke-opacity="0.18" stroke-width="0.5"/>')
    d.append(f'<ellipse cx="{cx}" cy="{cy-ry*0.15}" rx="{rx*0.18}" ry="{ry*0.32}" fill="#5c584a"/>')
    d.append(f'<ellipse cx="{cx-rx*0.3}" cy="{cy-ry*0.35}" rx="{rx*0.35}" ry="{ry*0.2}" fill="#c9d6e4" fill-opacity="0.12"/>')
    if crack: d.append(f'<path d="M{cx+rx*0.95} {cy} L{cx+rx*0.55} {cy+ry*0.15} L{cx+rx*0.2} {cy+ry*0.05}" stroke="#0a0c0a" stroke-width="1.1" fill="none"/>')
stand(-70,-150); stand(95,-128); stand(-115,-88)
cymbal(-70,-150,92,0.27,True); cymbal(95,-128,76,0.3)
cymbal(-115,-93,52,0.22); cymbal(-115,-86,52,0.22)
# floor tom under a dust sheet
fx,fy=S(70,0)
sheet=f"M{fx-36*s} {fy} C{fx-42*s} {fy-40*s} {fx-30*s} {fy-66*s} {fx-6*s} {fy-72*s} C{fx+18*s} {fy-78*s} {fx+38*s} {fy-62*s} {fx+42*s} {fy-38*s} C{fx+46*s} {fy-18*s} {fx+44*s} {fy-4*s} {fx+48*s} {fy+2*s} Z"
d.append(f'<path d="{sheet}" fill="url(#cloth)"/>')
for k in range(5):
    ox=fx+(-26+k*15)*s
    d.append(f'<path d="M{ox} {fy-60*s+abs(k-2)*4*s} C{ox+4*s} {fy-36*s} {ox-3*s} {fy-16*s} {ox+2*s} {fy}" stroke="#000" stroke-opacity="0.35" stroke-width="{3*s}" fill="none" filter="url(#b2)"/>')
    d.append(f'<path d="M{ox+4*s} {fy-58*s+abs(k-2)*4*s} C{ox+8*s} {fy-36*s} {ox+1*s} {fy-16*s} {ox+6*s} {fy}" stroke="#c9c2b0" stroke-opacity="0.18" stroke-width="{1.5*s}" fill="none"/>')
# bass drum (front head toward us, slightly turned)
kx,ky=S(0,-50); kr=50*s
d.append(f'<circle cx="{kx}" cy="{ky}" r="{kr+3*s}" fill="url(#shell)"/>')
d.append(f'<circle cx="{kx}" cy="{ky}" r="{kr}" fill="#2c3038" stroke="#6b737d" stroke-width="{2.4*s}"/>')
d.append(f'<circle cx="{kx}" cy="{ky}" r="{kr-5*s}" fill="url(#head)"/>')
d.append(f'<circle cx="{kx}" cy="{ky}" r="{kr*0.55}" fill="none" stroke="#3a3530" stroke-opacity="0.45" stroke-width="{3*s}"/>')
d.append(f'<circle cx="{kx}" cy="{ky}" r="{kr*0.45}" fill="none" stroke="#5a1a1e" stroke-opacity="0.35" stroke-width="{1.4*s}"/>')
for k in range(10):
    a=2*math.pi*k/10; lx=kx+(kr+1)*math.cos(a); ly=ky+(kr+1)*math.sin(a)
    d.append(f'<rect x="{lx-2*s}" y="{ly-2*s}" width="{4*s}" height="{4*s}" fill="#8a939e" transform="rotate({math.degrees(a)} {lx} {ly})"/>')
split=jag([(kx-kr*0.6,ky-kr*0.55),(kx-kr*0.1,ky-kr*0.05),(kx+kr*0.25,ky+kr*0.35)],1.2,rnd,4)
d.append(f'<path d="{d_of(split)}" stroke="#141210" stroke-width="{2.2*s}" fill="none"/>')
d.append(f'<path d="{d_of([(p[0]+1.5,p[1]+1) for p in split])}" stroke="#d8d2c4" stroke-opacity="0.4" stroke-width="0.7" fill="none"/>')
# rack tom on the kick
tx,ty=S(-22,-112)
d.append(f'<rect x="{tx-24*s}" y="{ty}" width="{48*s}" height="{28*s}" rx="{3*s}" fill="url(#shell)"/>')
d.append(f'<ellipse cx="{tx}" cy="{ty}" rx="{24*s}" ry="{7*s}" fill="url(#head)" stroke="#6b737d" stroke-width="{1.4*s}"/>')
# snare on its stand
stand(-75,-48)
sx,sy=S(-75,-60)
d.append(f'<rect x="{sx-26*s}" y="{sy}" width="{52*s}" height="{18*s}" fill="url(#chrome)" fill-opacity="0.7"/>')
d.append(f'<ellipse cx="{sx}" cy="{sy}" rx="{26*s}" ry="{7*s}" fill="url(#head)" stroke="#8a939e" stroke-width="{1.2*s}"/>')
d.append(f'<ellipse cx="{sx}" cy="{sy+18*s}" rx="{26*s}" ry="{6*s}" fill="#000" fill-opacity="0.4"/>')
b.append(f'<g>{"".join(d)}</g>')
# a cold rim light on the set from the window
b.append(f'<ellipse cx="{BX-60}" cy="{BY-70}" rx="140" ry="90" fill="#000" fill-opacity="0.45" filter="url(#b30)"/>')

# ---------- cobwebs
def web(cx,cy,a0,a1,R,n,seed):
    r=random.Random(seed); spokes=[a0+(a1-a0)*k/(n-1) for k in range(n)]
    g=f'<g stroke="#c4cfdb" fill="none" stroke-linecap="round">'
    for a in spokes: g+=f'<line x1="{cx}" y1="{cy}" x2="{cx+R*math.cos(a):.1f}" y2="{cy+R*math.sin(a):.1f}" stroke-opacity="0.22" stroke-width="0.6"/>'
    for ring in range(1,11):
        rr=R*ring/11*(1+r.uniform(-0.05,0.05)); path=""
        for k,a in enumerate(spokes):
            x=cx+rr*math.cos(a); y=cy+rr*math.sin(a)
            if k==0: path=f"M{x:.1f} {y:.1f}"
            else:
                am=(a+spokes[k-1])/2; sag=rr*0.88
                path+=f" Q{cx+sag*math.cos(am):.1f} {cy+sag*math.sin(am)+2:.1f} {x:.1f} {y:.1f}"
        g+=f'<path d="{path}" stroke-opacity="{0.08+0.1*r.random():.2f}" stroke-width="0.5"/>'
    g+='</g>'; b.append(g)
web(W,0,math.pi*0.5,math.pi,120,9,1); web(0,0,0,math.pi*0.5,80,7,2)
web(W,HZ-14,math.pi,math.pi*1.5,70,7,3)
# strand from the crash cymbal down to the wall corner
cx_,cy_=S(-70,-150)
b.append(f'<path d="M{cx_+30} {cy_} Q{cx_+70} {cy_-40} {W} {cy_-80}" stroke="#c4cfdb" stroke-opacity="0.15" stroke-width="0.6" fill="none"/>')

# ---------- grade: vignette, grain
b.append(f'<rect width="{W}" height="{H}" fill="url(#vign)"/>')
b.append(f'<rect width="{W}" height="{H}" fill="#1a2c44" fill-opacity="0.12"/>')
b.append(f'<rect width="{W}" height="{H}" filter="url(#grain)" opacity="0.10"/>')
b.append(f'<rect width="{W}" height="{H}" filter="url(#grainD)" opacity="0.18"/>')
open("background.svg","w").write(svg(W,H,"\n".join(b),"\n".join(defs)))
