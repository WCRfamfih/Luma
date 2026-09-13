"""End-to-end native process / local API checks. Restores existing preferences."""
import json, os, pathlib, statistics, subprocess, time, urllib.request, urllib.parse, http.client
ROOT=pathlib.Path(__file__).resolve().parents[1]
SESSION=pathlib.Path(os.environ['LOCALAPPDATA'])/'Luma'/'session.json'
s=json.loads(SESSION.read_text()); base=f"http://127.0.0.1:{s['port']}"
results=[]
def call(path='state',data=None,auth=True,extra=None):
    h={'X-Luma-Token':s['token']} if auth else {}
    if extra:h.update(extra)
    payload=urllib.parse.urlencode(data).encode() if data is not None else None
    if payload is not None:h['Content-Type']='application/x-www-form-urlencoded'
    connection=http.client.HTTPConnection('127.0.0.1',s['port'],timeout=5)
    try:
        connection.request('POST' if data is not None else 'GET','/api/'+path,body=payload,headers=h)
        response=connection.getresponse();return response.status,json.loads(response.read()) if response.status==200 else {}
    finally:connection.close()
def verify(name,condition):
    results.append({'check':name,'pass':bool(condition)})
    print(('PASS ' if condition else 'FAIL ')+name)
    if not condition:raise AssertionError(name)
original=call()[1]
keys=['size','fps','density','mode','sensitivity','pointSize','motion','glow','opacity','locked','color','demo','innerDensity','innerOpacity','flowStrength','waveStrength','waveSpeed','physicsFreedom','rotationSpeed','waveRoughness','bloomStrength','bloomSpread','heatStrength','outerWaveInfluence','innerBounce','aaStrength','bloomPulse','bloomDecay']
try:
    verify('FFT plus transient attack / sustained-tone rejection / spring decay',subprocess.run([str(ROOT/'dist/Luma-x64.exe'),'--self-test']).returncode==0)
    verify('Settings embedded and served without dependencies',b'id="preview"' in urllib.request.urlopen(base).read())
    verify('Reject unauthenticated API access',call(auth=False)[0]==403)
    verify('Reject cross-origin settings mutation',call('settings',{'size':600},extra={'Origin':'https://example.com'})[0]==403)
    verify('Reject DNS rebinding host',call(extra={'Host':'example.com'})[0]==403)
    for bad in ['nan','inf','-1','999999','1.3','oops']:
        verify('Reject invalid size '+bad,call('settings',{'size':bad})[0]==400)
    verify('Reject invalid color',call('settings',{'color':'#zzzzzz'})[0]==400)
    for key, bad in [('innerDensity',1.1),('innerOpacity',-1),('flowStrength',3),('waveStrength',4),('waveSpeed',0),('physicsFreedom',1.1),('rotationSpeed',3),('waveRoughness',3),('bloomStrength',4),('bloomSpread',0),('heatStrength',4),('outerWaveInfluence',1.1),('innerBounce',2.1),('aaStrength',1.1),('bloomPulse',3.1),('bloomDecay',0)]:
        verify('Reject out-of-range '+key,call('settings',{key:bad})[0]==400)
    before=call()[1]['color'];verify('Atomic validation',call('settings',{'color':'#74BFFF','size':'invalid'})[0]==400 and call()[1]['color']==before)
    verify('Real WASAPI endpoint is connected',bool(call()[1]['audioConnected']))
    target={'color':'#9DE5CC','size':640,'mode':1,'density':3,'fps':60,'demo':1,'locked':1,'innerDensity':.4,'innerOpacity':.3,'flowStrength':1.5,'waveStrength':2,'waveSpeed':1.5,'physicsFreedom':.75,'rotationSpeed':.8,'waveRoughness':1.5,'bloomStrength':.9,'bloomSpread':1.3,'heatStrength':1.5,'outerWaveInfluence':.35,'innerBounce':1.5,'aaStrength':.6,'bloomPulse':1.5,'bloomDecay':.6}
    verify('Live settings update accepted',call('settings',target)[0]==200);time.sleep(.3)
    after=call()[1];verify('All changed settings read back',all(after[k]==v for k,v in target.items()))
    ini=(SESSION.parent/'settings.ini').read_text();verify('Color and mode persisted to disk','color=#9DE5CC' in ini and 'mode=1' in ini)
    visible=call()[1]['visible'];call('action',{'action':'visibility'});time.sleep(.2);verify('Tray-equivalent hide action',call()[1]['visible']!=visible);call('action',{'action':'visibility'})
    paused=call()[1]['paused'];call('action',{'action':'pause'});time.sleep(.2);verify('Pause action',call()[1]['paused']!=paused);call('action',{'action':'pause'})
    call('settings',{'density':2,'mode':0,'size':560,'fps':60,'demo':1})
    samples=[]
    for i in range(8):time.sleep(1);samples.append(call()[1])
    metrics={'fps_mean':round(statistics.mean(x['actualFps'] for x in samples),2),'cpu_total_percent_mean':round(statistics.mean(x['cpu'] for x in samples),4),'working_set_mb_max':max(x['memoryMb'] for x in samples),'audio_rms_max':max(x['rms'] for x in samples),'samples':8,'settings':'560px, 20,736 particle budget, 60 FPS limit, demo enabled; native process only'}
    verify('GPU frame pacing exceeds 50 FPS average',metrics['fps_mean']>50)
    verify('Captured live audio has nonzero RMS',metrics['audio_rms_max']>0)
    from PIL import Image
    image=Image.open(ROOT/'artifacts/native-particles.png').convert('RGBA');alpha=image.getchannel('A');hist=alpha.histogram()
    verify('Native render has zero-alpha background',hist[0]>image.width*image.height*.3)
    verify('Native render has antialiased partial-alpha particles',sum(hist[1:255])>1000)
    center=alpha.crop((image.width*.4,image.height*.4,image.width*.6,image.height*.6))
    verify('Default centre contains translucent particles',center.getextrema()[1]>0 and center.histogram()[0]>center.width*center.height*.3)
    verify('Native rhythm features available to preview',all(k in call()[1] for k in ['spring','transient','bass','dominant','waves','kicks','bloomEnvelope','effectiveBloom']))
    verify('Every release EXE is under 50 MB',all(p.stat().st_size<50*1024*1024 for p in (ROOT/'dist').glob('*.exe')))
    (ROOT/'artifacts/verification.json').write_text(json.dumps({'checks':results,'metrics':metrics},indent=2),encoding='utf-8')
    print(json.dumps(metrics))
finally:
    call('settings',{k:original[k] for k in keys})
