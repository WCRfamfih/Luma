# Windows validation

Build both architectures first. Python 3 and Pillow are required (`python -m pip install Pillow`). Run commands from the repository root on a Windows desktop with a Direct3D 11 device. These are native integration checks, not a browser approximation.

Exit Luma using its tray menu, then generate current rendering evidence:

```powershell
python tests/prepare_evidence.py
python tests/render_properties.py
python tests/verify_aa.py
python tests/verify_heat.py
python tests/verify_rim.py
python tests/verify_bloom_quality.py
python tests/verify_breath.py
```

The generator leaves Luma running. `artifacts/` contains generated PNGs and reports and is ignored by Git. The two PNGs in `fixtures/` are original Luma renders from earlier implementations, retained solely as regression baselines. `factory-defaults.json` is the sanitized product preset, without user position, audio data or session credentials.

For live audio, API validation and a brief performance sample, play audio through the default Windows output device and run:

```powershell
python tests/verify.py
python tests/benchmark_aa.py
python tests/release_smoke.py
```

These checks temporarily change parameters and restore them; do not adjust settings concurrently. The frame-rate checks assume a GPU capable of reaching 60 FPS and are not universal hardware guarantees. `release_smoke.py` restarts the app and tests occupied-port fallback and the x86 executable.

Finally, exit Luma from the tray and run `python tests/verify_product.py` to verify EXE icon resources, fresh-install defaults and reset behavior on both architectures. It backs up the original settings file, restores it in `finally`, and leaves the product EXE running. Use a disposable Windows account for unattended testing that must also withstand forced interruption.

Run fresh-install tests from a normal Windows terminal. If a packaged host virtualizes LocalAppData, the script verifies EXE resources but explicitly skips file-isolation checks rather than risking another copy of user preferences.

With the newly built app running, `python tests/verify_topmost.py` checks the tray command, actual native topmost style, independent mouse passthrough, persistence across x64/x86 restarts, and the complete reset preset. It restores preferences and position afterward.
