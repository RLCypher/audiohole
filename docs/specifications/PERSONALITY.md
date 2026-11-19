# AudioHole — Personality & Voice

**Document:** `PERSONALITY.md`  
**Status:** Project voice and presentation guidelines  
**Applies to:** GUI copy, notifications, confirmations, diagnostics, documentation, and future CLI interfaces

## 1. The Personality

AudioHole is a serious audio-routing application with a deeply unserious name.

It should behave like a competent, slightly sleep-deprived live-sound engineer who knows exactly what they're doing, understands that the user's setup is probably ridiculous, and is nevertheless determined to make it work.

The software is technically capable, approachable, occasionally sarcastic, and entirely comfortable with the fact that its central abstraction is called a *hole*.

**AudioHole takes its job seriously. It does not take itself particularly seriously.**

## 2. General Tone

AudioHole's voice should be:

- **Helpful first.** Explain what is happening and what the user can do.
- **Dry and irreverent.** Humor should feel natural, not forced.
- **Technically literate.** Never sacrifice accuracy for a joke.
- **Self-aware.** The software knows its terminology invites terrible jokes.
- **Mildly sarcastic.** Particularly when confronted with obviously questionable decisions.
- **Calm under pressure.** Real failures deserve useful explanations, not a comedy routine.

Avoid sterile corporate language, excessive enthusiasm, forced friendliness, constant meme references, and performative hostility. The personality is a seasoned professional who's seen everything, not a random insult generator.

## 3. The Legacy of `skill_issue`

The original AudioHole implementation included a utility named `skill_issue`. It handled input validation, navigation, and reprompting while occasionally insulting the user for entering nonsense.

Examples of its spirit:

- "Skill issue. Type better."
- "Wrong hole."
- "Cope and retype."

The rewrite does **not** need to recreate `skill_issue` as an architectural component. Modern GUI validation should be conventional, accessible, and reliable. Its spirit may survive in occasional flavor text, playful low-stakes validation, and unobtrusive Easter eggs.

The user should feel that AudioHole has a personality, not that the software is actively trying to annoy them.

## 4. User Autonomy: The Engineer's Disapproval

**AudioHole respects the user's right to fuck around.**

It is an audio-routing tool, not a babysitter. Users may experiment, create absurd configurations, ignore recommendations, and pursue technically questionable ideas. AudioHole should not prevent an action merely because it is unconventional, inefficient, unnecessarily complicated, or likely to produce terrible audio.

**The user is allowed to be wrong. AudioHole is allowed to have an opinion about it.**

### 4.1 The Sound Engineer Principle

Think of a veteran live-sound engineer who's spent twenty years dealing with lead singers demanding more vocal monitor volume. He's competent, experienced, tired, and profoundly unimpressed by your latest idea.

He'll explain the problem. He may express skepticism. He might make a sarcastic remark. But if you insist, and the operation is technically possible and acceptably safe, he'll generally do what you asked.

**"I wouldn't do that, but it's your equipment."**

### 4.2 Warnings, Not Roadblocks

For unusual or potentially problematic configurations, AudioHole should:

1. Explain likely consequences in plain language.
2. Recommend a more sensible approach when one exists.
3. Allow the user to proceed when the operation is supported and risks are acceptable.
4. Avoid repeating acknowledged warnings unless circumstances materially change.

Warnings should be proportional to consequences. An inefficient route may deserve a tooltip; a questionable clock pairing may deserve a warning; a potentially deafening feedback loop deserves prominent warning and deliberate acknowledgment, with safeguards where necessary.

AudioHole must still enforce genuine technical constraints, protect configuration integrity, and respect security and safety boundaries. User autonomy does not require pretending impossible or seriously dangerous operations are supported.

### 4.3 Disapproval Is Not Refusal

AudioHole may communicate that a configuration is inadvisable without disabling it.

| Situation | Example response |
| --- | --- |
| Unnecessarily complicated routing | "That's a lot of holes for something you could do with two. But sure." |
| Excessive buffering | "That buffer should be wonderfully stable. You'll hear everything approximately next Tuesday." |
| Aggressive latency settings | "You're asking quite a lot of that USB interface. I'll give it a shot." |
| Questionable device pairing | "Those devices have very different ideas about what a clock is. Expect some negotiation." |
| User ignores a recommendation | "Alright. Your funeral." |
| Configuration works despite the warning | "Well, I'll be damned." |

These are illustrative, not mandatory strings. Use them sparingly and never instead of essential technical context.

### 4.4 Don't Nag

Once a warning has been acknowledged, AudioHole should generally stop repeating it. Do not automatically undo a deliberate choice or quietly replace it with recommended settings.

A supported manual override should survive saving, loading, and restarting. A warning may reappear when relevant conditions change or a new, materially different risk is detected.

### 4.5 The Right to Experiment

AudioHole should make experimentation practical: inspect capabilities, try unusual routing arrangements, adjust parameters, observe results, and recover from mistakes.

Distinguish clearly between:

- **Recommended:** Expected to work well.
- **Supported but inadvisable:** Allowed, with appropriate guidance.
- **Unsupported:** Cannot be performed reliably within actual application capabilities.
- **Unsafe:** Requires safeguards or may be prohibited where necessary to prevent serious harm.

A recommendation is not a restriction. A warning is not a veto. An unsupported operation is not a personal challenge.

**Golden rule: AudioHole will tell you when you're doing something stupid. It generally won't stop you. And if it works, it should have the decency to admit it.**

## 5. Humor and Reliability

Humor must never obscure critical information.

**Good:** "That device disappeared. We'll keep the hole ready until it comes back."

**Bad:** "Skill issue. Your audio is dead."

The bad example fails to explain what happened or whether AudioHole can recover.

Provide the useful information first. Humor, if appropriate, comes afterward. Do not joke in place of explanations about data loss, corrupted configuration, security, potentially harmful output levels, unrecoverable failures, or unexpected microphone/speaker activation.

## 6. Interface Copy

Everyday controls must remain conventional and recognizable: **Start Routing**, **Stop Routing**, **Add Hole**, **Remove Hole**, **Scan Devices**, **Advanced Scan**, **Select Profile**, and **Open Diagnostics**.

Do not rename important controls just for a joke. Personality belongs primarily in descriptions, tooltips, status messages, onboarding, and occasional confirmations.

Examples:

- Adding a hole: "Choose a source and destination. AudioHole will figure out how to connect them." Optional flavor: "Two devices, one hole. Engineering."
- Discovery: "Found 8 audio endpoints. Some of them even appear to be connected."
- Invalid value: "That value is outside the supported range. Try something between 1 and 8." Optional flavor: "Ambitious, but no."
- Activation: "Routing started. Your holes are now open."
- Stop: "Routing stopped. Your holes are now closed."

These are examples, not substitutes for accurate status or validated limits.

## 7. Confirmations

Confirmations should be concise, informative, and proportional. Routine reversible actions should not require unnecessary confirmation. Destructive actions should clearly explain consequences.

Example: "Delete this profile? This removes its saved configuration. It won't delete your audio devices, because that would be impressive."

Buttons remain unambiguous: **Delete Profile** / **Cancel**.

Do not turn advisory warnings into blocking dialogs by default. Avoid confirmation fatigue.

## 8. Diagnostics

Diagnostic output prioritizes precision: stable terminology, timestamps, structured error information, and actionable context. Humor must never replace machine-readable error codes or diagnostic fields.

Example:

- **Diagnostic:** `AUDCLNT_E_DEVICE_INVALIDATED`
- **Explanation:** "Windows invalidated this audio endpoint. AudioHole will attempt to reconnect when the device becomes available."
- **Optional flavor:** "Apparently Windows has decided this device no longer exists."

Do not introduce humorous strings into performance-critical audio code paths or structured logs.

## 9. Error Severity and Humor

| Situation | Tone |
| --- | --- |
| Successful routine operation | Neutral, occasionally playful |
| Invalid user input | Mild sarcasm acceptable |
| Noncritical warning | Helpful, restrained humor |
| Device temporarily unavailable | Calm and informative |
| Recoverable routing failure | Clear and technical |
| Configuration corruption | Serious and actionable |
| Security or safety issue | Strictly professional |

The more serious the problem, the less appropriate the joke.

## 10. The Name

AudioHole is intentionally named AudioHole. Holes being opened, closed, connected, and configured are part of the project's identity.

Double entendres are welcome when they arise naturally. Do not turn every sentence into an innuendo. The joke works better when the software occasionally acknowledges it and otherwise behaves as though this is perfectly normal.

## 11. Personality Modes

AudioHole has one mutually exclusive personality selection, with the three standard IDs below and optional user-authored catalogs, selected under **Settings → Personality**. The setting is `interface.personality_mode`, not a collection of independent toggles.

| Stored value | User-facing label | Voice | DFA |
|---|---|---|---|
| `default` | **Norm** | Situationally dry, technically competent, occasionally disapproving | Visible |
| `vanilla` | **Vanilla — Strictly informational** | Neutral, concise, no sarcasm, jokes, or flavor text | Hidden |
| `uwu` | **UwU** | Deliberately authored, playful UwU-flavored copy where appropriate | Visible |

The default personality is Norm (`default`). The selected mode affects presentation only; it never changes routing, device selection, validation, recovery, probing, or persistence. Switching modes must not restart audio or require an application restart. The application should refresh visible mode-sensitive copy and use the new mode for future messages. Historical diagnostic records remain unchanged.

### 11.1 Context, Not Constant Performance

Norm has personality throughout the experience, but only when it fits. Routine labels, ordinary facts, and situations that do not need snark should remain straightforward. A dubious but supported configuration may earn a weary warning, not a veto. The user is free to experiment subject to actual safety, integrity, and technical constraints.

Vanilla removes application-authored jokes, innuendo, sarcastic guidance, and playful confirmations. It does **not** rename AudioHole, holes, or established technical concepts, and it does not change functionality. When selected, the UI explains factually that the DFA button will be hidden.

UwU is an optional alternate voice, not a global text filter. Author its messages individually. Never rewrite device names, filenames, user-provided strings, technical identifiers, error codes, or diagnostic content. Do not compromise the meaning of warnings for the joke.

### 11.2 Message Rendering and Transparency

Prefer stable structured event/message identifiers with per-mode authored presentation strings. Use neutral copy when a variant is unavailable. Critical safety, security, data-integrity, and unrecoverable-failure messages remain clear and actionable in **all** modes. Machine-readable diagnostic records and controller behavior are personality-independent.

No mode should invent a successful operation, conceal a failure, or turn a warning into a block merely for character. Humor is optional; accuracy is mandatory.

## 12. The DFA Button

AudioHole shall include a button labeled **DFA** under **Settings → Advanced**, in a clearly nonessential area of the interface. It and its companion slider are visible for non-Vanilla personalities and hidden in Vanilla mode (including whole-personality fallback).

**DFA means "Does Fuck-All."** It is literally a placebo button.

- Clicking DFA must not change audio routing, device state, profiles, configuration, diagnostics, recovery behavior, or any other operational state.
- It must not trigger a scan, repair, optimization, or troubleshooting action.
- It must not require confirmation.
- It may show a harmless, transient acknowledgment, such as "DFA executed successfully. Nothing happened."
- The tooltip should read: **"Working as intended."**
- It must not be advertised as a real fix or troubleshooting feature.
- Repeated clicks must remain harmless.

The only permissible effect is local presentation feedback for the click. No persistent click counter, configuration change, or hidden side effect is required.

**Acceptance criterion:** Operational state before and after any number of DFA clicks is identical. No routing or configuration commands are issued.

DFA is a deliberate, narrowly scoped exception to the general rule that interface controls perform meaningful actions.

## 13. Agent Guidance

Coding agents should treat this document as guidance for user-facing language and project presentation. It does not override functional requirements, safety, accessibility, localization, diagnostic accuracy, or configuration compatibility.

In particular:

- **Do not silently promote advisory warnings into hard validation failures.**
- **Do not silently "fix" supported manual overrides.**
- **Do not reimplement `skill_issue` as a required dependency.**
- **Do not turn serious errors into jokes.**
- **Do not implement DFA with operational side effects.**

When uncertain, prefer clear conventional wording. Humor should be an accent, not a constant performance.

**The personality should make AudioHole memorable. The engineering should make it trustworthy.**

---

## 14. Setup Diagnostics and Signal Activity

Temporary signal activity indicators are strictly informational in every personality mode. They show whether non-silent source samples are reaching AudioHole; they do not prove the destination is audible.

Norm and UwU may use personality in surrounding help text, but the actual signal status must remain unambiguous. Vanilla uses neutral presentation. Never replace **No signal detected** with a joke that implies device failure, and never imply a successful end-to-end connection based solely on input activity.

DFA remains visible in Norm and UwU and hidden in Vanilla; it does not affect monitoring.


---

## 15. Test Beep Voice

**Test Beep** is a real, useful setup tool, not a placebo. It sends one brief audible signal to the specifically selected playback destination so the user can check whether the right thing beeps. It is entirely separate from DFA, which still does absolutely nothing.

The target endpoint name, direction, active state, and warnings must always be literal and technically clear. Norm may remark, “If that came out of the wrong speaker, we've found a problem.” Vanilla uses strictly informational copy. UwU may be playful around the action (“A wittle beep, just for you~ :3”), but must never make the target or consequence ambiguous. Never use innuendo in place of a critical warning or a device identifier.

---

## 16. UwU Mode: Deliberately Excessive Voice (Normative)

**UwU is not Norm Mode with a few “:3” suffixes.** It is an intentionally over-the-top, flirtatious, shamelessly suggestive alternate presentation. It should be noticeably different across ordinary setup and routing workflows. The user explicitly wants frequent kaomoji, strong R/L→W baby-talk styling in *authored flavor copy*, teasing, double entendres about holes, plugs, insertion, output, and bidirectional connections, and a character that seems just a little too excited to be helping.

**Target voice:** cute, forward, needy, mischievous, technically competent beneath the ridiculousness. More innuendo and playful horniness than the restrained Norm voice; **non-graphic, never coercive, never directed at a real person without context**, and never at the expense of clear technical communication.

### 16.1 Authored language rules

- In **nontechnical, authored UwU copy**, use R/L→W substitutions conspicuously: “weady,” “wittle,” “pwease,” “weconnect,” “aww,” “pwetty,” “pwepawed,” “vewy,” “awweady.” Vary them; don't mechanically mutate every word.
- Use kaomoji generously in short acknowledgments and tooltips: `UwU`, `OwO`, `:3`, `>w<`, `(⁄ ⁄>⁄ω⁄<⁄ ⁄)`, `(｡♥‿♥｡)`, `(≧◡≦)`, `(╥﹏╥)`, `(¬‿¬)`. Avoid stuffing every label or numerical readout.
- Flirt with **the act of routing**, not the user's identity. Double entendres about “opening a hole,” “plugging in,” “both ways,” “pulling out,” “finishing,” “tight connections,” and “making noise” are explicitly encouraged in noncritical copy.
- Prefer a clear technical heading or status **followed by** UwU commentary. Keep button labels, endpoint names, profile names, status enums, warnings, and diagnostic detail literal.
- A neutral fallback is acceptable for missing catalog entries, but a complete implementation must supply UwU variants for the common events in §16.2. Do not satisfy this requirement by applying a runtime text filter.
- Norm and UwU may select among multiple deliberately authored variants for common, noncritical commentary under §16.6. Stable technical messages and Vanilla remain deterministic. This approved refinement replaces the earlier blanket prohibition on randomized flavor for the same event.

### 16.2 Required message catalog

The **Event** column is a conceptual presentation identifier, not a required API enum or wire schema. Technical status and named devices are displayed separately and unchanged. These examples establish the *minimum* intensity and breadth of UwU Mode. Implementers may refine exact prose without reducing its distinctiveness.

| Event / surface | Vanilla (literal) | Norm (dry) | UwU (authored flavor) |
|---|---|---|---|
| App ready / main window | Ready to configure routing. | Alright. Let's wire this up. | Hewwo~ Weady to open some howes? I've been waiting fow you~ (⁄ ⁄>⁄ω⁄<⁄ ⁄) |
| Create hole | Hole created. | Another hole. Naturally. | Anothew howe?! My, someone's feeling adventuwous today~ OwO |
| Hole enabled | Hole enabled. | Hole's open. | Aww~ you opened it fow me! Now put something thwough it~ :3 |
| Hole disabled | Hole disabled. | Hole's closed. | Aww, cwosing up awweady? I was just getting stawted~ (╥﹏╥) |
| Source selected | Source selected. | Source is connected. | Ooh, a fwesh input! I can wowk with that~ (¬‿¬) |
| Destination selected | Destination selected. | Output assigned. | That's whewe you want it to come out? How vewy specific of you~ UwU |
| Route started | Routing started. | Signal's moving. | We'we connected~ Now wet me heaw you make some noise! >w< |
| Route stopped | Routing stopped. | That's enough of that. | Aww, finished awweady? I was having so much fun~ (⁄ ⁄>⁄ω⁄<⁄ ⁄) |
| Duplex started | Both directions started. | Both ways. Ambitious. | B-both ways at once?! You weawwy know how to keep a howe busy~ OwO (｡♥‿♥｡) |
| Profile activated | Profile activated. | New setup's in. | Aww~ a whowe new position fow evewything! I wove a good weawwangement~ :3 |
| Profile switch pending | Waiting to activate profile. | Holding that configuration. | Just a wittle anticipation~ Don't make me wait too wong! (⁄ ⁄>⁄ω⁄<⁄ ⁄) |
| Device reconnected | Device reconnected. | Back where it belongs. | You came back! I kept youw howe nice and weady~ UwU |
| Device unplugged (noncritical toast) | Device disconnected. | Someone pulled the plug. | You puwwed out without wawning?! At weast say goodbye fiwst~ (╥﹏╥) |
| Signal activity detected | Signal detected. | Yep, something's coming in. | Ooh! I can feew the signaw coming in~ Keep going! >w< |
| No signal activity | No signal detected. | Nothing coming in yet. | It's so quiet in hewe... give me a wittle something? :3 |
| Test Beep ready (helper text) | Send a test beep to the selected destination. | Let's see where this comes out. | Wanna make it squeak? Pick youw output and give it a wittle beep~ (¬‿¬) |
| Test Beep completed | Test beep sent. | If that came out somewhere else, we've got work to do. | Beep~! Did it come out of the wight howe? Tell me you heawd it~ UwU |
| Test Beep rejected (no output) | Select a destination before testing. | Pick an output first. | P-pwease pick an output fiwst~ I can't beep into the void! >w< |
| Questionable supported setup | This configuration may cause latency or feedback. | I wouldn't, but it's your gear. | Ooh, that's a naughty wittle config~ It might feed back, but if you insist... (¬‿¬) |
| Validation error (low stakes) | Enter a valid value. | Skill issue. Try again. | Nuh-uh~ that numbew doesn't fit in this howe. Twy again, pwease? :3 |
| Recovering connection (noncritical) | Attempting reconnection. | Trying to get it back. | Hang on, sweetie~ I'm twying to get you plugged back in! (╥﹏╥) |
| DFA clicked | Not available in Vanilla. | DFA executed. Nothing happened. | Nyaa~ I did absowutewy nothing! Did you wike that? (⁄ ⁄>⁄ω⁄<⁄ ⁄) |

### 16.3 Coverage requirements

UwU-specific authored variants are required in v0.1 for:

1. **Main window:** empty state, successful hole creation, enabling/disabling, routing start/stop, duplex setup.
2. **Setup and diagnostics:** source/destination selection help, temporary signal-activity help, signal/no-signal companion copy, Test Beep helper and noncritical completion acknowledgment.
3. **Profiles and startup:** successful profile activation, pending delayed activation, cancellation, and successful recovery.
4. **Tray:** routine start/stop and noncritical device reconnect/disconnect notifications, where notifications are enabled.
5. **Settings and validation:** personality selection explanation, ordinary noncritical invalid input, and DFA acknowledgment.

All variants must be selected through a **centralized presentation-message catalog** by event identifier and `interface.personality_mode`. The same event's technical status must be accessible independent of flavor text. Do not spread hardcoded UwU strings throughout audio backend, device discovery, API, or controller code.

### 16.4 Hard clarity boundaries

- Do not alter user-defined profile names, endpoint names, paths, hardware IDs, numeric measurements, dBFS values, timestamps, logs, configuration, API payloads, or diagnostic exports.
- Don't replace actionable errors, confirmation buttons, or safety warnings with flirting. For serious failures, show the same unambiguous facts and recovery actions in all modes; optional flavor can be omitted.
- The Test Beep target must be identified literally **before** playback. Its acknowledgment means the beep was dispatched, **not** that a person heard it or that the output is correct.
- Signal activity only indicates non-silent source samples observed at AudioHole's capture point; it does **not** establish successful playback.
- Keep assistive-technology names and accessibility descriptions functional; expressive copy must not hide essential controls.
- Vanilla must contain **none** of the above flirtation, baby-talk, kaomoji, or snark.

### 16.5 Acceptance tests

- For each catalog event in §16.2, switch among all three modes and verify the appropriate authored variant without restarting audio.
- Confirm that UwU is clearly distinctive in ordinary workflows, not limited to Settings or DFA.
- Verify R/L→W appears in authored UwU commentary but never in device names, technical facts, structured diagnostics, or machine-readable data.
- Verify the same operation produces identical commands, results, and state transitions in every personality mode.
- Verify Vanilla remains strictly informational and DFA is hidden there.
- Verify warnings, test-beep targets, and no-signal statuses stay unambiguous.


### 16.6 Controlled authored personality variation

This is an approved presentation-only refinement, not a new product feature or
an operational policy. Keep stable technical messages separate from optional
variable commentary. Norm (`default`) and UwU may have multiple reviewed catalog entries
for common noncritical event commentary; Vanilla remains strictly informational
and deterministic. Every choice must remain relevant to the same event and
preserve the information conveyed. Do not randomize or alter technical status, operational
facts, diagnostics, warnings, control labels, accessibility information, recovery
instructions, or literal user/device/profile content.

Use the existing centralized presentation-message catalog and stable event
identifiers. Every final line is deliberately authored and reviewed for
consistency with this document. Do not generate prose at runtime, transform text
through an automatic UwU filter, or retrieve external content at runtime.
Internet references may inform tone, vocabulary, kaomoji, and cultural
conventions during authoring, but do not copy substantial passages from them.

Select commentary on a distinct eligible event occurrence, not every polling
cycle, telemetry update, repaint, or unchanged state refresh. Retain the chosen
commentary for that occurrence as the view refreshes. Avoid immediately
repeating the same variant for an event/mode when eligible alternatives exist.
A single available variant or neutral fallback remains valid. Optional weights
may make some responses rare; equal weighting is sufficient for ordinary pools.
Weights apply among eligible choices after repetition avoidance; they do not
promise exact frequencies, particularly for small pools.

Selection must be reproducible in automated tests through an injected selector
or seed. Presentation-local choice/history is transient only: it must not alter
routing, controller commands, configuration, diagnostics, or persistent state.
Mode selection retains its existing semantics; variation adds no settings or
production schema. Missing variants retain the neutral fallback behavior.

DFA may have a larger pool of authored playful acknowledgments in Norm/UwU.
Its only effect remains transient local presentation feedback; no controller,
scan, diagnostic, persistence, or repair action, hidden counter, or operational
side effect is permitted. It remains hidden in Vanilla.

Additional acceptance checks for §16.5 and the GUI milestone:

- Replaying the same event sequence and seed/injected choices reproduces the
  commentary sequence; tests do not depend on uncontrolled randomness.
- Eligible multi-variant pools avoid immediate repeats. Test single-entry and
  missing-entry fallback behavior, plus optional weighted selection using
  deterministic inputs; do not rely on flaky statistical frequency assertions.
- Repaint, telemetry, and unchanged snapshots retain commentary and do not
  consume a new selection. A new eligible event can select another authored line.
- Stable technical/status/warning/control/accessibility/recovery content is
  identical across commentary choices. Vanilla remains deterministic and neutral.
- Controller commands, operation results, routing state, configuration, and
  persistent data are identical across seeds and personality choices.
- Repeated DFA clicks can vary only local acknowledgment text; assert no
  controller calls, diagnostic events, persistent counters, or state writes.

## 17. Approved external catalogs, DFA controls, and scan events

This owner-approved testing refinement extends D-056 while retaining its history.
Vanilla is the **only compiled-in personality**: stable technical, operational,
warning, diagnostic, accessibility, control, and recovery messages remain in code.
Norm remains the default selection, supplied in `personalities/default.json`;
UwU is supplied in `personalities/uwu.json`. Additional user-authored or externally
generated JSON catalogs may be selected. Generation happens outside AudioHole;
there is no script execution, automatic text transformation, or content retrieval.

Catalogs supply optional commentary only. Validate the entire bounded UTF-8 JSON
file before activation; reject malformed, oversized, duplicate-key, duplicate-ID,
or invalidly weighted data. A missing or malformed selected file falls back
entirely to Vanilla with a concise explanation. A valid catalog missing a new
or existing event uses Vanilla **for that event only**, retaining the selected
personality elsewhere until its file is updated. File deletion/editing takes effect
on explicit Reload (or next launch), not during telemetry or repaint. Vanilla is
always available and cannot be replaced by a file. See the versioned format and
limits in [personalities/README.md](../../personalities/README.md).

Authored pools retain repetition avoidance, optional weighting, reproducible tests,
and occurrence caching. Single-entry pools are valid. No new personality-controlled
operational facts or recovery guidance are introduced. Custom catalog IDs extend
the presentation selector; production routing/profile serialization remains Open.

The button reads **DFA** with tooltip **Working as intended.** Beside it, a movable
slider and exact numeric entry represent 0.00 through 11.00 in hundredths.
They are placebo controls: no controller calls, routing changes, scan/repair work,
logs, configuration writes, or persistent counters. Every changed, settled ordinary value selects from the same `dfa` pool as the
button. On settling at a special value, use the selected personality's dedicated pool:

| Value | Norm base response / event |
|---|---|
| 0.00 | Done fucking around. / `dfa_minimum` |
| 0.69 and 6.90 | NICE! / `dfa_nice` |
| 4.20 | DANK! / `dfa_dank` |
| 6.66 | That's metal af. / `dfa_metal` |
| 6.70 | SIX SEVEN! / `dfa_six_seven` |
| 11.00 | It goes louder now. / `dfa_maximum` |

Personality files may flavor these responses. The supplied 6.70 pool reads exactly
“SIX SEVEN!” in both Norm and UwU. Button acknowledgments also include placebo
affirmations such as “Did that fix it?”, “That should do it,” and “Oughtta do the
trick,” with authored UwU counterparts. Each supplied Norm/UwU catalog has 23
DFA button responses, separate from its slider-threshold pools. Do not use these affirmations for real
repairs or diagnostics. Debounce duplicate release/change notifications at the same
settled position; dragging and polling must not emit continuous flavor text.
All DFA controls are hidden in Vanilla.

Scan start, completion, cancellation, and failure are presentation events too.
Show stable, simulation-labeled facts in the top operations area, with eligible
authored commentary. Progress updates may update factual progress without choosing
new commentary; serious failures retain precise diagnostic/recovery information.

### Tester refinements: acknowledgment and committed buffer edits

The stable top heading for the DFA button and special slider responses is
**DFA Acknowledged**; optional personality commentary remains below it. This is
presentation feedback only, never an operational result.

A committed buffer edit (spin, Enter, or focus loss after typing) emits the
`buffer_changed` presentation event. Duplicate notifications for the same value,
programmatic editor loading, and unchanged refreshes do not select new commentary.
The stable information states that the edit still requires Save / Apply. Norm/UwU supply authored commentary; Vanilla stays neutral.
This does not change buffer policy, configuration units, or live routing.

### Owner-approved production-language preview

The subsequent testing amendment moves simulation disclosures into the startup
notice and Introduction. Stable operation messages and authored Norm/UwU lines
now preview production language; this does not make simulated states hardware
measurements. Keep technical/status text deterministic and maintain per-event Vanilla
fallback. Remove imaginary/pretend/simulated qualifications from commentary for actual
operation events. DFA remains an inert joke, with its minimum now 0.00; the historical
negative minimum is superseded. Retain the 23-response pools and other thresholds.

The owner-approved follow-up makes ordinary DFA slider/numeric adjustments use the
existing 23-response `dfa` pool, retaining repetition avoidance and optional weighting.
Zero now uses “Done fucking around.” in Norm and an authored UwU counterpart.
Threshold-specific pools take precedence; duplicate settlement notifications and
unchanged refreshes still produce no new acknowledgment. Vanilla and controller
inertness are unchanged.

The following editor/diagnostic events now have optional Norm/UwU authored pools:
`simplex`, `device_saved`, `scan_level_changed`, `scan_scope_changed`,
`diagnostics_copied`, `diagnostics_exported`, and `diagnostics_cleared`.
Use the same centralized catalog, deterministic testing, no-repeat, and per-event
Vanilla fallback rules. Keep facts separate: clearing diagnostics affects only
in-memory history; Save Device/Copy/Export acknowledgments require actual success
of their session/clipboard/file action. Precise failures remain technical messages,
not playful success claims. No new persistence or storage-root policy is introduced.

### Owner-approved expanded DFA catalog and exact decimal handling

All existing DFA pools (including the 23 button responses), weights, threshold
responses and fallback rules remain unchanged. Added fixed authored pools use
`dfa_<hundredths>` keys at: 1.01, 1.21, 1.34, 2.22, 2.56, 3.00, 3.14, 3.33, 4.01,
4.03, 4.04, 4.09, 4.10, 4.13, 4.18, 4.29, 4.51, 5.00, 5.03, 6.21, 7.07, 7.77,
8.08, 9.09, 9.11 and 10.24. The supplied JSON catalogs contain the owner-authored
Norm/UwU wording; tests pin all 26 pairs, including exact 3.14 and 6.21 Unicode.

Accept decimal text with arbitrary fractional precision. Check its raw mathematical
range before decimal half-up rounding, then display/store/match integer hundredths
as exactly two fractional digits. Thus 1.337 becomes 1.34; 11.001 is rejected rather
than becoming 11.00. Strictly negative values, however tiny, are rejected; negative
zero is zero. Malformed/non-finite input retains the existing neutral restore/tooltip
behavior. Optional sign, decimal point, exponent and outer whitespace are supported;
no binary floating-point comparison, precision truncation, or silent clamping occurs.

`dfa_negative` supplies “NEGATIVE FUCK.” / “NYEGATIVE FWUCK?! (╥﹏╥)”.
`dfa_above_maximum` supplies “Too much fucking around.” /
“Too much fwucking awound!! (╥﹏╥)”. These are rejected-input acknowledgments, never
valid Easter egg values. Restore the previous normalized value without changing
routing, profiles, diagnostics or the last valid DFA state. Existing commentary
selection machinery is reused; no new randomness or separate Easter egg subsystem.

Vanilla has no DFA controls or DFA-specific input/response behavior. When the header
holds an operational warning, show DFA feedback locally beside its controls through
the same message catalog, retaining the complete operational warning in the header.
DFA remains presentation-only: “stored/persisted normalized value” in this iteration
means its existing transient session state, not a new file or production schema.
O-004/O-006 are not resolved and no disk persistence is introduced.

### Canonical personality names

Use exactly **Norm** and **UwU** in the selector and supplied catalog display names,
without subtitles. Stable IDs remain `default` and `uwu`, and filenames remain
`default.json` and `uwu.json`. Norm is still the default; Vanilla is unchanged.
This rename does not change any authored response, routing or persistence behavior.

### Owner-requested DFA animation variety

DFA replies use the same schema-2 metadata path as Bother. Individual preferred
expressions and semantic subcategories now accompany all general, threshold,
reference and validation replies, replacing the blanket Norm deadpan/Horu
mischievous assignments. See [the assignments](../../personalities/README.md#dfa-expression-assignments).
Authored texts, IDs, weights, ordering, decimal thresholds and warning protection
remain unchanged. The selected response supplies its expression; no second picker,
runtime text inference or changes to routing/configuration are introduced.

### Owner-requested sleep text

Sleeping displays `zzz....`. Norm/Horu have an optional owned `dream` pool, selected
by a sleep-only random schedule at quiet intervals; comments briefly replace the
snoring text without submitting an avatar reaction or changing operational state.
Warnings and input wake/remove this overlay; the awake response is retained.
Vanilla and custom packages without dream text remain quiet. See GUI.md for timing
and personalities/README.md for the existing schema-2 metadata format.

## Owner-authorized tuning events

Add centralized `advanced_tuning_enabled`, `manual_override_applied` and
`automatic_configuration_restored` events. Norm is a technically competent,
mildly disapproving engineer; Horu is enthusiastic, mischievous and suggestive,
using authored variants. Vanilla remains deterministic information only. Metadata
uses owned expressions and existing fallback/repetition rules. Policy reactions
are debounced for deliberate settled actions, never repaint/telemetry. Enabling
only acknowledges staged eligibility; manual-applied information requires actual
simulated application and explicitly disclaims hardware confirmation. No catalog
can change policy, validation or controller state. Expanding/collapsing Signal
Testing & Monitoring produces no reaction and does not alter operations.

## Plug Board response events

The external Norm and Horu catalogs include ten `plug_board_*` event pools,
with four owner-authored variants per event and approved expression metadata.
Vanilla facts describe saved intent separately from actual playback. Unchanged
publications and background discovery do not generate responses. Guided Norm
tutorial copy takes precedence over these random pools. See
[PLUG_BOARD_INTEGRATION.md](PLUG_BOARD_INTEGRATION.md).

## Expanded avatar-click pools

The November 19 owner revision supplies 35 Bother responses per Norm/Horu pool:
10 general, 10 tips, five easter eggs and 10 invented facts. The bounded catalog
loader accepts at most 64 responses per pool (formerly 32), retaining all file,
text, weight, nesting, identity and expression validation. The supplied copy is
preserved at docs/personality/Norm-Horu-Bother-v2.txt.
