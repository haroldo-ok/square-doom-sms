# SquareDoom — Sega Master System conversion

A port of Steve McCrea's Commodore 64 **SquareDoom** (6502 assembly) to the
Sega Master System, written in C + Z80 assembly with devkitSMS / SMSlib.
Everything in the C64 game is converted: all 9 maps (E1M9 is empty in the
original data too), the sector raycaster, monsters, weapons, pickups, doors,
lifts, stairs, switches, closets, secrets, damage floors, flashing lights,
automap, cheats, title / menus / story / credits / ending pages, intermission
tally, screen melt, the 18 PC-speaker sound effects and the `e1m1.sid` tune.

`squaredoom.sms` — 512 KiB, Sega mapper, NTSC and PAL (auto-detected).

## Controls (standard 2-button pad)

| Action | Pad |
|---|---|
| move forward / back | D-pad up / down |
| turn | D-pad left / right |
| strafe | hold 2 + left / right |
| use (doors, switches) | tap 2 |
| fire | 1 |
| next / previous weapon | hold 2 + up / down |
| automap | PAUSE |
| quit to menu | on the automap, hold 1 + 2 for a second |
| restart after death | 2 |

Menus: D-pad moves / adjusts volumes, 1 selects, 2 goes back.

**Cheats** (type on the automap): left, right, left, right, then
`1 1 2` god mode (iddqd) · `2 2 1` keys + full ammo (idkfa) ·
`1 2 2` change level (idclev): up/down picks E1M1–E1M9, 1 confirms.

## Building

Needs SDCC + devkitSMS (`setup_toolchain.sh` from the skill), Python 3 with
Pillow, numpy and py65, and the original C64 sources checked out next to this
directory (`../SquareDoom`, or set `SQUAREDOOM_SRC`).

    make            # converts all assets from the C64 sources, builds squaredoom.sms
    make playtest   # runs every headless smstest script in tests/

The build is reproducible (byte-identical ROM from a clean checkout).

## How the conversion works

* **Screen.** The C64 draws its 3D view as 40×25 character cells, each a dither
  pattern in one colour. The SMS does the same with 32×24 tiles: the view shows
  the central 32 C64 columns with identical per-cell proportions (same column
  angle step and vertical projection), so the picture is the C64 picture with
  the outer 4 columns cropped. HUD on the bottom row as on the C64.
* **Colours.** Each dither tile is drawn in palette index *k*; the background
  palette and the sprite palette hold a different C64 colour at *k*, and the
  tile's palette bit picks one — so 6 tile sets cover 12 dithered colours and
  the whole view needs 189 tiles. Name tables are double buffered
  (0x3800 / 0x3000) and swapped in VBlank.
* **Renderer.** Ray DDA, sector crossings, flats/walls/ledges/door jambs/lift
  faces, sky strip, switch texture and the per-column clip stack follow the C64
  `render_*.asm` rules (rows = ⌈Δh·4/perp-tiles⌉, wall darkness from sector
  light + distance). The hot loop is Z80 assembly (`src/cast.s`,
  `src/castasm.c`); billboards (enemies, items, missiles, explosions) are
  clipped against the stack exactly like `render_items.asm`.
* **Weapons.** The C64 weapons are stacks of expanded hardware sprites. They
  are composited per pose at C64 pixel scale; fully opaque 8×8 cells become
  background tiles in the frame buffer, edge cells become sprites (never more
  than 8 per line). The C64 highlight colour (light level / muzzle flash) is a
  live palette entry.
* **Game logic** (enemy AI, projectiles, hitscan line of sight, barrels,
  doors/lifts/stairs processes, triggers, pickups, damage and armour rules,
  timings in ms) is a line-by-line port of the 6502 routines.
* **Banking.** Fixed code in banks 0–1; front end (menus, text, tally) in a
  code bank; per level a *render bank* (map at 0x8000 + textures + tables) and
  a *logic bank* (the same map at 0x8000 + enemy/level code), so map reads
  work whichever is paged in.
* **Sound.** PC-speaker effects (VicDoom lumps) play on PSG tone 2, resampled
  to 60 Hz. The SID tune was converted by running its 6502 player (py65) and
  capturing the register writes: voices 1–2 → PSG tones 0–1 with an ADSR
  model, noise waveform → PSG noise, looped. The C64 build leaves music stubbed
  out; here it plays in game and in the menus (Music volume 0 turns it off).

## Differences from the C64 version

* 32-column view instead of 40 (see above); messages longer than 32
  characters wrap to a second line.
* Keyboard controls mapped to the pad; cheats are pad codes on the automap.
* No mouse option (no SMS equivalent); *Quit* returns to the title screen.
* Menu and story text use the menu font at a fixed width instead of the
  C64's proportional bitmap text; pages are reflowed to 30 columns.
* Speed: about 3–4.5 frames per second depending on the view (the C64 runs at
  a similar handful of frames per second).

## Tests (`tests/*.tmpl`, run with `make playtest`)

`playtest` (pad only: title → menu → new game → move/turn/strafe/fire →
automap → quit), `fixtures` (door, exit switch → tally → E1M2, cheats, weapon
cycling), `death` (killed by a monster, restart), `pick` (pickups, barrel
explosion), `sky`, `menus` (options, controls, story, credits, quit), `ending`
(finish E1M8 → ending pages → menu), `l2` (level 2 load).
