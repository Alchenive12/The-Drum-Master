import math, random
C = dict(night="#0a090c", stage="#16121a", stageLight="#2a2230", velvet="#5a0f16", velvetDark="#2a0509",
         bone="#e6dcc6", boneShadow="#9d9180", gold="#c9a45c", goldDim="#7d6636", blood="#b01f2a",
         glow="#8fd18a", wood="#4a3322", woodLight="#7a5a3c", moon="#a9bccf", oldShell="#3a3f4a",
         wall="#1c222c", wallDark="#0d1016", floor="#1a1d22", oldMetal="#7c8794")
def svg(w,h,body,defs=""):
    return f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{w}" height="{h}" viewBox="0 0 {w} {h}">\n<defs>\n{defs}\n</defs>\n{body}\n</svg>\n'
def hex2rgb(h): h=h.lstrip('#'); return tuple(int(h[i:i+2],16)/255 for i in (0,2,4))
def noise(fid, freq, oct_, seed, color, a_mul, a_off, x, y, w, h, kind="fractalNoise", extra=""):
    r,g,b = hex2rgb(color)
    return (f'<filter id="{fid}" filterUnits="userSpaceOnUse" x="{x}" y="{y}" width="{w}" height="{h}" color-interpolation-filters="sRGB">'
            f'<feTurbulence type="{kind}" baseFrequency="{freq}" numOctaves="{oct_}" seed="{seed}" result="n"/>'
            f'<feColorMatrix in="n" type="matrix" values="0 0 0 0 {r:.3f} 0 0 0 0 {g:.3f} 0 0 0 0 {b:.3f} {a_mul} 0 0 0 {a_off}" result="c"/>{extra}</filter>')
def jag(points, amp, rnd, steps=6):
    """polyline through points with random jitter for torn/cracked edges"""
    out=[]
    for (x1,y1),(x2,y2) in zip(points, points[1:]):
        for s in range(steps):
            t=s/steps; out.append((x1+(x2-x1)*t+rnd.uniform(-amp,amp), y1+(y2-y1)*t+rnd.uniform(-amp,amp)))
    out.append(points[-1]); return out
def pts(p): return " ".join(f"{x:.1f},{y:.1f}" for x,y in p)
def d_of(p, close=False): return "M"+" L".join(f"{x:.1f} {y:.1f}" for x,y in p)+(" Z" if close else "")
