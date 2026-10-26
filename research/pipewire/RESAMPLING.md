# Adaptive resampling

This file reconstructs the native float resampler and its actual use through audioconvert/audioadapter in PipeWire 1.4.9. References [S04–S06](SOURCE_REFERENCES.md#s04) cover the verified implementation. No claim is made that this converter runs for every graph, format, or backend.

## Control and frame-count contract

**Verified:** `audioadapter.c:link_io()` shares one `spa_io_rate_match` between the hardware port and conversion port. ALSA can write flags/rate; audioconvert writes required size and delay. `resample_is_passthrough()` rejects passthrough if nominal rates differ, rate scaling differs from 1, explicit adjustment is active, or RateMatch ACTIVE is set. An explicit resampling-disabled property can override that path; disabling required conversion is not a general stability guarantee.

`resample_update_rate_match()` constructs the resampler's **multiplicative rate parameter**:

\[
r={\text{DSP rate scale}\over\text{props.rate}}
  \times\begin{cases}\text{RateMatch.rate},&\text{ACTIVE}\\1,&\text{otherwise.}\end{cases}
\]

The scale is applied only when the opposite port operates in DSP/graph-rate mode. `impl_node_process()` updates it for graph-rate differences. This `r` is **not** simply input frames divided by output frames. Nominal rates are separate fields. For graph→node conversion the actual output count varies; for node→graph the required input count varies. Fixed destination demands therefore need variable source consumption even for nominally equal-rate independent devices.

The converter queries `resample_in_len()` or `resample_out_len()` depending on direction, subtracts already queued frames with saturation at zero, and reports integer delay plus `delay_frac` in billionths of a **node frame**. `delay_frac/1e9` is not seconds. The base native delay is in input frames; output-direction reporting scales it to node rate. [S03, lines 308–350; S05, lines 2159–2215.]

## Rate update and precision

**Verified, `impl_native_update_rate()`:** for nominal input `I`, output `O`, and `g=gcd(I,O)`:

| Condition | Effective rates used by phase advancement | Selected kernel |
|---|---|---|
| `r != 1` | `I'=uint32_t(I/r)`, `O'=O` | interpolated phase-bank FIR |
| `r == 1 && I == O` | `I'=I`, `O'=O` | copy kernel inside the native resampler |
| `r == 1 && I != O` | `I'=I/g`, `O'=O/g` | exact rational phase-bank FIR |

It then sets `inc=I'/O'` integer quotient and `frac=I'%O'`. When changing `O'`, it rescales existing phase by `O'_new/O'_old` using floating arithmetic. A rate update selects a kernel and changes phase increments; it does not allocate/rebuild the filter on every update. [S06, native.c lines 139–175.]

**Calculated implication:** for nonunity updates, positive division truncates toward zero. At 48 kHz, `r=1.000001` maps to `I'=47999`, giving output/input factor `48000/47999≈1.00002083377` rather than 1.000001. `r=.999999` maps to 48000 and gives factor 1. At `r=1.0001`, the factor is `48000/47995≈1.00010417752`. Thus the effective rate is quantized in steps of order `1/48000≈20.8 ppm` and has direction-dependent rounding bias.

**Inferred:** closed-loop feedback can alternate between adjacent steps to achieve a correct long-term average; that can leave a small occupancy limit cycle or modulation. **Experimental:** the isolated quantized actuator model shows bounded small oscillation, but it does not establish audibility or native sample output quality. There is no evidence here of explicit ratio slew limiting, dithering, or a dedicated anti-windup mechanism in this native rate-update function.

## Phase and interpolation

**Verified:** full rational processing uses an integer local phase, while interpolated processing uses float phase. Persistent `native_data.phase` is a float. Each output advances:

\[
i\leftarrow i+\mathrm{inc},\quad p\leftarrow p+\mathrm{frac};\quad
\text{if }p\ge O':\ p\leftarrow p-O',\ i\leftarrow i+1.
\]

The phase stays bounded rather than being a growing stream-time counter. The interpolated kernel maps phase to table position `ph=p×pm`, where `pm=n_phases/O` from initialization; `j=floor(ph)` and `α=ph−j`. The scalar kernel calculates two dot products and returns:

\[
y=\sum_n x_{i+n}h_{j,n}+\alpha\left(\sum_n x_{i+n}h_{j+1,n}-\sum_n x_{i+n}h_{j,n}\right).
\]

This interpolates neighboring **filter responses**, not merely neighboring raw audio samples. The full kernel uses one phase-bank dot product. Both stop when insufficient input exists for all taps or output capacity is reached and report consumed/produced lengths. History and residual phase survive block boundaries. [S06, native-impl.h lines 78–143; native-c.c.]

**Inferred limits:** bounded phase prevents unbounded timestamp accumulation inside the resampler, but float/integer operations still have finite precision. Float storage of integer phase is exact only within its integer precision range; realistic audio rates are small, but arbitrary extreme rates are not justified. Integer products in length calculations and allocation-size calculations also need bounded caller inputs. This inspection does not prove behavior for zero, nonfinite, negative, or out-of-range rate factors. Those should be rejected before an AudioHole ASRC call.

## Filter construction and delay

**Verified:** quality is clamped to 0–14, with default 4. Quality 4's table entry has base taps 48 and cutoff .85. Let reduced nominal ratio be `I_r=I/g`, `O_r=O/g`:

\[
s=\min(.85\,O_r/I_r,.85),\qquad
N=\min(\operatorname{roundUp8}(\lceil48/s\rceil),2^{18}).
\]

At other qualities, substitute their base-tap/cutoff pair. At equal 48 kHz, `N=64`; at 48→44.1 kHz it is also 64 with scale .7809375. The number of phases is `P=O_r×floor((255+O_r)/O_r)`, yielding at least 256 phases. This is 256 for equal rates and 294 for 48→44.1 kHz. The table includes phase `P` for neighboring-bank interpolation. Filter strides and histories are aligned to 64 bytes; taps are multiples of eight for SIMD.

The active window is `window_cosh`, not the alternative Blackman function in the same source. With `A=16.97789`, centered positive distance `x`, and `u=2x/N`:

\[
W(x)=\begin{cases}
\frac{\exp(A\sqrt{1-u^2})-1}{\exp(A)-1},&u^2<1\\0,&u^2\ge1,
\end{cases}\quad
h(x)=s\,\operatorname{sinc}(sx)W(x).
\]

The local sinc returns 1 for its near-zero nonnegative argument (`x<1e-6`); otherwise it evaluates `sin(πx)/(πx)`. Construction exploits symmetry. A comment cites DOI `10.1109/RME.2008.4595727` "with tweak"; this investigation did not inspect that paper or establish derivation of A, the tweak, or quality-table values. Precomputed matching filters can be used, otherwise coefficients are built at initialization. [S06, native.c lines 21–83, 336–435.]

`impl_native_delay()` returns `N/2−1` input frames, and `impl_native_phase()` reports fractional phase/history effects. It contains source comments admitting the reason for a one-frame history adjustment is unclear; do not turn that comment into a claimed proof. Default equal-rate base delay is `31/48000=0.645833 ms`, plus phase/reporting details. A top-level audioconvert bypass can avoid native processing entirely when appropriate; the native copy kernel itself still participates in native history handling.

`impl_native_reset()` clears history, uses `N/2` zero history frames normally or `N−1` under PREFILL, and zeros phase. `impl_native_process()` refills/moves fixed history, can produce zero frames on insufficient input, and can ask the caller to resubmit remaining input. Allocation occurs in `resample_native_init()`; frees occur in teardown. These exact contracts matter more than assuming every call consumes one block.

## Performance and AudioHole relevance

**Verified:** native dispatch includes scalar float and optional SSE/SSSE3/AVX-FMA/NEON implementations selected from CPU flags. Adjacent-bank processing does more work than a single bank. Complexity is proportional to channels × output frames × taps, with bounded history copies. This is operation-count reasoning, not a measured CPU benchmark. [S06.]

**Experimental recommendation:** keep one variable-ratio stateful resampler at each independent clock boundary; combine nominal conversion and drift in one ratio to avoid cascaded filters where feasible. Require APIs for exact consumed/produced counts and input-frame delay. Compare an existing backend ASRC against an application-owned candidate before selecting D-034. Verify transition behavior near unity, channel continuity, impulse delay, aliases, modulation, and sustained sub-20-ppm mismatch. Preserve phase on ordinary updates; reset only as part of an explicit discontinuity/re-prime operation. Do not copy this native converter into production without separate approval and license review.
