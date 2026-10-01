from lib import *
rnd=random.Random(11)
W,H=300,380
FACE="M150 8 C230 8 289 72 289 170 C289 262 238 350 150 372 C62 350 11 262 11 170 C11 72 70 8 150 8 Z"
defs=[f'<clipPath id="face"><path d="{FACE}"/></clipPath>']
for r in (1,2,3,5,8,14): defs.append(f'<filter id="b{r}" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="{r}"/></filter>')
defs.append(noise("pore","0.6",3,2,"#5a5040","1.6","-0.82",0,0,W,H))
defs.append(noise("grime","0.012 0.02",4,7,"#2a2218","1.5","-0.62",0,0,W,H))
defs.append(noise("blushN","0.05",3,9,"#8e1c24","1.8","-0.75",0,0,W,H))
defs.append(noise("bisqueN","0.25",3,13,"#3d3022","1.6","-0.6",0,0,W,H))
defs.append(f'''<radialGradient id="porc" cx="118" cy="120" r="250" gradientUnits="userSpaceOnUse">
<stop offset="0" stop-color="#f6efe1"/><stop offset="0.35" stop-color="#e6dcc6"/><stop offset="0.7" stop-color="#b3a892"/><stop offset="1" stop-color="#5f574a"/></radialGradient>
<radialGradient id="cool" cx="60" cy="90" r="200" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#bcd0e4" stop-opacity="0.35"/><stop offset="1" stop-color="#bcd0e4" stop-opacity="0"/></radialGradient>
<radialGradient id="sock" cx="0.5" cy="0.62" r="0.6"><stop offset="0" stop-color="#000"/><stop offset="0.75" stop-color="#050404"/><stop offset="1" stop-color="#2a241d"/></radialGradient>
<linearGradient id="bisque" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#8a7358"/><stop offset="1" stop-color="#3f3226"/></linearGradient>''')
b=[]
b.append(f'<path d="{FACE}" fill="#000" fill-opacity="0.55" transform="translate(6 10)" filter="url(#b8)"/>')
b.append(f'<path d="{FACE}" fill="url(#porc)"/>')
g=[]  # everything clipped to the face
g.append(f'<rect width="{W}" height="{H}" fill="url(#cool)"/>')
# form shading: temples, under-cheekbones, chin
g.append('<ellipse cx="282" cy="200" rx="40" ry="150" fill="#2b261f" fill-opacity="0.45" filter="url(#b14)"/>')
g.append('<ellipse cx="18" cy="200" rx="22" ry="140" fill="#2b261f" fill-opacity="0.25" filter="url(#b14)"/>')
g.append('<path d="M60 230 C90 250 110 240 120 222" stroke="#5f574a" stroke-opacity="0.35" stroke-width="16" fill="none" filter="url(#b8)"/>')
g.append('<path d="M240 230 C210 250 190 240 180 222" stroke="#3f392f" stroke-opacity="0.45" stroke-width="16" fill="none" filter="url(#b8)"/>')
g.append('<ellipse cx="150" cy="352" rx="70" ry="22" fill="#3a342a" fill-opacity="0.35" filter="url(#b8)"/>')
g.append('<ellipse cx="74" cy="196" rx="30" ry="14" fill="#fffaf0" fill-opacity="0.25" filter="url(#b8)"/>')
g.append('<ellipse cx="226" cy="196" rx="30" ry="14" fill="#fffaf0" fill-opacity="0.12" filter="url(#b8)"/>')
g.append('<ellipse cx="150" cy="235" rx="60" ry="20" fill="#3a342a" fill-opacity="0.18" filter="url(#b8)"/>')
# nose bridge relief
g.append('<path d="M150 150 C146 175 143 190 146 202" stroke="#fff8ea" stroke-opacity="0.5" stroke-width="5" fill="none" filter="url(#b3)"/>')
g.append('<path d="M156 152 C160 176 162 192 158 204" stroke="#6b6252" stroke-opacity="0.45" stroke-width="5" fill="none" filter="url(#b3)"/>')
g.append('<path d="M138 206 C144 212 156 212 162 206" stroke="#4a4236" stroke-opacity="0.7" stroke-width="2.4" fill="none" stroke-linecap="round" filter="url(#b1)"/>')
# faded blush
for cx in (80,220):
    g.append(f'<ellipse cx="{cx}" cy="222" rx="36" ry="24" fill="#a3242d" fill-opacity="0.2" filter="url(#b8)"/>')
defs.append('<clipPath id="cheeks"><ellipse cx="80" cy="222" rx="34" ry="22"/><ellipse cx="220" cy="222" rx="34" ry="22"/></clipPath>')
# eye sockets: carved, deep, with a lit lower rim
for side in (-1,1):
    ex=150+side*62
    eye=f"M{ex-side*52} 152 C{ex-side*38} 128 {ex+side*2} 126 {ex+side*24} 146 C{ex+side*6} 168 {ex-side*36} 170 {ex-side*52} 152 Z"
    g.append(f'<path d="{eye}" fill="#000" fill-opacity="0.45" transform="translate(0 -4)" filter="url(#b5)"/>')
    g.append(f'<path d="{eye}" fill="url(#sock)"/>')
    g.append(f'<path d="M{ex-side*48} 156 C{ex-side*30} 170 {ex+side*6} 166 {ex+side*22} 150" stroke="#fff6e4" stroke-opacity="0.55" stroke-width="1.6" fill="none" filter="url(#b1)"/>')
    g.append(f'<path d="M{ex-side*54} 140 C{ex-side*36} 116 {ex+side*6} 114 {ex+side*30} 134" stroke="#3a3328" stroke-opacity="0.3" stroke-width="6" fill="none" filter="url(#b3)"/>')
    # dark tear stains running from the inner corners
    tx=ex+side*18
    path=f"M{tx} 160 C{tx+side*5} 182 {tx-side*4} 200 {tx+side*2} 222 S{tx-side*2} 240 {tx+side*1} 250"
    g.append(f'<path d="{path}" stroke="#1d1712" stroke-opacity="0.3" stroke-width="7" fill="none" stroke-linecap="round" filter="url(#b2)"/>')
    g.append(f'<path d="{path}" stroke="#1d1712" stroke-opacity="0.5" stroke-width="1.2" fill="none" stroke-linecap="round"/>')
    g.append(f'<circle cx="{tx+side*1}" cy="251" r="2.4" fill="#1d1712" fill-opacity="0.55"/>')
# painted brows, worn almost away
for side in (-1,1):
    ex=150+side*62
    g.append(f'<path d="M{ex-side*44} 118 Q{ex-side*14} 100 {ex+side*20} 110" stroke="#2a221a" stroke-opacity="0.28" stroke-width="3" fill="none" stroke-linecap="round" filter="url(#b1)"/>')
# the slit smile: carved gap with depth
def q(t,p0,p1,p2): return ((1-t)**2*p0[0]+2*(1-t)*t*p1[0]+t*t*p2[0], (1-t)**2*p0[1]+2*(1-t)*t*p1[1]+t*t*p2[1])
P0,P1,P2=(66,250),(150,306),(234,250)
g.append(f'<path d="M66 250 Q150 306 234 250" stroke="#7a1c22" stroke-opacity="0.35" stroke-width="12" fill="none" filter="url(#b5)"/>')
g.append(f'<path d="M66 250 Q150 306 234 250" stroke="#050303" stroke-width="5" fill="none" stroke-linecap="round"/>')
g.append(f'<path d="M68 254 Q150 311 232 254" stroke="#fff4e0" stroke-opacity="0.55" stroke-width="1.4" fill="none"/>')
g.append(f'<path d="M66 246 Q150 300 234 246" stroke="#3a3126" stroke-opacity="0.6" stroke-width="1.2" fill="none" filter="url(#b1)"/>')
g.append('<path d="M66 250 C58 244 56 236 62 230" stroke="#050303" stroke-width="3" fill="none" stroke-linecap="round"/>')
g.append('<path d="M234 250 C242 244 244 236 238 230" stroke="#050303" stroke-width="3" fill="none" stroke-linecap="round"/>')
# stitches: waxed thread crossing the slit, puncture holes with rust halos
st=[]
for i in range(14):
    t=0.05+0.9*i/13; x,y=q(t,P0,P1,P2)
    dx,dy=2*(1-t)*(P1[0]-P0[0])+2*t*(P2[0]-P1[0]), 2*(1-t)*(P1[1]-P0[1])+2*t*(P2[1]-P1[1])
    L=math.hypot(dx,dy); nx,ny=-dy/L,dx/L
    a=(x+nx*10-dx/L*2.5, y+ny*10-dy/L*2.5); c=(x-nx*10+dx/L*2.5, y-ny*10+dy/L*2.5)
    for hx,hy in (a,c):
        st.append(f'<circle cx="{hx:.1f}" cy="{hy:.1f}" r="4" fill="#6e1a1e" fill-opacity="0.3" filter="url(#b2)"/>')
        st.append(f'<circle cx="{hx:.1f}" cy="{hy:.1f}" r="1.7" fill="#0a0605"/>')
    st.append(f'<line x1="{a[0]+1:.1f}" y1="{a[1]+1.5:.1f}" x2="{c[0]+1:.1f}" y2="{c[1]+1.5:.1f}" stroke="#000" stroke-opacity="0.45" stroke-width="3.2" stroke-linecap="round" filter="url(#b1)"/>')
    st.append(f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{c[0]:.1f}" y2="{c[1]:.1f}" stroke="#2b1d16" stroke-width="2.6" stroke-linecap="round"/>')
    st.append(f'<line x1="{a[0]-0.6:.1f}" y1="{a[1]-0.4:.1f}" x2="{c[0]-0.6:.1f}" y2="{c[1]-0.4:.1f}" stroke="#8a6d55" stroke-opacity="0.6" stroke-width="0.8" stroke-linecap="round"/>')
g+=st
# hairline crazing across the glaze
for _ in range(40):
    x=rnd.uniform(30,270); y=rnd.uniform(40,350); p=[(x,y)]
    for _ in range(rnd.randint(2,5)): p.append((p[-1][0]+rnd.uniform(-14,14), p[-1][1]+rnd.uniform(-14,14)))
    g.append(f'<path d="{d_of(p)}" stroke="#4a4236" stroke-opacity="{rnd.uniform(0.12,0.28):.2f}" stroke-width="0.5" fill="none"/>')
# the chipped crack: a missing shard of glaze showing the bisque beneath, crack running up from it
chip=jag([(176,352),(170,330),(186,318),(198,326),(196,346),(186,360),(176,352)],1.6,rnd,3)
g.append(f'<path d="{d_of(chip,True)}" fill="#000" fill-opacity="0.5" transform="translate(1 2)" filter="url(#b1)"/>')
g.append(f'<path d="{d_of(chip,True)}" fill="url(#bisque)"/>')
defs.append(f'<clipPath id="chipC"><path d="{d_of(chip,True)}"/></clipPath>')
g.append(f'<rect width="{W}" height="{H}" filter="url(#bisqueN)" clip-path="url(#chipC)" opacity="0.6"/>')
g.append(f'<path d="{d_of(chip,True)}" fill="none" stroke="#000" stroke-opacity="0.6" stroke-width="3" clip-path="url(#chipC)" filter="url(#b1)"/>')
g.append(f'<path d="{d_of(chip,True)}" fill="none" stroke="#fffaf0" stroke-opacity="0.6" stroke-width="0.8" transform="translate(-0.6 -0.6)"/>')
crack=jag([(180,322),(172,300),(186,280),(176,258),(190,232),(184,212)],2.0,rnd,3)
g.append(f'<path d="{d_of(crack)}" stroke="#0e0b08" stroke-width="1.6" fill="none" stroke-linejoin="round"/>')
g.append(f'<path d="{d_of([(x+1.2,y+0.6) for x,y in crack])}" stroke="#fff6e4" stroke-opacity="0.5" stroke-width="0.6" fill="none"/>')
br=jag([(186,280),(206,270),(214,254)],1.5,rnd,3); g.append(f'<path d="{d_of(br)}" stroke="#0e0b08" stroke-width="1" fill="none"/>')
br=jag([(176,258),(160,250)],1.2,rnd,3); g.append(f'<path d="{d_of(br)}" stroke="#0e0b08" stroke-width="0.8" fill="none"/>')
# grime in the low places + porcelain pores
g.append(f'<rect width="{W}" height="{H}" filter="url(#grime)" opacity="0.28"/>')
g.append(f'<rect width="{W}" height="{H}" filter="url(#pore)" opacity="0.22"/>')
# edge darkening (thickness of the shell) and specular highlights from the cold window light
g.append(f'<path d="{FACE}" fill="none" stroke="#2a251e" stroke-opacity="0.6" stroke-width="14" filter="url(#b5)"/>')
g.append('<ellipse cx="108" cy="70" rx="44" ry="26" fill="#ffffff" fill-opacity="0.35" transform="rotate(-25 108 70)" filter="url(#b8)"/>')
g.append('<ellipse cx="98" cy="66" rx="16" ry="7" fill="#ffffff" fill-opacity="0.4" transform="rotate(-25 98 66)" filter="url(#b2)"/>')
g.append('<ellipse cx="70" cy="200" rx="12" ry="7" fill="#ffffff" fill-opacity="0.35" filter="url(#b3)"/>')
g.append('<ellipse cx="142" cy="182" rx="3" ry="10" fill="#ffffff" fill-opacity="0.45" filter="url(#b2)"/>')
g.append('<path d="M40 120 C48 80 80 40 120 26" stroke="#ffffff" stroke-opacity="0.35" stroke-width="2" fill="none" filter="url(#b1)"/>')
b.append(f'<g clip-path="url(#face)">{"".join(g)}</g>')
b.append(f'<path d="{FACE}" fill="none" stroke="#2a251e" stroke-width="1.2"/>')
open("mask.svg","w").write(svg(W,H,"\n".join(b),"\n".join(defs)))
