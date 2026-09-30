# File: research/classifier-eval/export_sets.py
#
# Writes the two real-drum evaluation sets in one simple format for eval.cpp:
#   <out>/<set>/<name>.wav   mono float32 WAV
#   <out>/<set>/<name>.txt   one line per labelled hit: "<onset sample> <type>[,<type>...]"
# Types use the plugin's names: kick snare closedHat openHat crash ride rimshot clap misc.
#
#   acoustic   - MDB Drums drum-only tracks + subclass annotations (as in research/lookahead-test)
#   electronic - loops from held-out Dirt-Samples one-shots, generated exactly like
#                research/lookahead-test/lookahead_test.py (same seed), incl. claps and rimshots
#
# Usage: python3 export_sets.py <mdb_dir> <dirt_dir> <out_dir>

import os, sys
import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "lookahead-test"))
import lookahead_test as lt   # noqa: E402

NAMES = {"kick": "kick", "snare": "snare", "closed hat": "closedHat", "open hat": "openHat", "crash": "crash",
         "ride": "ride", "rimshot": "rimshot", "clap": "clap", "misc": "misc"}

def write(out_dir, name, x, events):
    os.makedirs(out_dir, exist_ok=True)
    sf.write(os.path.join(out_dir, name + ".wav"), x.astype(np.float32), lt.SR, subtype="FLOAT")
    with open(os.path.join(out_dir, name + ".txt"), "w") as f:
        for o, labs in events:
            f.write(f"{o} {','.join(sorted(NAMES[l] for l in labs))}\n")

def main():
    mdb_dir, dirt_dir, out = sys.argv[1], sys.argv[2], sys.argv[3]
    for name, x, events in lt.load_mdb(mdb_dir):
        write(os.path.join(out, "acoustic"), name, x, [(o, labs) for o, labs in events])
    loops = lt.make_loops(lt.load_oneshots(dirt_dir))
    for name, x, events, fold in loops[:40]:
        write(os.path.join(out, "electronic"), name, x, [(o, labs) for o, labs in events])
    print("done")

if __name__ == "__main__":
    main()
