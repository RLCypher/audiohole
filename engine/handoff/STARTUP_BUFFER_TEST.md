# Startup buffering and underrun diagnostics — validation 0.4

This is a targeted follow-up to the wired 0.3 comparison. It does not establish
a new end-to-end latency result or fix the intermittent ongoing underruns. The
0.3 packages remain unchanged for comparison.

## Hardware test

1. Extract this package separately and copy the SAME wired profile into `Profile`.
2. Run `Run-Validation.cmd`, select that profile and Start. After at least
   30 seconds, take several LagMeter readings with unchanged phone placement,
   speaker volume and app gain. Keep routing running approximately 15 minutes.
3. Stop, confirm `Stopped`, and Quit. Keep the corresponding `.txt` or `.jsonl`.
4. Repeat with `Run-Low-Latency.cmd` and the same profile/setup, also about
   15 minutes. Do not run the two processes simultaneously.
5. Send each run's log and its typical/range of LagMeter readings. Note audible
   clicks/dropouts and whether Stop removes the app's returned signal.

The 15-minute observation is for intermittent behavior, not a demonstrated
acceptance threshold. Both launchers still require explicit Start. There is no
test beep, synthetic device, GUI change or automatic profile rewrite.

## What changed

Before any captured PCM has been transferred, an automatic Shared client can
request more output than one normal period, for example an empty 1,056-frame
buffer instead of the normal 480-frame refill. In 0.3 that request taught the
FIFO controller a permanent `480 + 1056 + 2 = 1538`-frame / 32-ms target.

Version 0.4 restores the requested native lead with a counted zero prefix and
at most one period of pipeline output at the end. With a 1,056-frame startup
request and a 480-frame period, that is 576 prefix frames plus 480 pipeline
frames. The ordinary unprimed pipeline can still emit silence while collecting
its safety reserve. This is startup priming, not additional source-derived audio.

This treatment ends immediately after the first source-derived output, including
valid source silence. A genuinely large request AFTER that still reaches the
existing safety guard in full. Explicit `buffer_ms`, Exclusive mode and unknown
or mismatched period timing retain their previous startup behavior. No unread
source samples are discarded or live queue targets arbitrarily reduced.

For the previous automatic 48-kHz wired configuration, the expected startup-only
target is approximately 962 frames / 20.04 ms in Standard and 960 / 20 ms in Low,
rather than Standard's observed 1,538. A larger request occurring after valid
transfer can still raise the target: that is deliberately not hidden or assumed
to be startup. Hardware logs are needed to verify which case applies.

## New counters and event context

- `native_initial_silence_frames`: initial native preroll before Start.
- `native_startup_padding_silence_frames`: extra prefix zeros used by this change.
- `native_submitted_frames`: all successfully submitted native frames, including
  preroll and prefix, not proof they were acoustically played.
- `native_render_request_max_frames`: largest actual native write after preroll.
  Existing `render_request_max_frames` describes the pipeline's PCM requests;
  these can differ during startup.
- `last_underrun`: the most recently observed FIFO-starvation context, retained
  across non-RT polls. It identifies the failing block's requested/produced
  frames, available source frames, source-published/frame indexes, active target
  and requested correction. Native context includes render-call QPC, last valid
  capture position/QPC, last capture drain gap, last valid packet age, padding
  and the actual native write request. These are not acoustic timestamps.
- `dropped_underrun_contexts`: new event contexts dropped when the fixed eight-slot
  audio-to-observer mailbox was full. Audio never waits for telemetry. Snapshots
  retain only the latest consumed context; sequence/counter gaps can also show
  events occurring between host snapshots. This is not a complete event trace.

After a clean Stop with successful submissions, accounting should satisfy:

```
native_submitted_frames = native_initial_silence_frames
                        + native_startup_padding_silence_frames
                        + rendered_frames
```

Counters sampled during Running can span an in-flight call; backend submission
failure can also break this equality. `rendered_frames` is pipeline output,
whereas `transferred_frames` counts source-derived frames. Native silence must
not be interpreted as valid transferred audio or quietly lost in the logs.

Packet age is last valid capture timestamp to receipt. Render QPC is call entry,
not the exact first starving sample or physical playback. Position and frame
indexes require interpretation across discontinuities/drops; these fields do
not by themselves measure clock drift or provide sample provenance end to end.

Native handling changes are confined to WASAPI and its Experimental adapter.
Miniaudio gains generic FIFO event diagnostics only; no native startup policy
is imposed on its callbacks. All original profile requirements remain binding.
