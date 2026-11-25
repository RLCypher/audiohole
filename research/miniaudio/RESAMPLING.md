# Sample-rate conversion: implementation and mathematics

Revision: miniaudio 0.11.25, `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`. **Verified source** unless marked otherwise. Source references [R1–R4, A1, G3](SOURCE_REFERENCES.md). Executed witnesses: [C probe](experiments/processing_probe.c), [results](experiments/processing_probe_results.txt).

## Available algorithms and integration

`ma_resample_algorithm` exposes linear and custom. Built-in linear accepts s16/f32 interleaved samples and optional low-pass filtering; it is not sinc/polyphase reconstruction. A custom vtable supplies heap/init/process/rate/latency/count/reset behavior. This revision removed the old built-in Speex option (see upstream CHANGES entry); do not promise Speex merely from older miniaudio tutorials. Selecting custom does not supply an algorithm by itself.

`ma_linear_resampler` owns configuration, x0/x1 frames per channel, integer/fractional phase, integer/fractional advance and filter state. `ma_resampler` wraps a backend/vtable; `ma_data_converter` combines format, channel and rate processing. Each is usable without device I/O, as the isolated build demonstrates. A route converter must enable `allowDynamicSampleRate` at initialization if equal nominal rates may later need drift correction. Device-created converters explicitly disable this flag. [R1, R4, G3]

Defaults differ:

- `ma_linear_resampler_config_init`: order `min(MA_DEFAULT_RESAMPLER_LPF_ORDER, MA_MAX_FILTER_ORDER)`, normally 4; Nyquist factor 1.
- `ma_resampler_config_init` and device config's resampling configuration: normally order 4.
- `ma_data_converter_config_init_default`: explicitly order **1**, dither none, dynamic sample rate false.
- Maximum filter order normally 8; order zero skips LPF work. These quality defaults are policy choices; no empirical optimization rationale was established. [R1, R4, A1]

## Exact phase/ratio calculation

Inputs: input rate `Fin` and output rate `Fout` positive unsigned integers (frames/s, or any proportional pair). Reduce by `gcd`: `N=Fin/g`, `D=Fout/g`. Consumed input-frame advance per output frame is `N/D`, decomposed as:

`aInt=floor(N/D)`, `aFrac=N mod D`.

Initial state is `inTimeInt=1`, `inTimeFrac=0`, x0=x1=0. Before generating each output, load/shift input frames while `inTimeInt>0`; each load decrements it. Generate an interpolated output when it reaches zero. Then:

```
inTimeInt  += aInt
inTimeFrac += aFrac
if inTimeFrac >= D:
    inTimeFrac -= D
    inTimeInt += 1
```

Phase remains bounded rather than accumulating a large absolute floating-point position. At constant sensible rates, this preserves an exact rational cadence independent of call boundaries; insufficient input stops the call with actual consumed/generated counts. The caller must honor both counts. NULL input means silence; NULL output permits seeking/discarding, with path-specific filter effects. [R1, R2]

Example 44.1→48 kHz: `N/D=147/160`, so output positions advance 0 whole frames plus 147 fractional units; the first two post-output phases are `(0,147)` then `(1,134)` before pending whole-frame loading. Over 160 generated outputs, the rational position advance totals 147 input frames, apart from initial history state. This is sample cadence, not a hardware drift estimate.

Interpolation for each channel is `y=x0+(x1-x0)*α`, where `α=inTimeFrac/D` in f32. The s16 branch computes `a=floor((inTimeFrac<<12)/D)` and an integer weighted interpolation at 12 fractional bits. Values and filter state retain only the chosen representation's precision. [R1, R2]

For a request for O>0 output frames in the current state, the input-count helper computes:

`required = inTimeInt + (O-1)*aInt + floor((inTimeFrac + (O-1)*aFrac)/D)`.

All counts here are frames, not samples or bytes. Use this state-aware helper in render-demand processing instead of `ceil(O*Fin/Fout)` alone; startup/history can change requirements. Huge 64-bit products are not universally overflow-protected. [R3]

## Actual filtering

On downsampling (including equality branch), input frames are filtered before interpolation when N≠D. On upsampling, interpolate then filter output. Equality bypasses filtering to avoid the source-commented glitch risk. Filter design uses reduced rates:

`Fs_filter=max(N,D)`

`fc=0.5*min(N,D)*lpfNyquistFactor`.

Only fc/Fs matters for coefficient design, so rate reduction preserves normalized cutoff. The factor defaults to 1. It sets cutoff at the lower Nyquist, not a specified transition band or guaranteed stopband attenuation. [R1, R2]

Order L is split into one first-order section for odd L and floor(L/2) second-order sections. For second-order index i beginning at 0:

- Odd L: `θ=(1+i)*π/L`.
- Even L: `θ=(1+2i)*π/(2L)`.
- `Q=1/(2*cos θ)`.

For L=4, Q values are about 0.541196 and 1.306563. This arranges Butterworth-style section Q values. First-order coefficient is `a=exp(-2πfc/Fs)`; f32 processing uses `y=(1-a)x+a*yPrevious`. Second-order coefficients use `ω=2πfc/Fs`, `α=sin(ω)/(2Q)`:

```
b0=(1-cos ω)/2; b1=1-cos ω; b2=b0
a0=1+α; a1=-2*cos ω; a2=1-α
```

Coefficients are normalized by a0. Runtime f32 is direct-form-II transposed:

```
y  = b0*x + r1
r1 = b1*x - a1*y + r2
r2 = b2*x - a2*y
```

Coefficient construction uses double intermediates, then stores f32 or fixed-point s16-path coefficients; processing state is per channel. Two multiplies and an add for interpolation are only part of cost; filtering costs scale with processed frames × channels × sections. Downsampling filters all consumed input, upsampling filters generated output. [A1]

## Latency and quality interpretation

`ma_linear_resampler_get_input_latency` returns `1 + ma_lpf_get_latency`, and the LPF reports `2*lpf2Count + lpf1Count`, i.e. order. Output latency helper returns integer `inputLatency*D/N`. Normally order 4 yields a reported five input frames; at 48 kHz this converts arithmetically to 0.10417 ms. This is a library reporting convention, **not** a measured end-to-end delay or frequency-independent IIR group delay. At equal rates LPF processing is bypassed even though its configured order may still affect the reported count. [R3, A1]

Linear interpolation's high-frequency droop, LPF attenuation and non-linear phase must be measured for AudioHole's signal requirements. Increasing order adds work and filter state; order zero removes anti-alias filtering. Neither setting is automatically acceptable for wide-ratio conversion. Near-unity adaptive SRC needs continuity/transition tests, especially when the ratio crosses exactly 1 and filtering switches branch/bypass. No quality or CPU ranking was measured here.

## Rate changes: two reproduced limitations

### Generic ratio convenience API is too coarse for small drift

`ma_resampler_set_rate_ratio(r)` computes `n=(ma_uint32)(r*1000)` and passes n,1000 to the backend. `ma_data_converter_set_rate_ratio` delegates to it. The **direct** `ma_linear_resampler_set_rate_ratio` uses denominator **1,000,000**. This asymmetry is source-verified and executed:

| Requested f32 ratio | Generic effective ratio | Direct linear effective ratio |
| --- | --- | --- |
| 1.000100017 (+100 ppm approximately) | 1.000000000 | 1.000100000 |
| 0.999899983 (−100 ppm approximately) | 0.999000000 | 0.999900000 |
| 1.000000954 (about +0.954 ppm) | 1.000000000 | 1.000000000 |

The generic helper's grid is 0.001, or 1000 ppm near unity, with truncation bias; direct linear nominal grid is 1 ppm, subject to float representation and truncation. Integer-pair `set_rate` can express finer rational corrections without that convenience quantization, but encounters the next risk. [R3, R4, E1]

### Phase rescaling product wraps before division

Rate changes call LPF reinit preserving history, recompute advance, then scale fractional time for the new denominator. Intended calculation, with old remainder f and old/new denominators Dold/Dnew:

`fNew = floor(f*Dnew/Dold)` plus any old whole part, then normalize.

Actual `oldRateTimeFract * newSampleRateOut` multiplies **two 32-bit unsigned operands**. Wide mathematical product can wrap modulo 2^32 before division. Public-API probe initializes 1000001/1000000, generates 10000 outputs, reaches f=10000, then changes to 1000003/1000000:

- Mathematical product: 10,000,000,000; expected remainder 10000.
- 32-bit wrapped product: 1,410,065,408; actual remainder 1410.
- Fractional phase changed by −0.00859 input frame despite unchanged denominator.

This test does not write private state and runs unmodified upstream code. Its small ratio changes are chosen to expose arithmetic, not to imply that hardware has a million-Hz sample rate: proportional rate pairs are legitimate SRC parameters. The general setters accept them. The same overflow class is relevant to finely quantized drift actuators. Output artifact size and audibility remain unmeasured. [R1, E1]

**Disposition:** Reject generic ratio convenience setter for ppm control at this revision. Investigate wide-product phase repair, fine ratio precision, finite/range validation, and continuous filter transitions before adopting an adaptive path. No fix was applied to upstream or production. A different/custom resampler remains a valid comparison candidate; merely bypassing generic quantization does not clear all dynamic-use concerns.

## Numerical boundaries

Positive zero-rate validation exists, but comparisons such as `ratio<=0` do not establish finite/NaN/range safety before floating-to-unsigned casts. Very large denominators can also overflow `(inTimeFrac<<12)` in s16 interpolation, or phase additions; actual safe ranges depend on reduced N/D. Keep configuration bounded and validate externally. Normal 44.1/48 kHz reduced pairs are small, so these observations do not establish corruption of ordinary fixed-rate playback. Exact rational phase avoids long-run floating position growth; it does not prove arbitrary-rate arithmetic or IIR coefficient stability.
