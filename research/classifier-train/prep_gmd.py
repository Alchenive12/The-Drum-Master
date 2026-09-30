# File: research/classifier-train/prep_gmd.py
#
# Groove MIDI Dataset v1.0.0 (Magenta, CC BY 4.0 - commercial use allowed with attribution):
# converts each performance with audio into our label format next to a link to its WAV:
#   <out>/<id>.wav -> original audio,   <out>/<id>.txt: "<onset sample> <type>[,<type>]" per hit
# Roland TD-11 pitch map (GMD paper, Table 2) -> plugin drum types:
#   36 kick | 38,40 snare (head, rim-shot) | 37 rimshot (side stick) | 42,22,44 closedHat
#   46,26 openHat | 49,55,57,52 crash | 51,59,53 ride | toms 43,58,45,47,48,50 misc
# Usage: python3 prep_gmd.py <groove_dir> <out_dir>
import csv, os, sys
import mido
import soundfile as sf

MAP = {36: "kick", 38: "snare", 40: "snare", 37: "rimshot", 42: "closedHat", 22: "closedHat", 44: "closedHat",
       46: "openHat", 26: "openHat", 49: "crash", 55: "crash", 57: "crash", 52: "crash", 51: "ride", 59: "ride",
       53: "ride", 43: "misc", 58: "misc", 45: "misc", 47: "misc", 48: "misc", 50: "misc"}

def main():
    root, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    n = 0
    for row in csv.DictReader(open(os.path.join(root, "info.csv"))):
        if not row.get("audio_filename"):
            continue
        wav = os.path.join(root, row["audio_filename"])
        if not os.path.exists(wav):
            continue
        sr = sf.info(wav).samplerate
        mid = mido.MidiFile(os.path.join(root, row["midi_filename"]))
        t, hits = 0.0, []
        for msg in mid:          # iterating a MidiFile yields delta times in seconds (tempo map applied)
            t += msg.time
            if msg.type == "note_on" and msg.velocity > 0 and msg.note in MAP:
                hits.append((int(round(t * sr)), MAP[msg.note]))
        hits.sort()
        events = []
        for s, ty in hits:
            if events and s - events[-1][0] <= int(0.010 * sr):
                events[-1][1].add(ty)
            else:
                events.append([s, {ty}])
        name = row["id"].replace("/", "_")
        link = os.path.join(out, name + ".wav")
        if not os.path.exists(link):
            os.symlink(os.path.abspath(wav), link)
        with open(os.path.join(out, name + ".txt"), "w") as f:
            for s, types in events:
                f.write(f"{s} {','.join(sorted(types))}\n")
        n += 1
    print("performances:", n)

if __name__ == "__main__":
    main()
