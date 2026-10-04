import re,bisect,sys
syms=[]
for l in open('game.map'):
    m=re.match(r'^\s+0000([0-9A-F]{4})\s+(\S+)\s+(\S+)\s*$',l)
    if m and not m.group(2).startswith(('l__','s__')): syms.append((int(m.group(1),16),m.group(2)+'@'+m.group(3)))
syms=sorted(set(syms)); keys=[s[0] for s in syms]
agg={}; tot=0
for l in open('prof.out'):
    a,c=l.split(); a=int(a,16); c=int(c); tot+=c
    if a>=0x8000: n='[banked]'
    else:
        i=bisect.bisect_right(keys,a)-1; n=syms[i][1] if i>=0 else '?'
    agg[n]=agg.get(n,0)+c
for n,c in sorted(agg.items(),key=lambda x:-x[1])[:int(sys.argv[1]) if len(sys.argv)>1 else 25]: print('%5.1f%% %8d %s'%(100*c/tot,c,n))
