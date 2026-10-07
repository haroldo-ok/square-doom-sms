# SquareDoom — Sega Master System conversion

A port of Steve McCrea's Commodore 64 **SquareDoom** (6502 assembly) to the
Sega Master System, written in C + Z80 assembly with devkitSMS / SMSlib.
The title screen, menus, logo, weapons and fonts are taken from id Software's
**Doom 1.9 shareware** (`DOOM1.WAD`) so the presentation is closer to the
original Doom.
Everything in the C64 game is converted: all 9 maps (E1M9 is empty in the
original data too), the sector raycaster, monsters, weapons, pickups, doors,
lifts, stairs, switches, closets, secrets, damage floors, flashing lights,
automap, cheats, title / menus / story / credits / ending pages, intermission
tally, screen melt, the 18 PC-speaker sound effects and the `e1m1.sid` tune.

`squaredoom.sms` — 1 MiB (53 banks), Sega mapper, NTSC and PAL (auto-detected).

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
Pillow, numpy and py65, the original C64 sources checked out next to this
directory (`../SquareDoom`, or set `SQUAREDOOM_SRC`), and the Doom 1.9
shareware `DOOM1.WAD` in `assets/` (or set `DOOM_WAD`).

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
  code bank; per level a *cast bank* (map + per-crossing table + a full
  projection table + sky/switch textures), a *render bank* (map + sprite
  textures) and a *logic bank* (map + enemy/level code + the level's start-up
  data). The map sits at 0x8000 in all three, so map reads work whichever is
  paged in. Two *ray banks* hold the 32 columns' DDA steps for every view angle.

## Speed

Measured with `tools/fps.sh` (E1M1 start view, the heaviest) and
`tools/fps_walk.sh` (a scripted walk through E1M1), NTSC:

| | start view | walk |
|---|---|---|
| first complete build | 18.0 vblanks/frame (3.3 fps) | — |
| this version | 12.8 (4.7 fps) | 6.9 (8.7 fps) |

What made the difference:
* **Column caster in Z80** (`src/cast.s`): register-resident DDA (`dda_run`), one
  pass per sector crossing with ytop/ybot in registers, page-aligned clip
  stacks, cached sector heights.
* **Big-ROM tables**: a per-level projection table gives the screen row of any
  height difference at any distance in one indexed load (no multiplies);
  per-crossing info (row table, wall light falloff, clip depth) comes from one
  4-byte record; the ray banks replace 64 multiplies whenever the view turns.
* **Deferred flip**: the frame is queued and the frame interrupt swaps the
  name table, sprite table and border colour, so the next frame starts
  computing immediately instead of waiting for VBlank.
* **Sprites**: things are only projected when their sector was reached by a
  ray this frame (as Doom renders things in visible subsectors), early
  field-of-view rejection, reciprocal tables instead of divisions, and the
  column/texel loop in Z80.
* Weapon overlay records precomputed per VRAM tile bank and copied in Z80.
* **Sound.** PC-speaker effects (VicDoom lumps) play on PSG tone 2, resampled
  to 60 Hz. The SID tune was converted by running its 6502 player (py65) and
  capturing the register writes: voices 1–2 → PSG tones 0–1 with an ADSR
  model, noise waveform → PSG noise, looped. The C64 build leaves music stubbed
  out; here it plays in game and in the menus (Music volume 0 turns it off).

## Doom 1.9 presentation

* **Title screen**: `TITLEPIC`, scaled to 256x192 and converted with both SMS
  palettes (32 colours, chosen per tile) and similar-tile merging to fit 440 tiles.
* **Menus**: Doom's `m_menu.c` layout with the original graphics — `M_DOOM`
  logo, `M_NGAME`/`M_OPTION`/`M_RDTHIS`/`M_QUITG`, `M_EPISOD` + `M_EPI1-3`,
  `M_NEWG`/`M_SKILL` + skill names, `M_SVOL` sound menu with `M_SFXVOL`/`M_MUSVOL`
  and the `M_THERM*` sliders (the knob is a sprite), and the blinking `M_SKULL1/2`
  cursor. "Controls" and "Credits" are set in Doom's menu font, harvested letter by
  letter from the other menu graphics (`tools/doomfont.py`).
* **Weapons**: Doom's psprites (`PUNG`, `SAWG`, `PISG`/`PISF`, `SHTG`/`SHTF`,
  `CHGG`/`CHGF`, `MISG`/`MISF`) placed with Doom's full-screen offsets and
  scaled 0.8 x 0.85. Fully opaque cells are background tiles, edge cells are
  sprites (at most 8 per line). A pose's tiles are uploaded when the pose
  changes, into whichever of two VRAM tile banks the visible frame is not using.
* **Fonts**: in-game messages and HUD numbers use Doom's `STCFN` message font
  (red shades); keys use `STKEYS`; the story/credits/ending/controls pages use
  `STCFN` too. The intermission uses the `WI*` graphics (level names `WILV0x`,
  "Entering", "Finished", Kills / Items / Secret / Time / Par, `WINUM` digits).
* **Wall lighting**: walls darken with distance in every sector (about one
  dither step per 0.8 tiles on top of the sector light level, levelling off
  just before black), like Doom's diminishing light. The C64 version skipped
  distance shading for full-bright sectors.

## Differences from the C64 version

* 32-column view instead of 40 (see above); messages longer than 32
  characters wrap to a second line.
* Keyboard controls mapped to the pad; cheats are pad codes on the automap.
* No mouse option (no SMS equivalent); *Quit* returns to the title screen.
* Menus have no "Back" entries (as in Doom); button 2 goes back.
* Title, menus, weapons, fonts and intermission graphics come from Doom 1.9
  (see above) instead of the C64 art; text pages are reflowed to 30 columns in
  a fixed-width layout of Doom's STCFN font.
* Speed: about 8.7 frames per second on average while walking through E1M1
  (NTSC; 6.9 TV frames per redraw), 4.7 fps at the heaviest view (the E1M1 start).

## Tests (`tests/*.tmpl`, run with `make playtest`)

`playtest` (pad only: title → menu → new game → move/turn/strafe/fire →
automap → quit), `fixtures` (door, exit switch → tally → E1M2, cheats, weapon
cycling), `death` (killed by a monster, restart), `pick` (pickups, barrel
explosion), `sky`, `menus` (options, controls, story, credits, quit), `ending`
(finish E1M8 → ending pages → menu), `l2` (level 2 load), `weapons` (every
weapon idle and firing, rocket ammo spent).
