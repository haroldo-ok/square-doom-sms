#!/bin/bash
# profiling build: every static function exposed so the map has its symbol
cd "$(dirname "$0")/.."
export PATH="$HOME/.devkitsms/bin:$PATH"
CF="-mz80 -I$HOME/.devkitsms/include --peep-file $HOME/.devkitsms/include/peep-rules.txt -Isrc -Igen --opt-code-size -Dstatic="
for f in render main weapon player world vdp automap img castasm; do sdcc $CF -c src/$f.c -o $f.rel || exit 1; done
for f in level enemy; do sdcc $CF --codeseg BANK3 --constseg BANK3 -c src/$f.c -o $f.rel; done
rm -f game.ihx; make game.ihx >/dev/null 2>&1; python3 tools/mkrom.py game.ihx game.map gen/banks_all.pkl squaredoom.sms >/dev/null
smstest squaredoom.sms ${PROFTEST:-tests/prof.txt} >/dev/null 2>&1
python3 tools/profmap.py ${1:-30}
touch src/*.c; make >/dev/null 2>&1
