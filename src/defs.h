/* SquareDoom for Sega Master System - shared definitions.
 * Port of Steve McCrea's C64 SquareDoom (6502 asm) to C / devkitSMS. */
#ifndef DEFS_H
#define DEFS_H
#include "SMSlib.h"
#include "fixed.h"
#include "sound_data.h"

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
typedef signed int s16;
typedef unsigned long u32;
typedef signed long s32;

/* ---- screen ---- */
#define VIEW_W 32
#define VIEW_H 24
#define HORIZON 12
#define CENTER_COL 15
#define MUZZLE_COL 16
#define AIM_COL_SLACK 2
#define HUD_ROW 23

/* ---- level layout (cooked .bin, see editor/js/io.js) ---- */
#define MAP_SIZE 32
#define SEC_N 200
/* The level map sits at 0x8000 in both the render bank and the logic bank of
 * each level, so map reads work whichever of the two is paged into slot 2. */
#define LV_MAP      ((const u8 *)0x8000)
#define RENDER_BANK (BANK_RENDER0 + level_num - 1)
#define LOGIC_BANK  (BANK_LOGIC0 + level_num - 1)
extern u8 spawn_xya[3];
#define SKY_COLS    ((const u8 *)SKY_ADDR)
#define SWITCH_TEX  ((const u8 *)SWITCH_ADDR)

/* sector type packing: action[4:0] | trigger[6:5] | single_shot[7] */
#define ACT_MASK 0x1f
#define TRIG_MASK 0x60
#define TRIG_NONE 0
#define TRIG_WALK 1
#define TRIG_USE 2
#define TRIG_SWITCH 3
#define SHOT_BIT 0x80
enum { ACT_NONE, ACT_WINDOW, ACT_OPEN_DOOR, ACT_OPEN_DOOR_FOREVER, ACT_OPEN_DOOR_30S,
  ACT_LOWER_FLOOR, ACT_RAISE_FLOOR, ACT_RAISE_STAIRS, ACT_CONTINUE_STAIRS, ACT_END_LEVEL,
  ACT_LOWER_FLOOR_FOREVER, ACT_OPEN_DOOR_10S, ACT_LOWER_FLOOR_15S, ACT_DAMAGE_FLOOR,
  ACT_FLASH_LIGHTS, ACT_OPEN_MONSTER_CLOSET, ACT_SECRET };

/* item type ids (editor ITEM_TYPES) */
enum { IT_SPAWN, IT_SOLDIER, IT_IMP, IT_PINKY, IT_BARON, IT_BARREL, IT_HEALTH, IT_SHELLS,
  IT_SHOTGUN, IT_CHAINGUN, IT_CHAINSAW, IT_ROCKETLAUNCHER, IT_GREENARMOR, IT_BLUEARMOR,
  IT_BACKPACK, IT_REDCARD, IT_BLUECARD, IT_YELLOWCARD, IT_SOULSPHERE, IT_RADSUIT,
  IT_HEALTHBONUS, IT_ARMORBONUS, IT_CLIP, IT_SHELLBOX, IT_AMMOBOX, IT_HEALTHCRATE,
  IT_ONEROCKET, IT_BOXOFROCKETS, IT_CANDELABRA, IT_LIGHTPOST, IT_GIBS, IT_GRUNTGIBS,
  IT_GRUNTCORPSE, IT_SKULLPILE, IT_TECHCOLUMN, IT_POSCORPSE, IT_IMPCORPSE, IT_DEMONCORPSE,
  IT_BARONCORPSE, IT_SWITCH, IT_FIREBALL, IT_PLASMABALL, IT_ROCKET, IT_EXPLOSION };

/* sounds (VicDoom ESound order) */
enum { SND_CLAW, SND_DMPAIN, SND_DOROPN, SND_DORCLS, SND_ITEMUP, SND_OOF, SND_GURGLE,
  SND_PISTOL, SND_PLPAIN, SND_SGCOCK, SND_SGTDTH, SND_SHOTGN, SND_STNMOV, SND_SAWIDL,
  SND_SAWFUL, SND_SAWHIT, SND_PUNCH, SND_BAREXP };

/* ---- RAM state (sectors) ---- */
extern u8 sec_floor[SEC_N], sec_ceil[SEC_N], sec_type[SEC_N], sec_target[SEC_N];
extern u8 sec_fcol[SEC_N], sec_ccol[SEC_N], sec_bright[SEC_N];
extern u8 sec_wdark[SEC_N], sec_seen[SEC_N], sec_visited[SEC_N];
extern u8 sector_max, seen_gen;
extern u8 sw_n, sw_sec[8], sw_dir[8];
extern u8 level_num_enemies, level_num_items, level_num_secrets, level_par_time;

#define MAX_ITEMS 128
extern u8 item_x[MAX_ITEMS], item_y[MAX_ITEMS], item_t[MAX_ITEMS];
extern u8 n_items;

/* ---- player ---- */
extern u16 playerx, playery;        /* world 8.8 (tile = 8 world units) */
extern u8 playera, eyeheight, player_prev_sec, player_sector, plr_id;
extern u8 health, armor, combat_armor, keys, owned_weapons, cur_weapon, has_backpack;
extern u8 ammo[3];
extern u8 god_mode, hurt_flash, hud_dirty;
extern u16 radsuit_ms;
extern u8 dt_ms;
extern u8 level_num, difficulty, end_level, clev, game_complete, episode;
extern u8 num_kills, num_items_got, num_secrets_got;
extern u16 map_time_sec, map_time_ms;
extern u8 info_kind, info_len; extern const char *info_name; extern u16 info_ms;
extern u8 effects_vol, music_vol;

/* ---- mobjs ---- */
#define MAX_MOBJ 32
#define MOBJ_MISSILE 31
#define MOBJ_PLAYER_ROCKET 30
#define MOBJINFO_POS 0
#define MOBJINFO_IMP 1
#define MOBJINFO_DEMON 2
#define MOBJINFO_BARON 3
#define MOBJINFO_IMPSHOT 4
#define MF_JUSTATTACKED 1
#define MF_ATK_WINDUP 2
#define MF_PLASMA 4
#define MF_GUTS_TAKEN 8
extern u8 mobj_alloc[MAX_MOBJ], mobj_x[MAX_MOBJ], mobj_y[MAX_MOBJ], mobj_xfrac[MAX_MOBJ], mobj_yfrac[MAX_MOBJ];
extern u8 mobj_info[MAX_MOBJ], mobj_state[MAX_MOBJ], mobj_health[MAX_MOBJ], mobj_movedir[MAX_MOBJ];
extern u8 mobj_movecnt[MAX_MOBJ], mobj_react[MAX_MOBJ], mobj_flags[MAX_MOBJ], mobj_corpse[MAX_MOBJ];
extern u8 missile_z, procket_z, anim_frame;
extern const u8 state_texture[], state_action[];
#define TEX_ANIMATE 64
#define ACTION_CHASE 0
#define FX_MAX 16
#define FX_FUSE 1
#define FX_EXPL 2
extern u8 fx_kind[FX_MAX], fx_tx[FX_MAX], fx_ty[FX_MAX], fx_time[FX_MAX];

/* ---- render outputs used by weapons ---- */
extern u8 col_aim_slot[VIEW_W], col_aim_z[VIEW_W];
extern u8 vis_slot[]; extern u8 vis_sec[];
#define VIS_LAYER 0x80
#define VIS_FX 0x40
extern u16 fb[VIEW_H * VIEW_W];

/* ---- weapons ---- */
extern u8 wpn_visible, wpn_shot_req, wpn_pose, wpn_flash_ticks, wpn_tiles_loaded;
extern volatile u8 key_fire_held;

/* ---- functions ---- */
/* main.c */
void wait_frames(u8 n);
void present_frame(u8 with_sprites);
void set_border(u8 sprpal_index);
u8 rnd8(void);
u16 rnd16(void);
extern volatile u8 frame_ms;    /* ms elapsed (ISR accumulates) */
extern volatile u16 vbl_count;
extern u8 pal_mode;
/* input (ISR latched) */
extern volatile u8 in_turn_l, in_turn_r, in_fwd, in_back, in_strafel, in_strafer;
extern volatile u8 in_use, in_fire, in_wnext, in_wprev, in_map;
extern volatile u8 pad_now;     /* raw pad bits (active high) */
extern volatile u8 pad_pressed; /* edges latched by the ISR until read */
#define PAD_UP 1
#define PAD_DOWN 2
#define PAD_LEFT 4
#define PAD_RIGHT 8
#define PAD_1 16
#define PAD_2 32
extern volatile u8 input_paused;
/* vdp.c */
void vdp_init_game(void);
void vdp_upload_fb(void);
void vdp_upload_sat(void);
extern u8 sat_n;
void vdp_flip(void);
void vdp_set_nt_front(void);
void fb_clear(void);
void load_game_tiles(void);
void load_weapon_tiles(u8 w);
void text_at(u8 x, u8 y, const char *s);
u16 char_entry(char c);
void ui_clear_screen(void);
/* sound.c */
void sound_init(void);
void sound_isr(void);
void play_sound(u8 s);
void music_start(void);
void music_stop(void);
/* level.c */
void load_level(void);
void start_level(void);
u8 map_id(u8 mx, u8 my);
u8 sector_at(u8 mx, u8 my);
void proc_update(void);
void sector_specials_update(void);
void try_walk_into(void);
void try_use(void);
void try_switch(void);
u8 sec_action(u8 s);
void elevator_find_dest(void);
void forever_adopt_nb(u8 s);
void floor_forever_activate(void);
void boss_lower_forever_floors(void);
void show_splash(void);
void update_eye(void);
s8 item_find(u8 tx, u8 ty);
void item_remove(u8 i);
void clear_sector_seen(void);
void mark_seen(u8 s);
extern u8 key_use, key_use_was;
extern u8 elev_mode, elev_dest, tmp_sec;
/* player.c */
void read_input(void);
void apply_move(void);
void try_pickups(void);
void damage_player(u8 dmg);
void player_frame(void);
void player_death_frame(void);
void update_info_msg(void);
void draw_info_msg(void);
void draw_hud(void);
void init_hud_state(void);
void add_ammo(u8 pool, u8 amount);
void info_show(const char *s);
void update_map_time(void);
u8 tile_blocked(u8 id);
extern u8 old_floor, old_ceil;
extern const u8 wpn_ammo_idx[6], ammo_max_base[3], ammo_max_pack[3], wpn_own_bit[6];
/* render.c */
void render_world(void);
extern u8 last_playera;
/* enemy.c */
void enemy_reset(void);
void enemy_alloc_all(void);
void enemy_think(void);
void enemy_damage(u8 m, u8 dmg);
void try_damage_enemy(u8 dmg, u8 maxz);
void try_damage_at_col(u8 dmg, u8 col);
void barrel_update(void);
void explode_tile(u8 tx, u8 ty);
void hitscan_process(void);
u8 spawn_player_rocket(void);
extern u8 pain_boost;
extern u8 react_dt_units;
/* weapon.c */
void switch_weapon(u8 w);
void init_weapon(void);
void weapon_tick_isr(void);
void update_muzzle_flash(void);
void weapon_overlay(void);
void hide_weapon(void);
void weapon_set_hl(void);
/* front.c */
void frontend(void);
void show_entering(void);
void after_level_end(void);
void mapscreen(void);
#endif
