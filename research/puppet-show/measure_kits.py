# File: research/puppet-show/measure_kits.py
# Measures The Puppet Show's ratios (prompts/05) on real, isolated hits of the E-GMD kits
# (research/classifier-train/fetch_egmd.py + prep_gmd.py): per kit the median, then the median,
# 10th and 90th percentile over kits. Used to anchor the v1 kick targets (D-C15). In v2 the same
# measurement runs on the user's genre-named sample packs.
# Usage: python3 measure_kits.py <scratch_dir>   (expects <dir>/egmd/subset.csv and <dir>/evalsets/egmd)
import csv, os, sys, numpy as np, soundfile as sf, collections
S=sys.argv[1]
kit_of={os.path.splitext(r["audio_filename"])[0].replace("/","_"):r["kit_name"] for r in csv.DictReader(open(S+"/egmd/subset.csv"))}
def band(p,f,lo,hi): return p[(f>=lo)&(f<hi)].sum()
db=lambda a,b:10*np.log10(a/b+1e-12)
res=collections.defaultdict(lambda: collections.defaultdict(list))
WIN={"kick":0.15,"snare":0.15,"misc":0.15,"closedHat":0.06,"openHat":0.15,"crash":0.15,"ride":0.15}
for name,kit in kit_of.items():
    txt=f"{S}/evalsets/egmd/{name}.txt"
    if not os.path.exists(txt): continue
    if min(len(res[kit][m]) for m in ("M3","M5","M2")) >= 60 if res[kit] else False: continue
    ev=[(int(l.split()[0]),l.split()[1]) for l in open(txt) if l.strip()]
    x,sr=sf.read(f"{S}/evalsets/egmd/{name}.wav",dtype="float32")
    if x.ndim>1: x=x.mean(1)
    for i,(s,t) in enumerate(ev):
        if "," in t or t not in WIN: continue
        n=int(WIN[t]*sr); prev=ev[i-1][0] if i>0 else -10**9; nxt=ev[i+1][0] if i+1<len(ev) else 10**12
        if s-prev<0.03*sr or nxt-s<n or s+n>len(x): continue
        f=np.fft.rfftfreq(n,1/sr); p=np.abs(np.fft.rfft(x[s:s+n]*np.hanning(n)))**2
        tot=band(p,f,30,16000)
        if tot<=0: continue
        if t in ("kick","snare","misc"): res[kit]["M2"].append(db(band(p,f,200,500),tot))
        if t=="kick":
            res[kit]["M4"].append(db(band(p,f,2000,5000),tot))
            res[kit]["M1new"].append(db(band(p,f,30,90),band(p,f,90,16000)))
        if t=="snare": res[kit]["M3"].append(db(band(p,f,1500,5000),tot))
        if t in ("closedHat","openHat","crash","ride"): res[kit]["M5"].append(db(band(p,f,8000,16000),band(p,f,2000,16000)))
for m in ("M1new","M2","M3","M4","M5"):
    meds=[np.median(v[m]) for v in res.values() if len(v[m])>=5]
    print(f"{m:6} kit medians: median {np.median(meds):6.1f}  p10 {np.percentile(meds,10):6.1f}  p90 {np.percentile(meds,90):6.1f}  kits {len(meds)}")
