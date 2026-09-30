# File: research/puppet-show/average_targets.py
# Averages the answers to prompts/05-puppet-show-genre-targets.md cell by cell (equal weight).
# Usage: python3 average_targets.py prompts/answers/05-claude.md prompts/answers/05-agent1.md ...
import re, sys, statistics

GENRES = ["Rock", "Pop", "EDM", "Metal", "Hip-hop"]
KNOBS = ["Bass boost (kick)", "Mud control (bodies)", "Mid presence (snare, clap)",
         "Mid presence (kick click)", "High control (hats, cymbals)"]

def tables(path):
    text = open(path).read()
    out, cur = {}, None
    for line in text.splitlines():
        m = re.match(r"TABLE (\d)", line)
        if m: cur = int(m.group(1)); out[cur] = {}; continue
        if cur and line.startswith("|") and not set(line) <= set("|-: "):
            cells = [c.strip() for c in line.strip("|").split("|")]
            try: out[cur][cells[0]] = [abs(float(c)) if cur == 3 else float(c) for c in cells[1:] if re.match(r"^[+-]?\d", c)]
            except ValueError: pass
    return out

answers = [tables(p) for p in sys.argv[1:] if not p.startswith("--")]
def avg(t, row):
    rows = [a[t][row] for a in answers]
    return [statistics.mean(v) for v in zip(*rows)], [max(v) - min(v) for v in zip(*rows)]

print("TABLE 1 - TARGETS (dB), mean | spread (max-min) across answers")
for g in GENRES:
    m, s = avg(1, g); print(f"| {g:<8}| " + " | ".join(f"{a:6.1f}" for a in m) + " |   spread " + " ".join(f"{x:4.1f}" for x in s))
print("\nTABLE 2 - TOLERANCE (dB), mean")
for g in GENRES:
    m, _ = avg(2, g); print(f"| {g:<8}| " + " | ".join(f"{a:4.1f}" for a in m) + " |")
print("\nTABLE 3 - KNOBS: span, max boost, max cut (dB), mean")
for k in KNOBS:
    m, _ = avg(3, k); print(f"| {k:<29}| " + " | ".join(f"{a:4.1f}" for a in m) + " |")

# ---------------------------------------------------------------------------------------------
# v1 anchoring (docs/03-decisions-for-review.md D-C15): measured kit medians from 43 real E-GMD
# kits (research/puppet-show/measure_kits.py). Where the answers are physically impossible (M1,
# M4 - a kick's energy is almost all below 90 Hz), keep the answers' differences BETWEEN genres
# and move the whole row so its mean sits on the measured median. M1 is redefined as sub vs the
# rest (30-90 Hz / 90 Hz-16 kHz) because "sub / total" saturates near 0 dB for real kicks; each
# answer is converted to the new unit before averaging.
import math
MEASURED = {"M1": 7.9, "M4": -31.4}
def m1_new(old_db):
    r = min(10 ** (old_db / 10.0), 0.999)
    return 10 * math.log10(r / (1 - r))
if "--anchor" in sys.argv[1:] or True:
    col = {"M1": 0, "M4": 3}
    print("\nV1 ANCHORED KICK TARGETS (dB)")
    for m, c in col.items():
        per_genre = []
        for g in GENRES:
            vals = [a[1][g][c] for a in answers]
            if m == "M1": vals = [m1_new(v) for v in vals]
            per_genre.append(statistics.mean(vals))
        mean = statistics.mean(per_genre)
        print(m, " ".join(f"{g}:{MEASURED[m] + v - mean:+.1f}" for g, v in zip(GENRES, per_genre)))
