#!/bin/bash
# profile build: expose statics of the given C files, run tests/prof.txt, report
cd /home/claude/sms_port_k7
export PATH="$HOME/.devkitsms/bin:$PATH"
for f in "$@"; do [ -f src/$f.c ] || continue;
  sdcc -mz80 -I$HOME/.devkitsms/include --peep-file $HOME/.devkitsms/include/peep-rules.txt -Isrc -Igen --opt-code-size -Dstatic= -c src/$f.c -o $f.rel 2>&1 | grep -i error
done
rm -f game.ihx; make game.ihx 2>&1 | grep -i error
python3 tools/mkrom.py game.ihx game.map gen/banks_all.pkl squaredoom.sms >/dev/null
smstest squaredoom.sms tests/prof.txt >/dev/null 2>&1
python3 - <<'PY'
import bisect
syms=[]
for l in open('game.noi'):
    p=l.split()
    if len(p)>=3 and p[0]=='DEF': syms.append((int(p[2],16),p[1]))
syms.sort(); keys=[s[0] for s in syms]
agg={}
tot=0
for l in open('prof.out'):
    a,c=l.split(); a=int(a,16); c=int(c); tot+=c
    i=bisect.bisect_right(keys,a)-1
    n=syms[i][1] if i>=0 else '?'
    if a>=0x8000: n='[banked]'
    agg[n]=agg.get(n,0)+c
for n,c in sorted(agg.items(),key=lambda x:-x[1])[:22]:
    print('%6.1f%% %9d %s'%(100*c/tot,c,n))
PY

