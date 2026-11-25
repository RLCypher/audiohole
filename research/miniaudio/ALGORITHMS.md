# Conversion algorithms, constants and numerical limits

Pinned revision and evidence categories: [README](README.md). Source keys [A1–A3, R1–R4, B1–B3, G2–G3](SOURCE_REFERENCES.md). SRC equations and state update are in [RESAMPLING](RESAMPLING.md); period negotiation is in [WASAPI_BACKEND](WASAPI_BACKEND.md); queue conservation/controller proposal is in [DUPLEX_AND_CLOCKS](DUPLEX_AND_CLOCKS.md). This document adds actual conversion dispatch, quantization and boundary behavior without duplicating those derivations.

## Converter execution plan

**Verified.** `ma_data_converter_config_is_resampler_required` returns true when rates differ or `allowDynamicSampleRate` is true. Initialization selects an intermediate format: choose output s16/f32 if possible, otherwise input s16/f32, otherwise f32. A custom resampler forces f32 when needed because that is the guaranteed backend representation. Set up channel and rate processing only where required, then choose an execution path:

```
if no format/channel/rate operation: passthrough
else if channels increase: resample first, then expand channels
else if channel conversion exists: reduce/remap channels first, then resample
else: rate-only or format-only
```

Within composite paths pre/post format conversions are inserted only when representation differs from intermediate. Reducing channel count before SRC, expanding after SRC, reduces repeated per-channel resampling work. This optimization is supported by dispatch code; CPU savings are not benchmarked. Passthrough copies with actual count accounting; it is not a promise every upstream/downstream stage shares the same memory. [R4]

Inputs: interleaved frames and capacities, format enum, channel maps/counts, input/output rates. Outputs: written interleaved frames and **actual** consumed/generated frame counts. A frame contains C samples; `bytes=Nframes*C*bytesPerSample`. Integer format changes or mapping do not create a queue by themselves. Any required SRC carries history. Do not invoke one-shot allocating convenience conversion per real-time block; initialize reusable converters once and respect allocation callbacks/preallocated APIs.

## Sample representation and bit depth

**Verified.** Public scalar s16→f32 reference branch uses `y=x/32768`, so −32768 maps −1 and +32767 maps 0.999969482421875. Its optimized path implements that scale. Scalar f32→s16 conversion applies optional dither, clips to [−1,1], multiplies by 32767, then casts to s16 (truncation toward zero). Thus −1 maps −32767 there, and exact s16 round-trip equality is not universal. Do not substitute a textbook symmetric mapping for these actual constants. [A2]

Packed s24 is three bytes; helpers sign-extend by constructing a signed wide representation with shifts. Integer narrowing uses high bits/right shift when no dither, so quantization/negative rounding follows the C/target signed-shift behavior used by source. s32→f32 cannot preserve all 32 integer precision bits in f32's approximately 24-bit significand; repeated conversions can lose low bits. Matching representations permits copy bypass. Windows valid-bits-in-container metadata is not fully represented by miniaudio's small format enum, and exact support must be checked through the backend's format parsing/probing.

Vectorized public conversion dispatch uses SSE2 or NEON where supported and may fall back on alignment/tail length. For f32→s16, SSE2 bulk loads require 16-byte aligned buffers, multiply by 32767, convert with truncating vector instructions, then saturating pack; scalar tails explicitly clamp before conversion. The bulk code is not identical to scalar clipping for arbitrary nonfinite/huge input. This was **source inspected, not exhaustively tested**; finite range and equivalent rounding across scalar/vector paths belong in validation. A scalar waveform helper with similar name directly casts and is not the same contract as the public bulk converter. [A2]

Dither defaults off. Rectangle draws one uniform value between format-specific noise bounds. Triangle adds an independent draw from [lo,0] and one from [0,hi]; f32→s16 bounds are −1/32768 and +1/32767. Random generation uses a shared LCG state through `g_maLCG`. There is no per-route PRNG state in the conversion API inspected here; deterministic multi-thread use and synchronization require review if dither is enabled. Dither adds arithmetic/random calls and noise; it is not automatically beneficial for every float route. No noise-floor measurements were performed. [A2]

## Channel conversion and mixing

**Verified.** Path selection checks identical maps/counts first, then mono-out, mono-in, custom weights, same-position shuffle, otherwise weights. Thus mono special cases may precede custom-weight selection; do not assume every config always reaches a requested weight matrix. Shuffle matches channel labels; unmapped samples are silenced. Mono input duplicates to outputs. s16/s24/s32/f32 mono-out sums the input channels and divides by **input** channel count (integer division for integers). With two f32 samples 0.2 and 0.6, output is 0.4. The u8 branch's divisor/offset handling differs and was not reconstructed as equivalent; no blanket “all formats average identically” claim is made. [A3]

Weights initialize to zero, exact/shuffle-matched input/output pairs receive 1. For rectangular spatial conversion, unmatched spatial channels can contribute according to a six-plane table. Each speaker spreads unit contribution equally over its nonzero front/back/left/right/top/bottom planes. Pair weight is the dot product:

`w(A,B)=Σ(k=0..5) planeRatio[A,k]*planeRatio[B,k]`.

For front-left and side-left, shared left contribution is (1/2)*1=1/2. Front-left to back-left is (1/2)*(1/2)=1/4; exact front-left matching receives 1 by the mapping stage, rather than the dot product's 1/2. These are the library's geometric weights, not a standardized surround downmix law. If requested and no LFE input exists, LFE can average spatial input channels with weight 1/spatialCount; default configuration does not request this synthesis. [A3]

Runtime f32 matrix mixing is `y_j=Σ_i w_ij*x_i`; it can exceed unity. Integer matrix paths use 12-bit fixed-point weights, multiply/shift and saturating accumulation, with wider intermediate types for wider samples; repeated saturation can be order-dependent. f32 device playback clips by default after the callback, but standalone channel conversion does not establish a clipping contract. Matrix cost scales O(frames*Cin*Cout) and initialization allocates O(Cin*Cout) weights. Shuffle/mono/passthrough are cheaper structural paths. [A3, G2]

### Reproduced packed-s24 shuffle defect

An additional public-API witness uncovered a problem in `ma_channel_map_apply_shuffle_table_s24`. Closing-brace placement makes second-byte zeroing unconditional per channel and a third-byte zero write occur **after** the channel loop. A one-frame stereo shuffle expects six output bytes but writes a zero at byte offset 8. The probe uses a 12-byte guarded array so it can detect the violation without a physical allocation overrun.

Input bytes `[01 02 03 | 04 05 06]`, maps front-left/front-right → front-right/front-left:

- Expected logical output: `[04 05 06 | 01 02 03]`, guard unchanged.
- Actual: `[04 00 06 | 01 00 03]`, guard byte at offset 8 changed from AA to 00.

This is **Verified source and executed software behavior**, not a hypothetical precision concern. It affects this direct channel-shuffle branch; it does not prove every data-converter path uses packed s24 internally (many choose f32). With exactly sized caller memory the observed write extends beyond the output contract. **Reject** this packed-s24 shuffle path for reuse at the pinned revision pending an independently reviewed fix/alternative. No upstream modification was made or report sent. [A3, E1]

## Numerical stability and validation boundaries

| Mechanism | Actual protection or limitation | AudioHole consequence |
| --- | --- | --- |
| Rational phase | Bounded integer/fraction state avoids ever-growing float position | Adopt concept for long-running cadence; rate-update multiplication still wraps in probe. |
| Fine ratio setter | Generic denominator 1000, direct linear 1e6, float truncation | Reject generic helper for ppm use; validate any alternative actuator. |
| Phase update | 32-bit product before division; s16 fraction shift can overflow at huge reduced denominators | Wide arithmetic/range limits need review; ordinary reduced 147/160 is not the demonstrated failure case. |
| IIR | double coefficient construction; f32 runtime state, or 14-bit fixed-point coefficients for s16 | Rounding, poles near unity/Nyquist, coefficient changes and integer intermediate ranges need tests; not a universal stability proof. |
| NaN/Inf/extreme values | Many comparisons do not reject NaN; casts/range behavior differs by path | Validate finite configuration; define invalid PCM policy; default clipping is not a complete NaN sanitizer. |
| Frame/byte calculations | Some size checks and 64-bit products, others 32-bit multiplication | Bound channels, rates, ms, capacities; check multiplication before allocation. |
| Ring offsets | Restricted subbuffer size, lap bit, atomics | One producer/consumer; acquire/commit sizes and teardown ownership still caller responsibilities. |
| Count helpers | O(1) state-based arithmetic, large products not universally checked | Use bounded block requests; do not feed absolute multi-day counts as a block. |
| Denormals | Device callback wrapper disables/restores denormal handling unless configured otherwise | Standalone DSP does not necessarily inherit that wrapper; validate quiet tails and CPU behavior. |

## Constants and rationale ledger

| Constant/value | Role and units | Origin/classification | Applicability |
| --- | --- | --- | --- |
| 1000, 10^6, 10^7, factor 10 | ms/s, µs/s, 100 ns/s conversions | Mathematically derived units | Adopt with correct integer widths. |
| gcd reduction, quotient/remainder phase | Exact nominal rational cadence | Mathematical, rates configurable | Adopt principle; not clock control. |
| 10 ms / 100 ms default periods | Low-latency/conservative requests | Configurable compile defaults; empirical rationale unknown | Test actual negotiated result. |
| 3 periods | Default requested buffer multiplicity | Configurable compile default; rationale unknown | Not a universal stable latency choice. |
| Client3 fundamental/min/max | Legal engine periods, frames | Runtime OS/driver-dependent | Query and report, not hardcode. |
| 5000 ms wait; loopback 10 ms | Stall/retry waits | Backend constants, first macro overrideable; rationale incompletely documented | Separate from service period and Stop behavior. |
| 500 ms exclusive retry comparison, duration doubling | Initialization fallback | Hardcoded backend policy; precise empirical rationale unknown | Log effective settings; don't hide large negotiated requests. |
| 4096 bytes scratch | Conversion chunk size | Compile default, rationale unknown | Predictable stack; varies frames by format/channels. |
| Five / two capture periods | Async duplex ring capacity/prime | Hardcoded tuning, rationale unknown beyond desync margin | Not WASAPI drift compensation; mixed-rate priming units need review. |
| LPF order 4 / converter 1 / max 8 | SRC filter complexity | Configurable/default choices, rationale unknown | Quality/CPU study; do not conflate defaults. |
| Nyquist factor 1 | Cutoff relative to smaller Nyquist | Configurable linear field; rationale unknown | Transition-band quality unmeasured. |
| Q section formulas; 2π | Filter design | Mathematical construction | Verify coefficient quantization/poles over supported range. |
| 12 fractional bits interpolation/channel weights; 14 filter bits | Integer path precision | Hardcoded implementation tradeoff; rationale unknown | Test noise, saturation, overflow. |
| 1000 vs 1e6 ratio denominators | Ratio quantization | Hardcoded; difference rationale unknown | Critical for ppm control. |
| 64 WCHAR ID / 64 format entries | Bounded device metadata | Fixed representation; format-list capacity comment is explicitly uncertain | Not exhaustive enumeration/identity. |

Every “Adopt” here means a research recommendation to use a principle. Source reuse or production dependency remains separately authorized, with [license review](LICENSING.md).
