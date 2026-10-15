# File: research/loop-learning/par_extract.py
# Runs extract_long (or extract) on a labelled set with N processes: splits the set's <name>.wav/.txt pairs
# into N folders of hard links (copies if links fail), runs the tool on each, and merges the CSVs (and the
# .missed.csv files) in order. Same rows as one process, just faster.
# Usage: python par_extract.py <tool.exe> <set_dir> <out.csv> [n=12]
import os, subprocess, sys, shutil

def main():
    tool, src, out = sys.argv[1], sys.argv[2], sys.argv[3]
    n = int(sys.argv[4]) if len(sys.argv) > 4 else 12
    names = sorted(f[:-4] for f in os.listdir(src) if f.endswith(".wav") and os.path.exists(os.path.join(src, f[:-4] + ".txt")))
    parts = os.path.join(os.path.dirname(os.path.abspath(out)), "_parts_" + os.path.basename(out))
    procs = []
    for k in range(n):
        d = os.path.join(parts, f"p{k:02d}"); os.makedirs(d, exist_ok=True)
        for nm in names[k::n]:
            for ext in (".wav", ".txt"):
                dst = os.path.join(d, nm + ext)
                if os.path.exists(dst): continue
                try: os.link(os.path.join(src, nm + ext), dst)
                except OSError: shutil.copyfile(os.path.join(src, nm + ext), dst)
        procs.append(subprocess.Popen([tool, d, os.path.join(parts, f"p{k:02d}.csv")]))
    codes = [p.wait() for p in procs]
    if any(codes): sys.exit(f"extract failed: {codes}")
    for suffix in ("", ".missed.csv"):
        with open(out + suffix, "w", newline="") as f:
            for k in range(n):
                lines = open(os.path.join(parts, f"p{k:02d}.csv" + suffix), newline="").read().splitlines(True)
                f.writelines(lines if k == 0 else lines[1:])
    shutil.rmtree(parts, ignore_errors=True)
    print(f"{len(names)} files -> {out}")

if __name__ == "__main__":
    main()
