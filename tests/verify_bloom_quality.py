"""Native Bloom edge reconstruction, padding and window coordinate contract."""
from pathlib import Path
from PIL import Image,ImageChops
import math,statistics,json,os,urllib.request,ctypes,time
folder=Path(__file__).resolve().parents[1]/'artifacts';checks=[];metrics={}
def verify(name,ok):
    checks.append({'check':name,'pass':bool(ok)});print(('PASS ' if ok else 'FAIL ')+name)
plain=Image.open(folder/'aa-repeat.png').getchannel('A')
for name in ['bloom-resolution-before','heat-0-2']:
    im=Image.open(folder/f'{name}.png').getchannel('A');widths=[]
    for j in range(180):
        c,s=math.cos(j*math.tau/180),math.sin(j*math.tau/180)
        r=max(r for r in range(135,246) if plain.getpixel((round(280+r*c),round(280+r*s)))>30)
        ray=[im.getpixel((round(280+(r+k)*c),round(280+(r+k)*s))) for k in range(60)]
        widths.append(next(k for k,v in enumerate(ray) if v<25)-next(k for k,v in enumerate(ray) if v<230))
    metrics[name+'_edge_width']=statistics.mean(widths)
verify('Bloom shoulder is tighter while retaining a smooth fade',2<metrics['heat-0-2_edge_width']<metrics['bloom-resolution-before_edge_width']*.8)
for size in [160,320,560]:
    for angle in range(3):
        im=Image.open(folder/f'padding-{size}-{angle}.png').getchannel('A');w,h=im.size
        verify(f'{size}px orientation {angle} includes transparent margin',w==h==size+192)
        border=[im.getpixel((x,y)) for x,y in [(x,0) for x in range(w)]+[(x,h-1) for x in range(w)]+[(0,y) for y in range(h)]+[(w-1,y) for y in range(h)]]
        verify(f'{size}px orientation {angle} max Bloom/impulse fades before all four edges',max(border)==0)
before=Image.open(folder/'padding-scale-0.png').getchannel('A');after=Image.open(folder/'padding-scale-1.png').getchannel('A').crop((96,96,416,416))
delta=ImageChops.difference(before,after);metrics['padding_aligned_alpha_mae']=statistics.mean(delta.getdata())
verify('Extra window margin does not shrink or shift artwork',metrics['padding_aligned_alpha_mae']<.2 and max(delta.getdata())<=2)

session=json.loads((Path(os.environ['LOCALAPPDATA'])/'Luma/session.json').read_text())
def state():
    req=urllib.request.Request(f"http://127.0.0.1:{session['port']}/api/state",headers={'X-Luma-Token':session['token']})
    with urllib.request.urlopen(req) as response:return json.load(response)
s=state();pad=math.ceil(s['bloomSpread']*40)+16
class Rect(ctypes.Structure):_fields_=[('left',ctypes.c_long),('top',ctypes.c_long),('right',ctypes.c_long),('bottom',ctypes.c_long)]
user=ctypes.windll.user32;user.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4));user.FindWindowW.restype=ctypes.c_void_p;hwnd=user.FindWindowW('LumaOverlay',None);rect=Rect();user.GetWindowRect(ctypes.c_void_p(hwnd),ctypes.byref(rect))
verify('Native window size and location match padded rendering',rect.right-rect.left==rect.bottom-rect.top==s['size']+2*pad and rect.left+pad==s['x'] and rect.top+pad==s['y'])
user.SendMessageW.restype=ctypes.c_ssize_t
def hit(x,y):return user.SendMessageW(ctypes.c_void_p(hwnd),0x84,0,ctypes.c_ssize_t((x&65535)|((y&65535)<<16)))
verify('Transparent margin passes mouse through',hit(rect.left+1,rect.top+1)==-1)
verify('Artwork centre keeps expected drag / locked hit behavior',hit(s['x']+s['size']//2,s['y']+s['size']//2)==(-1 if s['locked'] else 1))
samples=[]
for _ in range(5):time.sleep(1);samples.append(state())
metrics['fresh_native_run']={'fps_mean':statistics.mean(x['actualFps'] for x in samples),'working_set_mb_max':max(x['memoryMb'] for x in samples),'cpu_mean':statistics.mean(x['cpu'] for x in samples),'logical_size':s['size'],'window_size':s['size']+2*pad,'aa':s['aaStrength'],'rms_max':max(x['rms'] for x in samples),'note':'Read-only sampling after release restart; live system audio, not isolated GPU timing.'}
(folder/'bloom-quality-verification.json').write_text(json.dumps({'checks':checks,'metrics':metrics},indent=2))
assert all(c['pass'] for c in checks)
print(json.dumps(metrics))
