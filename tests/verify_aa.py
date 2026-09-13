"""Controlled native subpixel-translation and point-preservation regression."""
from pathlib import Path
from PIL import Image,ImageChops
import json,statistics
folder=Path(__file__).resolve().parents[1]/'artifacts';checks=[];metrics={}
def verify(name,ok):
 print(('PASS ' if ok else 'FAIL ')+name);checks.append({'check':name,'pass':bool(ok)})
for size in [320,560]:
 modes=[]
 for mode in range(3):
  frames=[Image.open(folder/f'aa-{size}-{mode}-{k}.png').convert('RGBA') for k in range(4)]
  energies=[sum(i.getchannel('A').getdata()) for i in frames]
  crop=frames[0].getchannel('A').crop((int(size*.35),int(size*.35),int(size*.65),int(size*.65)))
  modes.append({'energy_mean':statistics.mean(energies),'translation_energy_cv':statistics.pstdev(energies)/statistics.mean(energies),'point_peak':max(crop.getdata()),'empty_fraction':crop.histogram()[0]/(crop.width*crop.height)})
  verify(f'{size}px mode {mode} preserves fully transparent corners',all(im.getpixel((0,0))[3]==0 and im.getpixel((size-1,size-1))[3]==0 for im in frames))
 off,on,full=modes;metrics[str(size)]=modes
 verify(f'{size}px AA reduces subpixel brightness instability',on['translation_energy_cv']<off['translation_energy_cv']*.8)
 verify(f'{size}px AA retains distinct bright grains',on['point_peak']>=off['point_peak']*.7 and on['empty_fraction']>.7)
 verify(f'{size}px AA preserves overall particle light output',.8<on['energy_mean']/off['energy_mean']<1.25)
 verify(f'{size}px maximum AA remains transparent and grainy',full['empty_fraction']>.7 and full['point_peak']>50)
a=Image.open(folder/'aa-560-1-0.png');b=Image.open(folder/'aa-repeat.png')
verify('Identical frame is independent of preceding frames (no temporal history)',ImageChops.difference(a,b).getbbox() is None)
(folder/'aa-verification.json').write_text(json.dumps({'checks':checks,'metrics':metrics},indent=2))
assert all(c['pass'] for c in checks)
print(json.dumps(metrics))
