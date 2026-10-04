import sys,re
noi={}
for l in open('game.noi'):
    p=l.split()
    if len(p)>=3 and p[0]=='DEF': noi[p[1]]=int(p[2],16)
src=open(sys.argv[1]).read()
def sub(m):
    return '0x%04X' % (noi['_'+m.group(1)] + int(m.group(2)))
sys.stdout.write(re.sub(r'\{(\w+)\+(\d+)\}', sub, src))
