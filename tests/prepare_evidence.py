"""Generate current native GPU frames; leaves Luma running for live API checks."""
from pathlib import Path
import json,os,shutil,subprocess,time

root=Path(__file__).resolve().parents[1]
data=Path(os.environ['LOCALAPPDATA'])/'Luma'
out=root/'artifacts';out.mkdir(exist_ok=True)
assert not (data/'session.json').exists(), 'Exit Luma from the tray before generating evidence.'
started=time.time()
process=subprocess.Popen([str(root/'dist/Luma.exe'),'--quiet','--render-evidence'])
try:
    deadline=time.monotonic()+60
    while time.monotonic()<deadline:
        assert process.poll() is None, 'Luma exited before rendering evidence'
        last=data/'snapshot.png'
        if last.exists() and last.stat().st_mtime>=started:
            time.sleep(.3)
            break
        time.sleep(.2)
    else:raise TimeoutError('Native render evidence timed out')
    frames=[p for p in data.glob('*.png') if p.stat().st_mtime>=started]
    assert len(frames)>=70, f'Expected native diagnostic frames, got {len(frames)}'
    for path in frames:shutil.copy2(path,out/path.name)
    for path in (root/'tests/fixtures').iterdir():shutil.copy2(path,out/path.name)
    shutil.copy2(out/'halo-current-after.png',out/'native-particles.png')
    print(f'Prepared {len(frames)} fresh native frames. Luma remains running for API tests.')
except BaseException:
    # This process was launched here; avoid leaving a failed diagnostic run alive.
    if process.poll() is None:
        process.terminate();process.wait(timeout=10)
    raise
