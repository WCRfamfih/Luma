"""Native color-selective heating and the captured factory-preset contract."""
from pathlib import Path
from PIL import Image,ImageChops
import json, statistics, re

root=Path(__file__).resolve().parents[1];folder=root/'artifacts';checks=[];metrics={}
def verify(name,ok):
    checks.append({'check':name,'pass':bool(ok)});print(('PASS ' if ok else 'FAIL ')+name)
for color,name in [(0,'Amber'),(1,'Blue')]:
    levels=[]
    for level in range(3):
        im=Image.open(folder/f'heat-{color}-{level}.png').convert('RGBA');pixels=list(im.getdata())
        rim=[p for p in pixels if p[3]>180];halo=[p for p in pixels if 8<p[3]<30]
        levels.append({'rim_neutrality':statistics.mean(min(p[:3])/max(p[:3]) for p in rim),'halo_neutrality':statistics.mean(min(p[:3])/max(p[:3]) for p in halo),'centre_alpha':statistics.mean(p[3] for p in im.crop((220,220,340,340)).getdata())})
        verify(f'{name} heat {level} retains fully transparent corners',im.getpixel((0,0))[3]==0)
    off,mid,full=levels;metrics[name]=levels
    cold=Image.open(folder/f'heat-{color}-0.png').convert('RGBA');hot=Image.open(folder/f'heat-{color}-2.png').convert('RGBA')
    gains=[min(q[:3])/max(q[:3])-min(p[:3])/max(p[:3]) for p,q in zip(cold.getdata(),hot.getdata()) if p[3]>160]
    changed=sum(g>.1 for g in gains)/len(gains)
    # The requested contract is LOCAL dense heating, not a white whole ring.
    verify(f'{name} dense hotspots progressively whiten while most material retains color',off['rim_neutrality']<mid['rim_neutrality']<full['rim_neutrality'] and max(gains)>.25 and .02<changed<.35)
    sparse=ImageChops.difference(cold,hot).crop((220,220,340,340))
    verify(f'{name} sparse particles are exactly unchanged at maximum heat',all(hi==0 for lo,hi in sparse.getextrema()))
    metrics[name+'_heated_fraction']=changed
    verify(f'{name} centre remains sparse at maximum heat',abs(full['centre_alpha']-off['centre_alpha'])<.5 and full['centre_alpha']<10)

# Validate the shipped factory initializers against the independently captured
# live user preset, rather than a second handwritten list of expected values.
capture=json.loads((root/'tests/fixtures/factory-defaults.json').read_text(encoding='utf-8-sig'))
source=(root/'src/main.cpp').read_text(encoding='utf-8-sig');config=re.search(r'struct Config \{(.*?)\};',source,re.S)[1]
keys=re.search(r'for\(auto key:\{([^}]+)\}',source)[1];keys=re.findall(r'"([^"]+)"',keys)
actual={}
for key in keys:
    value=re.search(r'\b'+key+r'=([^,;]+)',config)[1]
    actual[key]=value.strip('"') if key=='color' else int(value=='true') if key in ['locked','topmost'] else float(value.rstrip('f'))
verify('Every persisted factory parameter matches the captured browser preset',all(actual[k]==capture[k] for k in keys))
(folder/'factory-defaults.json').write_text(json.dumps(actual,indent=2))
(folder/'heat-verification.json').write_text(json.dumps({'checks':checks,'metrics':metrics},indent=2))
assert all(c['pass'] for c in checks)
