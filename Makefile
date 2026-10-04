DEVKIT ?= $(HOME)/.devkitsms
CC := sdcc
CFLAGS := -mz80 -I$(DEVKIT)/include --peep-file $(DEVKIT)/include/peep-rules.txt -Isrc -Igen --max-allocs-per-node 20000 --opt-code-size
LDFLAGS := -mz80 --no-std-crt0 --data-loc 0xC000 -Wl-b_BANK2=0x28000 -Wl-b_BANK3=0x38400
FIXED := cast.rel castasm.rel main.rel vdp.rel sound.rel player.rel render.rel weapon.rel mathx.rel world.rel automap.rel splash.rel fixed.rel sound_data.rel
LOGIC := level.rel enemy.rel
FRONT := front.rel menu_assets.rel
all: squaredoom.sms

gen/fixed.c gen/fixed.h gen/banks_game.pkl: tools/gen_game.py tools/weapons.py tools/common.py
	python3 tools/gen_game.py
gen/sound_data.c gen/sound_data.h: tools/gen_sound.py tools/sidcap.py
	python3 tools/gen_sound.py
gen/menu_data.h gen/menu_assets.c gen/banks_all.pkl: tools/gen_menu.py gen/banks_game.pkl
	python3 tools/gen_menu.py

HDRS := src/defs.h gen/fixed.h gen/sound_data.h gen/menu_data.h
$(filter-out cast.rel fixed.rel sound_data.rel,$(FIXED)): %.rel: src/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@
level.rel enemy.rel: %.rel: src/%.c $(HDRS)
	$(CC) $(CFLAGS) --codeseg BANK3 --constseg BANK3 -c $< -o $@
front.rel: src/front.c $(HDRS)
	$(CC) $(CFLAGS) --codeseg BANK2 --constseg BANK2 -c $< -o $@
menu_assets.rel: gen/menu_assets.c gen/menu_data.h
	$(CC) $(CFLAGS) --codeseg BANK2 --constseg BANK2 -c $< -o $@
cast.rel: src/cast.s
	sdasz80 -plosgff -o cast.rel src/cast.s
fixed.rel: gen/fixed.c gen/fixed.h
	$(CC) $(CFLAGS) -c $< -o $@
sound_data.rel: gen/sound_data.c gen/sound_data.h
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: game.ihx
game.ihx: $(FIXED) $(LOGIC) $(FRONT)
	$(CC) -o $@ $(LDFLAGS) $(DEVKIT)/lib/crt0_sms.rel $(FIXED) $(DEVKIT)/lib/SMSlib.lib $(LOGIC) $(FRONT)
squaredoom.sms: game.ihx gen/banks_all.pkl tools/mkrom.py
	python3 tools/mkrom.py game.ihx game.map gen/banks_all.pkl squaredoom.sms
playtest: squaredoom.sms
	tests/runall.sh
clean:
	rm -f *.rel *.ihx *.sym *.lst *.map *.noi *.lk *.asm *.rst *.sms
