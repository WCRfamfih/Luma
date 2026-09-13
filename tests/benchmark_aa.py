"""Brief paired on/off native performance sample; restores every changed key."""
from pathlib import Path
import json,os,time,http.client,urllib.parse,statistics
folder=Path(__file__).resolve().parents[1]/'artifacts'
s=json.loads((Path(os.environ['LOCALAPPDATA'])/'Luma/session.json').read_text())
def api(values=None):
 c=http.client.HTTPConnection('127.0.0.1',s['port'],timeout=5);h={'X-Luma-Token':s['token']}
 if values is not None:h['Content-Type']='application/x-www-form-urlencoded'
 c.request('POST' if values is not None else 'GET','/api/settings' if values is not None else '/api/state',urllib.parse.urlencode(values) if values is not None else None,h)
 r=c.getresponse();assert r.status==200;result=json.loads(r.read());c.close();return result
original=api();changed={'size':560,'density':2,'mode':0,'fps':60,'demo':1,'aaStrength':0};results={}
try:
 api(changed)
 for name,aa in [('off',0),('on',.75)]:
  api({'aaStrength':aa});time.sleep(2);samples=[]
  for k in range(6):time.sleep(1);samples.append(api())
  results[name]={'fps_mean':round(statistics.mean(x['actualFps'] for x in samples),2),'native_working_set_mb_max':max(x['memoryMb'] for x in samples),'samples':6}
 assert results['on']['fps_mean']>50 and results['on']['fps_mean']>results['off']['fps_mean']*.85
 results['notes']='560 px / 20,736 instance budget / 60 FPS limit / demo. Vsync-limited FPS sample, not a GPU-pass timing measurement. Browser cost excluded.'
 (folder/'aa-performance.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))
finally:api({key:original[key] for key in changed})
