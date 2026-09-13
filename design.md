# Luma architecture

Luma is a native C++17 Windows desktop process. It embeds its settings page, shaders, icon and version resources into a statically linked executable.

## Audio and motion

`src/audio.h` captures the default multimedia output through WASAPI loopback and performs a 2048-point FFT with 64 logarithmic bands. Transient detection distinguishes attacks from sustained energy. Low-frequency travelling waves and general drum impulses have independent histories, so occupied wave slots do not suppress new particle kicks. `src/breath.h` supplies bounded, decaying light modulation.

Music drives global spring displacement. The exterior has its own smooth field, with wave coupling defaulting to 20%. Interior impulses blend driven response with freer damped springs. Orientation evolves continuously in three dimensions. Equal-area particle anchors with stable jitter and curved displacement avoid a latitude/longitude lattice and pole crowding.

## Rendering

`src/particles.hlsl` generates the particle field procedurally. The exterior remains a soft volume during rotation; variable-width inward folds vary the luminous band's thickness. Interior layers have independent density and opacity limits.

`src/graphics.h` owns Direct3D 11 resources and a premultiplied transparent DirectComposition swap chain. The particle pass accumulates material energy in FP16. `src/post.hlsl` extracts and filters a half-resolution atmospheric buffer using separable 17-tap stretched-exponential weights: `exp(-6*abs(x/R)^1.65)`. Filter footprint follows tap spacing. Input energy is softly limited and low-energy tails decay faster.

The halo is composited behind the core with a maximum alpha of 60%. Dense heating uses material-normalized coverage above a single-particle threshold, independently of global Bloom and beat intensity. A half-resolution neighborhood also supplies the adjacent heated shoulder; distant scattering retains the chosen color.

Four-sample particle coverage and a compact directional FXAA pass operate without history frames. Sparse particles receive less smoothing. FP16 is retained until final conversion, where small static correlated color/alpha dither reduces coherent banding while preserving exact transparent pixels.

The transparent native window adds `ceil(40*bloomSpread)+16` pixels of margin per side. Settings control the logical artwork size; rendering, dragging, hit testing and monitor constraints account for the extra margin.

## Desktop and settings

`src/main.cpp` manages the Win32 overlay, tray, persistence, frame pacing and local HTTP server. `web/settings.html` contains the embedded settings UI and a lightweight Canvas structural preview, not the full native postprocessing pipeline.

The server binds only to loopback, validates Host and Origin, and requires a per-session token for API access. Settings are validated before atomic application and saved under LocalAppData. The renderer follows output-device changes and reduces work while quiet or hidden.

## Build and validation

`build.ps1` finds MSVC and the Windows SDK, generates eight native icon sizes and builds either architecture. `package.ps1` emits the portable ZIP and SHA-256 checksums. CI builds both architectures and runs numerical self-tests. Native desktop validation and generated rendering evidence are documented in [tests/README.md](tests/README.md).
