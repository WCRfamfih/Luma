"""Release lifecycle, 32-bit compatibility and occupied-port smoke test."""
import ctypes,http.client,json,os,pathlib,socket,subprocess,time,urllib.parse
root=pathlib.Path(__file__).resolve().parents[1];directory=pathlib.Path(os.environ['LOCALAPPDATA'])/'Luma';session=directory/'session.json';checks=[]
def api(s,path='state',data=None):
    c=http.client.HTTPConnection('127.0.0.1',s['port'],timeout=4)
    headers={'X-Luma-Token':s['token']};body=None
    if data is not None:headers['Content-Type']='application/x-www-form-urlencoded';body=urllib.parse.urlencode(data)
    c.request('POST' if data else 'GET','/api/'+path,body,headers);r=c.getresponse();value=json.loads(r.read());c.close();return value
def wait_for(predicate,seconds=10):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        try:
            value=predicate()
            if value:return value
        except (OSError,ValueError):pass
        time.sleep(.1)
    raise AssertionError('Timed out')
def verify(name,value):
    checks.append({'check':name,'pass':bool(value)});print(('PASS ' if value else 'FAIL ')+name)
    assert value,name
if session.exists():
    old=json.loads(session.read_text());before=api(old)
    kernel=ctypes.windll.kernel32;kernel.OpenProcess.restype=ctypes.c_void_p;handle=kernel.OpenProcess(0x100000,False,old['pid'])
    api(old,'action',{'action':'quit'});wait_for(lambda:not session.exists())
    if handle:kernel.WaitForSingleObject(ctypes.c_void_p(handle),10000);kernel.CloseHandle(ctypes.c_void_p(handle))
else:before=None
occupied=socket.socket();occupied.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1);occupied.bind(('127.0.0.1',17863));occupied.listen()
try:
    process=subprocess.Popen([str(root/'dist/Luma-x86.exe'),'--quiet','--snapshot'])
    current=wait_for(lambda:json.loads(session.read_text()));verify('32-bit executable launches',current['pid']==process.pid)
    verify('Occupied default port falls back to an available port',current['port']!=17863)
    status=wait_for(lambda:api(current) if api(current)['audioConnected'] else None)
    verify('32-bit WASAPI connects to system audio',status['audioConnected'])
    if before:verify('Preferences survive process / architecture restart',all(status[k]==before[k] for k in ['color','size','mode','density','locked','fps','innerDensity','innerOpacity','flowStrength','waveStrength','waveSpeed','physicsFreedom','rotationSpeed','waveRoughness','bloomStrength','bloomSpread','heatStrength','outerWaveInfluence','innerBounce','aaStrength','bloomPulse','bloomDecay']))
    user=ctypes.windll.user32;user.FindWindowW.restype=ctypes.c_void_p;hwnd=user.FindWindowW('LumaOverlay',None)
    style=user.GetWindowLongW(ctypes.c_void_p(hwnd),-20)
    verify('Native window is topmost, tool-window and layered',style&0x8 and style&0x80 and style&0x80000)
    api(current,'settings',{'locked':1});time.sleep(.15);style=user.GetWindowLongW(ctypes.c_void_p(hwnd),-20)
    verify('Locked mode applies WS_EX_TRANSPARENT',style&0x20)
    api(current,'settings',{'locked':before['locked'] if before else 0})
    api(current,'action',{'action':'quit'});process.wait(timeout=10);verify('Graceful 32-bit exit',process.returncode==0)
    verify('Session discovery file cleaned on exit',not session.exists())
finally:occupied.close()
final=subprocess.Popen([str(root/'dist/Luma-x64.exe'),'--quiet','--snapshot'])
current=wait_for(lambda:json.loads(session.read_text()));verify('64-bit release relaunched',current['pid']==final.pid)
verify('Normal port reused after cleanup',current['port']==17863)
(root/'artifacts/release-smoke.json').write_text(json.dumps(checks,indent=2))
