# Time Rewind (working title) - Red Dead Redemption 2, story mode

Hold a key and Arthur and his horse run back along the path of the last seconds, like the rewind in Life is Strange. Let go and the game carries on from that moment.

**Status: source only.** The game-access layer is not written yet and nothing here has been run inside Red Dead Redemption 2.

## Planned first release
- Rewinds Arthur and his horse: position, heading, health, stamina, ammo in the clip.
- Hold the key to rewind in real time, release to continue from there. Key, rewind length, sample rate and speed are set in `RewindTime.ini`.
- Story mode only. The plugin switches itself off in Red Dead Online.
- Later versions: nearby NPCs and animals, world state (doors, objects, fire, weather), taking back dialogue choices.

## Needs
- Red Dead Redemption 2 (PC), story mode.
- Ultimate ASI Loader (installed by Melty). Script Hook RDR2 is not used.

## What is in this folder
- `sheets/` - the design and source of truth: entities, fields, hooks, config, states.
- `tools/preflight.py` - lists unfilled cells and references that do not resolve (`--release` also blocks on open items).
- `tools/gen.py` - generates `src/generated/*.h` from the sheets.
- `src/core.h` - rewind buffer and controller, independent of the game, unit tested.
- `src/plugin.cpp` - Windows ASI plugin shell; stays idle until every hook in `sheets/hooks.json` is verified.
- `tests/test_core.cpp`, `build.sh` - `./build.sh` runs the generator and the tests, `./build.sh asi` cross-builds `RewindTime.asi`.
