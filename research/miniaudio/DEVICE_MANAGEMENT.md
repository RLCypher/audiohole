# Device management, diagnostics and recovery

Pinned upstream: [README](README.md). **Verified source** unless marked. References [D1–D3, W1–W4](SOURCE_REFERENCES.md).

## Identity and capability limits

`ma_device_id` is a backend-tagged-in-use union; its WASAPI member is `ma_wchar_win32 wasapi[64]`. Desktop enumeration gets the opaque string from `IMMDevice::GetId`, copies it into this member, and obtains friendly name from `PKEY_Device_FriendlyName`. Detailed information records sample format/channels/rate and an exclusive-mode flag for a bounded list; it does not store AudioHole's vendor/product/serial/interface fingerprint, logical ID, capability history or confidence states. Fixed string capacity and copy-result handling deserve validation for unusually long endpoint IDs; no truncation failure was measured here. [D1, D2]

The enumeration path lists only active endpoints. Disabled/unplugged/saved unavailable devices require AudioHole's registry or native enumeration. Default detection and resolution use `eConsole`. Stable across a particular restart may occur, but the library supplies no Windows reinstall/driver migration/portable-identity guarantee. [Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nn-mmdeviceapi-immnotificationclient) specifies opaque endpoint IDs and string equality, not a parseable hardware fingerprint. Treat these strings as machine-local bindings, never parse them for USB identity.

Detailed info calls `GetMixFormat` for shared mode; exclusive info first tests the device-format property, then searches prioritized formats/rates at the property channel count until **one** candidate succeeds. It is neither exhaustive nor a runtime reliability test. Initialization has a narrower exclusive path than information retrieval. The source's “nativeDataFormats” name must not override AudioHole's reported/accepted/verified evidence taxonomy. Shared-mode format and successful API queries do not prove ADC/DAC physical format or transport behavior. [D2, W2]

## Notification callbacks: what actually happens

A per-device IMMNotificationClient is registered if possible. Failed registration is tolerated, reducing automatic routing functionality without failing device initialization. [W2]

| Callback | Examined desktop behavior |
| --- | --- |
| OnDeviceAdded | No-op. |
| OnDeviceRemoved | No-op. |
| OnPropertyValueChanged | No-op. |
| OnDeviceStateChanged | For endpoints eligible for auto routing and matching current ID, marks detached/stops on inactive state; on active state can reinit/restart. |
| OnDefaultDeviceChanged | Filters flow and auto-route policy; explicitly ignores role parameter, excludes exclusive-mode rerouting; can stop/reinit/restart using default resolution. |

Auto routing is allowed only when endpoint ID was NULL/default and `wasapi.noAutoStreamRouting` is false. Explicit endpoint IDs do not get the same built-in reconnect policy. AudioHole generally wants explicit logical bindings, configured fallbacks, source sharing and per-direction status: built-in default-following is a different policy. It must not silently substitute a default endpoint for a missing saved device. [D3, W2]

On reroute, the code preserves previous running state, skips starting/uninitialized cases, attempts endpoint reinit, reconfigures converters with new backend format, updates IDs/names/periods, emits rerouted notification, and restarts if policy/state permits. Return handling is not a complete structured supervisor state machine; some callback calls to reroute/start ignore result. A single duplex object's state can tie both endpoints' restart readiness together. Application notification enum availability does not establish comprehensive Windows interruption reporting. [D3]

## Reinitialization lifecycle risk

**Verified source, runtime impact unmeasured.** `ma_device_reinit__wasapi` releases render/capture service pointers, but the previous `pAudioClientCapture`/`Playback` pointers are set NULL without calling Release; the Release/command calls are commented out. Upstream explicitly describes a known leak to avoid a deadlock when releasing from the routing notification callback. This is stronger evidence than a speculative leak allegation, but no memory-growth rate or failure count was measured. The context command infrastructure does not make this reinit path fully deferred. [D3, W1]

[Microsoft's notification contract](https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nn-mmdeviceapi-immnotificationclient) requires nonblocking callbacks and avoidance of final MMDevice reference release inside them. The source performs stop/start, waits/locks and reinitialization in notification handling; this is a reason to test and prefer deferred lifecycle work, not a claim to have reproduced a specific deadlock.

For AudioHole, queue a bounded device event and process it on the supervisor/control thread. Own lifetime with clear COM-thread constraints; stop, quiesce, release, rediscover/rebind and reopen under explicit policy. Preserve logical configuration across failure, distinguish missing/busy/access-denied/unsupported/invalidated, and cancel recovery on explicit Stop. Rate/history state must reset or re-prime deliberately on stream generation changes. This is an **experimental application design**, not a production change.

## Errors and operational evidence

Initialization paths distinguish access denied, busy and format/share-mode errors, often mapping HRESULT to `ma_result`. Some error logs include raw HRESULT; other paths log generic text. Many HRESULTs collapse into general result codes, so callback return alone cannot supply AudioHole's required structured raw Windows diagnostics. Store requested/effective settings, API stage, HRESULT, endpoint generation and timing in a native probe or carefully reviewed extension. Do not scrape human logs as a durable schema. [W2, W4]

No endpoint lifecycle should be called from AudioHole's real-time callback. A device successfully initialized/started is evidence of those stages only, not an audible route or long-run stability. No persistence, portable matching, transactional profile switching, configurable backoff, complete stop cancellation, or capability-history storage is supplied by miniaudio's device object.

**Disposition:** Adopt opaque native-ID treatment and negotiated-state separation; adapt I/O lifecycle mechanisms; reject the full library device registry/recovery abstraction as a replacement for AudioHole's identity/supervisor requirements. Test repeated removal, driver state changes, default-role changes, explicit-endpoint reconnection and resource cleanup before any backend acceptance.
