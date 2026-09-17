"""Native tray command, z-order persistence and complete default-preset checks.

Requires the freshly built Luma running. Restores settings and window position,
and leaves the x64 product running. Do not change settings during this test.
"""
from pathlib import Path
import ctypes,json,math,os,subprocess,time,urllib.request,urllib.parse,urllib.error

root=Path(__file__).resolve().parents[1]
session_file=Path(os.environ['LOCALAPPDATA'])/'Luma/session.json'
expected=json.loads((root/'tests/fixtures/factory-defaults.json').read_text())
session=json.loads(session_file.read_text());checks=[]
user=ctypes.windll.user32
user.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
user.FindWindowW.restype=ctypes.c_void_p
user.SendMessageW.restype=ctypes.c_ssize_t
kernel=ctypes.windll.kernel32;kernel.OpenProcess.restype=ctypes.c_void_p

def api(path='state',data=None):
    request=urllib.request.Request(f"http://127.0.0.1:{session['port']}/api/{path}",
        data=urllib.parse.urlencode(data).encode() if data is not None else None,
        headers={'X-Luma-Token':session['token']})
    with urllib.request.urlopen(request,timeout=5) as response:return json.load(response)

def wait(predicate):
    deadline=time.monotonic()+15
    while time.monotonic()<deadline:
        try:
            value=predicate()
            if value:return value
        except (OSError,ValueError):pass
        time.sleep(.05)
    raise AssertionError('Timed out waiting for Luma')

def hwnd():return ctypes.c_void_p(user.FindWindowW('LumaOverlay',None))
def style():return user.GetWindowLongW(hwnd(),-20)
def check(name,ok):
    checks.append({'check':name,'pass':bool(ok)});print(('PASS ' if ok else 'FAIL ')+name)
    assert ok,name

def restart(name):
    global session
    handle=kernel.OpenProcess(0x100000,False,session['pid'])
    try:
        api('action',{'action':'quit'})
        if handle:assert kernel.WaitForSingleObject(ctypes.c_void_p(handle),10000)==0
    finally:
        if handle:kernel.CloseHandle(ctypes.c_void_p(handle))
    process=subprocess.Popen([str(root/'dist'/name),'--quiet'])
    session=wait(lambda:json.loads(session_file.read_text()) if session_file.exists() else None)
    assert session['pid']==process.pid
    wait(lambda:api())

original=api()
assert 'topmost' in original,'Run the new release before testing'
try:
    for name in ['Luma.exe','Luma-x86.exe']:
        restart(name)
        api('action',{'action':'reset'})
        wait(lambda:all(api()[k]==v for k,v in expected.items()))
        check(name+' restores every factory parameter and defaults to topmost',all(api()[k]==v for k,v in expected.items()) and bool(style()&8))
        locked=api()['locked']
        user.SendMessageW(hwnd(),0x111,7,0) # The tray menu command.
        check(name+' tray command disables native topmost without changing mouse passthrough',api()['topmost']==0 and not style()&8 and api()['locked']==locked)
        api('settings',{'locked':1,'size':480,'glow':1})
        wait(lambda:api()['size']==480 and bool(style()&0x20))
        check(name+' other settings preserve non-topmost z-order',not style()&8)
        restart(name)
        check(name+' disabled topmost persists across restart',api()['topmost']==0 and not style()&8)
        user.SendMessageW(hwnd(),0x111,7,0)
        check(name+' tray command re-enables native topmost',api()['topmost']==1 and bool(style()&8))
        restart(name)
        check(name+' enabled topmost persists across restart',api()['topmost']==1 and bool(style()&8))
        for value in [-1,2,.5]:
            try:api('settings',{'topmost':value});rejected=False
            except urllib.error.HTTPError as error:rejected=error.code==400
            check(name+f' rejects invalid topmost {value}',rejected)
finally:
    api('settings',{k:original[k] for k in expected})
    restart('Luma.exe')
    current=api();pad=math.ceil(current['bloomSpread']*40)+16
    user.SetWindowPos(hwnd(),None,original['x']-pad,original['y']-pad,0,0,0x0015) # NOSIZE|NOZORDER|NOACTIVATE
    user.SendMessageW(hwnd(),0x232,0,0) # Commit position using the normal move handler.
    check('Original preferences and position restored',all(api()[k]==original[k] for k in [*expected,'x','y']))
    (root/'artifacts').mkdir(exist_ok=True)
    (root/'artifacts/topmost-verification.json').write_text(json.dumps(checks,indent=2))
