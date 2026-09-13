"""Compare inward-fold thickness with the preceding uniform-band render."""
from pathlib import Path
from PIL import Image,ImageChops
import math,statistics,json
folder=Path(__file__).resolve().parents[1]/'artifacts';checks=[]
def verify(name,ok):
    checks.append({'check':name,'pass':bool(ok)});print(('PASS ' if ok else 'FAIL ')+name)
def widths(im):
    result=[]
    for j in range(180):
        c,s=math.cos(j*math.tau/180),math.sin(j*math.tau/180)
        samples=[im.getpixel((round(280+r*c),round(280+r*s))) for r in range(135,246)]
        band=[i for i in range(2,len(samples)-2) if sum(samples[i-2:i+3])/5>100]
        if band:result.append(max(band)-min(band))
    return result
before=Image.open(folder/'rim-before.png').getchannel('A');after=Image.open(folder/'heat-0-2.png').getchannel('A')
old,new=widths(before),widths(after)
verify('Inner band has substantially varied thickness around the rim',statistics.pstdev(new)>statistics.pstdev(old)*2)
verify('Broad inward folds are visible at native 560px',max(new)>max(old)+10)
verify('Thin stretches are at most half the broad-fold width',min(new)<=max(new)*.5)
# Broader luminous folds slightly change distant halo tails. Permit at most
# one 8-bit alpha level; particle relocation or opaque centre fill must fail.
verify('Sparse centre changes by at most one alpha level',max(ImageChops.difference(before,after).crop((220,220,340,340)).getdata())<=1)
verify('Inward folds retain transparent corners',after.getpixel((0,0))==0)
(folder/'rim-verification.json').write_text(json.dumps({'checks':checks,'metrics':{'old_width_px':[min(old),max(old)],'new_width_px':[min(new),max(new)],'old_width_std':statistics.pstdev(old),'new_width_std':statistics.pstdev(new)}},indent=2))
assert all(c['pass'] for c in checks)
