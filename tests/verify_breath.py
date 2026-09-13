"""Native breathing-light evidence and final-output dark-gradient regression."""
from pathlib import Path
from PIL import Image, ImageChops
import json, os, time, urllib.request, urllib.parse

root=Path(__file__).resolve().parents[1]; folder=root/'artifacts'; checks=[]
def verify(name, ok):
    checks.append({'check':name,'pass':bool(ok)})
    print(('PASS ' if ok else 'FAIL ')+name)

frames=[Image.open(folder/f'breath-{i}.png').convert('RGBA') for i in range(5)]
energies=[sum(im.getchannel('A').getdata()) for im in frames]
# Measure the requested breathing AIR, not total energy dominated by unchanged
# particle cores. The independent translucent halo must visibly strengthen.
empty=[v==0 for v in Image.open(folder/'aa-repeat.png').getchannel('A').getdata()]
halo_energy=[sum(v for v,is_empty in zip(im.getchannel('A').getdata(),empty) if is_empty) for im in frames]
verify('General onset visibly raises native halo light output',halo_energy[1]>halo_energy[0]*1.2)
verify('Bloom falls back gradually after onset',energies[0]<energies[2]<energies[1])
verify('Bloom settles within 1% of baseline after two seconds',abs(energies[3]/energies[0]-1)<.01)
verify('Zero pulse exactly reproduces baseline pixels',ImageChops.difference(frames[0],frames[4]).getbbox() is None)
verify('Breathing light preserves transparent corners',all(im.getpixel((0,0))[3]==0 for im in frames))
verify('Peak bloom keeps sparse centre transparent',sum(frames[1].getchannel('A').crop((220,220,340,340)).getdata())/(120*120)<30)

def dark_plateaus(im):
    p=im.getchannel('A').load(); flat=count=0
    for y in range(131,339):
        for x in range(6,144):
            v=p[x,y]
            if 1<=v<=20:
                count+=1
                flat+=all(p[x+dx,y+dy]==v for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)])
    return flat/max(count,1)
before=Image.open(folder/'halo-current-before.png').convert('RGBA')
after=Image.open(folder/'halo-current-after.png').convert('RGBA')
old_flat,new_flat=dark_plateaus(before),dark_plateaus(after)
verify('Final dithering breaks coherent dark 8-bit plateaus',new_flat<old_flat*.5)
verify('Debanding preserves total light energy within 1%',abs(sum(after.getchannel('A').getdata())/sum(before.getchannel('A').getdata())-1)<.01)

session=json.loads((Path(os.environ['LOCALAPPDATA'])/'Luma/session.json').read_text())
def api(data=None):
    req=urllib.request.Request(f"http://127.0.0.1:{session['port']}/api/"+('settings' if data is not None else 'state'),data=urllib.parse.urlencode(data).encode() if data is not None else None,headers={'X-Luma-Token':session['token']})
    with urllib.request.urlopen(req) as response:return json.load(response)
original=api()
try:
    api({'demo':1,'bloomStrength':1.1,'bloomPulse':.8,'bloomDecay':.35})
    samples=[]
    for _ in range(25):
        time.sleep(.04);samples.append(api())
    verify('Demo drives the live general-onset bloom envelope',max(s['bloomEnvelope'] for s in samples)>.2)
    verify('Rapid hits keep the live envelope bounded',all(0<=s['bloomEnvelope']<=1 and 1.1<=s['effectiveBloom']<=1.981 for s in samples))
    api({'bloomPulse':0});time.sleep(.08)
    verify('Pulse zero disables live modulation',api()['effectiveBloom']==1.1)
    api({'bloomStrength':0,'bloomPulse':3});time.sleep(.08)
    verify('Base bloom zero also disables breathing light',api()['effectiveBloom']==0)
finally:
    api({k:original[k] for k in ['demo','bloomStrength','bloomPulse','bloomDecay']})
metrics={'native_alpha_energy':energies,'dark_plateau_fraction_before':old_flat,'dark_plateau_fraction_after':new_flat,'note':'Fixed-scene dark halo crop; plateau metric is not a percentage of all visible banding removed.'}
(folder/'breath-verification.json').write_text(json.dumps({'checks':checks,'metrics':metrics},indent=2))
assert all(c['pass'] for c in checks)
