# File: research/snare-train/ogg2wav.py
# FL Studio's packs are RIFF/WAVE files whose data chunk is an Ogg Vorbis stream (format tag 0x674f),
# which dsp/tools/wav_io.h can't read. Decode them to plain 24-bit WAVs in a private cache and write a
# copy of the list with those entries pointing at the decoded files (pack and label unchanged).
# Usage: python3 ogg2wav.py <list.tsv> <out_list.tsv> <cache_dir>
import io, os, sys, hashlib
import soundfile as sf

src_list, out_list, cache = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(cache, exist_ok=True)
n = conv = 0
with open(out_list, "w") as out:
    for line in open(src_list, encoding="utf-8", errors="surrogateescape"):
        parts = line.rstrip("\n").split("\t"); path = parts[0]; n += 1
        try:
            b = open(path, "rb").read(64 * 1024 * 1024)
            if b[:4] == b"RIFF" and b"OggS" in b[:4096]:
                x, sr = sf.read(io.BytesIO(b[b.index(b"OggS"):]), dtype="float32")
                dst = os.path.join(cache, hashlib.md5(path.encode("utf-8", "surrogateescape")).hexdigest() + ".wav")
                if not os.path.exists(dst): sf.write(dst, x, sr, subtype="PCM_24")
                parts[0] = dst; conv += 1
        except Exception as e:
            print("skip", path, e, file=sys.stderr)
        out.write("\t".join(parts) + "\n")
print(n, "entries,", conv, "converted from Ogg-in-WAV")
