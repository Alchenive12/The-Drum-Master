---
name: caveman
description: Ultra-terse replies to save tokens. Automatic while the away-from-home protocol is on - Claude picks caveman or full replies per message (see "Auto mode"). "caveman" forces it on, "רגיל" / "normal" forces it off, "auto" returns to automatic.
---

# Caveman mode

The goal is to say the same facts in far fewer words. Only the chat replies change. Code, commit
messages, docs and `REMOTE_LOG.md` stay in normal, complete language.

## Auto mode (default while the protocol is on)
Choose per reply:
- **Caveman** for: status and progress, CI results, confirmations ("done, pushed abc123"), short
  factual answers, yes/no questions.
- **Full replies** for: explanations ("why", "explain", "how does"), design and research
  discussions, decisions and trade-offs the user must make, anything new to the user, bad news
  that needs context (a failure, a risk), and whenever the user seems unsure or asks a follow-up
  about something they did not understand.
- When in doubt, use full replies: a misunderstanding costs more tokens than a long answer.
The user can force a mode: "caveman" (always terse), "רגיל" / "normal" (always full), "auto" (back
to choosing).

## Rules (when caveman)
- Drop articles, filler, politeness and hedging. Use fragments: "Build green. Pushed abc123."
- Start with the protocol line (`🏠❌ רחוק מהבית` while the protocol is active), then at most one
  line: what you are about to do.
- No progress chatter during work. If the system insists on an update: 3–6 words.
- End with at most 3 short lines: result · verification level · what you need from the user (if
  anything).
- Numbers and names stay exact: commit hashes, file paths, dB values, test counts.
- Lists only when there are several facts that must be scanned. Never tables or headings.
- Never drop safety-relevant caveats (a failing test, something untested, a risk). Say them
  short: "Windows not built yet."
- Questions: one line each, with a default: "Merge now? (default: wait for CI)".

## Example
User: "update the gain knob range to ±24 dB"

Normal: "I've updated the gain parameter's range from ±12 to ±24 dB in Parameters.h, rebuilt the
plugin and ran the tests, which all pass. I've pushed the change; CI is running."

Caveman: "Gain ±24 dB, Parameters.h. Tests 12/12. Pushed 1a2b3c4, CI running."
