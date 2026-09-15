# Contributing to Network Goblin ND-1

Thanks for taking a look! This is a hobby project built by a self-taught
tinkerer (with a lot of AI help as a learning aid), so contributions,
corrections, and "here's the right way to do this" feedback are all genuinely
welcome — no gatekeeping here.

## Ways to help

- **Build one and report back.** Photos, wiring notes, "worked / didn't work
  on my hardware" — all useful. Open an issue.
- **Calibration data.** If you have known-length cables, the linked-cable
  length (CBLN) estimate needs per-chip tuning. Raw `cbln` values (from the
  `cable` serial command) paired with true cable lengths help a lot.
- **Code improvements.** Cleaner LVGL, better error handling, the planned
  features (LLDP/CDP decode, settings persistence, etc.). See the roadmap in
  the README.


## Ground rules

- Keep it friendly. Everyone here is learning.
- Small, focused PRs are easier to review than giant ones.
- If you're changing hardware behavior, say what you tested it on.
- The firmware targets **Arduino IDE + esp32 core 3.x + LVGL 9.x**. Please
  keep new code within that toolchain unless we're deliberately moving.

## Good first issues

- Persist theme/settings across reboots (NVS/Preferences).
- Show traceroute hop addresses on-device.
- Fill out the Tools screen.
- Add on-screen custom ping/traceroute target entry.
- LLDP/CDP frame capture + decode (the big one).

- I will be adding better build documents at a later point when I am able to use both arms. 

## Reporting bugs

Include:
- What you did (steps).
- What you expected vs. what happened.
- Your hardware (ESP32 board, PHY, display).
- Serial output if relevant (the boot log + any error lines).

If it's a display or wiring issue, a photo is worth a thousand words.

Thanks for helping the goblin get better. 🧌
