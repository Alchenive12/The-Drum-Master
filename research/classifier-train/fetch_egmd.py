# File: research/classifier-train/fetch_egmd.py
#
# Downloads a balanced subset of the Expanded Groove MIDI Dataset (E-GMD v1.0.0, Magenta,
# CC BY 4.0: the same performances as Groove, re-recorded on 43 different Roland TD-17 kits,
# from acoustic to 808/909 and lo-fi). The full zip is ~90 GB, so this reads the zip's central
# directory with HTTP range requests and fetches only the chosen members.
#
# Usage: python3 fetch_egmd.py <out_dir> [per_kit=25] [split=train]
# Output: <out_dir>/<wav and midi as in the zip>, plus <out_dir>/subset.csv (rows of e-gmd csv).
import csv, os, random, struct, subprocess, sys, time, zlib

BASE = "https://storage.googleapis.com/magentadata/datasets/e-gmd/v1.0.0/"
ZIP = BASE + "e-gmd-v1.0.0.zip"

def get(start, end):
    for attempt in range(6):   # transient TLS / connection errors happen on long runs
        r = subprocess.run(["curl", "-sSf", "--retry", "4", "--retry-all-errors", "-r", f"{start}-{end}", ZIP],
                           capture_output=True)
        if r.returncode == 0 and len(r.stdout) == end - start + 1:
            return r.stdout
        time.sleep(2 ** attempt)
    raise RuntimeError(f"range {start}-{end} failed")

def size():
    out = subprocess.run(["curl", "-sSfI", ZIP], check=True, capture_output=True, text=True).stdout
    return int([l for l in out.splitlines() if l.lower().startswith("content-length")][-1].split(":")[1])

def central_directory(total):
    tail = get(total - 65536, total - 1)
    i = tail.rfind(b"PK\x06\x06")                      # zip64 end of central directory record
    if i >= 0:
        cd_size, cd_off = struct.unpack("<QQ", tail[i + 40:i + 56])
    else:
        i = tail.rfind(b"PK\x05\x06")
        cd_size, cd_off = struct.unpack("<II", tail[i + 12:i + 20])
    cd = get(cd_off, cd_off + cd_size - 1)
    entries, p = {}, 0
    while p + 46 <= len(cd) and cd[p:p + 4] == b"PK\x01\x02":
        method, = struct.unpack("<H", cd[p + 10:p + 12])
        csize, usize = struct.unpack("<II", cd[p + 20:p + 28])
        nlen, xlen, clen = struct.unpack("<HHH", cd[p + 28:p + 34])
        off, = struct.unpack("<I", cd[p + 42:p + 46])
        name = cd[p + 46:p + 46 + nlen].decode()
        extra = cd[p + 46 + nlen:p + 46 + nlen + xlen]
        q = 0
        while q + 4 <= len(extra):                     # zip64 extended information
            hid, hlen = struct.unpack("<HH", extra[q:q + 4])
            if hid == 1:
                vals, r = [], q + 4
                for flag in (usize, csize, off):
                    if flag == 0xFFFFFFFF:
                        vals.append(struct.unpack("<Q", extra[r:r + 8])[0]); r += 8
                    else:
                        vals.append(flag)
                usize, csize, off = vals
            q += 4 + hlen
        entries[name] = (method, csize, off)
        p += 46 + nlen + xlen + clen
    return entries

def fetch(entry):
    method, csize, off = entry
    head = get(off, off + 29)
    nlen, xlen = struct.unpack("<HH", head[26:30])
    data = get(off + 30 + nlen + xlen, off + 30 + nlen + xlen + csize - 1)
    return data if method == 0 else zlib.decompress(data, -15)

def main():
    out = sys.argv[1]
    per_kit = int(sys.argv[2]) if len(sys.argv) > 2 else 25
    split = sys.argv[3] if len(sys.argv) > 3 else "train"
    os.makedirs(out, exist_ok=True)
    meta = os.path.join(out, "e-gmd-v1.0.0.csv")
    if not os.path.exists(meta):
        subprocess.run(["curl", "-sSf", "-o", meta, BASE + "e-gmd-v1.0.0.csv"], check=True)
    rows = [r for r in csv.DictReader(open(meta)) if r["split"] == split and float(r["duration"]) >= 8.0]   # skip 1-bar fills
    rng = random.Random(7)
    byKit = {}
    for r in rows:
        byKit.setdefault(r["kit_name"], []).append(r)
    chosen = []
    for kit in sorted(byKit):
        chosen += rng.sample(byKit[kit], min(per_kit, len(byKit[kit])))
    entries = central_directory(size())
    prefix = next(n for n in entries if n.endswith(".wav")).split("drummer")[0]
    with open(os.path.join(out, "subset.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(chosen[0].keys()))
        w.writeheader()
        for k, r in enumerate(chosen):
            for key in ("midi_filename", "audio_filename"):
                dst = os.path.join(out, r[key])
                if os.path.exists(dst):
                    continue
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(dst + ".part", "wb") as g:
                    g.write(fetch(entries[prefix + r[key]]))
                os.replace(dst + ".part", dst)
            w.writerow(r)
            if k % 50 == 0:
                print(f"{k}/{len(chosen)}", flush=True)
    print("files:", len(chosen))

if __name__ == "__main__":
    main()
