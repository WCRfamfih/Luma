"""Fresh-install factory defaults and native EXE icon resources. Restores INI."""
from pathlib import Path
import os,json,time,subprocess,urllib.request,urllib.parse,ctypes,struct,uuid
root=Path(__file__).resolve().parents[1];directory=Path(os.environ['LOCALAPPDATA'])/'Luma';session=directory/'session.json';ini=directory/'settings.ini';checks=[]
expected=json.loads((root/'tests/fixtures/factory-defaults.json').read_text())
def verify(name,ok):
    checks.append({'check':name,'pass':bool(ok)});print(('PASS ' if ok else 'FAIL ')+name)
    assert ok,name
def wait(predicate):
    deadline=time.monotonic()+15
    while time.monotonic()<deadline:
        try:
            result=predicate()
            if result:return result
        except (OSError,ValueError):pass
        time.sleep(.1)
    raise AssertionError('Timed out')
def api(s,path='state',data=None):
    req=urllib.request.Request(f"http://127.0.0.1:{s['port']}/api/{path}",data=urllib.parse.urlencode(data).encode() if data is not None else None,headers={'X-Luma-Token':s['token']})
    with urllib.request.urlopen(req) as response:return json.load(response)

kernel=ctypes.windll.kernel32
kernel.LoadLibraryExW.restype=ctypes.c_void_p;kernel.FindResourceW.restype=ctypes.c_void_p;kernel.LoadResource.restype=ctypes.c_void_p;kernel.LockResource.restype=ctypes.c_void_p
for name in ['Luma.exe','Luma-x64.exe','Luma-x86.exe']:
    path=root/'dist'/name;handle=kernel.LoadLibraryExW(str(path),None,0x22)
    try:
        resource=kernel.FindResourceW(ctypes.c_void_p(handle),ctypes.c_void_p(1),ctypes.c_void_p(14))
        size=kernel.SizeofResource(ctypes.c_void_p(handle),ctypes.c_void_p(resource));loaded=kernel.LoadResource(ctypes.c_void_p(handle),ctypes.c_void_p(resource));pointer=kernel.LockResource(ctypes.c_void_p(loaded));data=ctypes.string_at(pointer,size)
        count=struct.unpack_from('<H',data,4)[0]
        sizes=[data[6+14*k] or 256 for k in range(count)]
        verify(name+' embeds the tray design in all 8 icon resolutions',sizes==[16,20,24,32,48,64,128,256])
        verify(name+' is below the 50 MB release limit',path.stat().st_size<50_000_000)
    finally:
        if handle:kernel.FreeLibrary(ctypes.c_void_p(handle))
verify('Named product entry is byte-identical to the x64 release',(root/'dist/Luma.exe').read_bytes()==(root/'dist/Luma-x64.exe').read_bytes())
# Virtualized LocalAppData can expose another underlying INI when one file is
# moved. Never treat that as a fresh installation or move both user files.
if ini.exists() and ini.resolve().parent!=directory.resolve():
    (root/'artifacts').mkdir(exist_ok=True)
    (root/'artifacts/product-verification.json').write_text(json.dumps({'checks':checks,'skipped':'Fresh-install checks require a normal Windows terminal; this host virtualizes LocalAppData.'},indent=2))
    print('SKIP Fresh-install checks: run from a normal Windows terminal outside the packaged host.')
    raise SystemExit(0)
assert not session.exists(),'Close the existing Luma instance first'
backup=directory/('settings.product-test-'+uuid.uuid4().hex+'.ini')
assert ini.resolve().parent==backup.resolve().parent==directory.resolve()
had_ini=ini.exists()
original_ini=ini.read_bytes() if had_ini else None
if had_ini:ini.replace(backup)
process=None;current=None
try:
    for name in ['Luma.exe','Luma-x86.exe']:
        # Both launch without local preferences to exercise actual Config{}.
        if ini.exists():ini.unlink() # Only generated test settings; original is backed up.
        process=subprocess.Popen([str(root/'dist'/name),'--quiet'])
        current=wait(lambda:json.loads(session.read_text()))
        verify(name+' starts as a native standalone application',current['pid']==process.pid)
        state=api(current)
        verify(name+' fresh install uses every captured user parameter',all(state[k]==v for k,v in expected.items()))
        api(current,'settings',{'motion':0,'color':'#FFCB81'})
        api(current,'action',{'action':'reset'})
        restored=wait(lambda:api(current) if api(current)['motion']==expected['motion'] else None)
        verify(name+' Restore defaults returns the complete product preset',all(restored[k]==v for k,v in expected.items()))
        api(current,'action',{'action':'quit'});process.wait(timeout=10)
        verify(name+' exits cleanly',process.returncode==0 and not session.exists())
        process=None;current=None
finally:
    if process and process.poll() is None:
        if current:api(current,'action',{'action':'quit'})
        process.wait(timeout=10)
    if had_ini:backup.replace(ini)
    elif ini.exists():ini.unlink()

final=subprocess.Popen([str(root/'dist/Luma.exe'),'--quiet'])
current=wait(lambda:json.loads(session.read_text()))
verify('Final product EXE is running',current['pid']==final.pid)
verify('Original user settings file restored after fresh-install tests',ini.read_bytes()==original_ini if had_ini else all(api(current)[k]==v for k,v in expected.items()))
(root/'artifacts/product-verification.json').write_text(json.dumps(checks,indent=2))
