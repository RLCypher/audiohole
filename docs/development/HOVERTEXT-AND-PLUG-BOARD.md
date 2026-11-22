# Revised hovertext, Plug Board rows and license refresh

The owner-supplied [Revised v2 catalog](AudioHole-Hovertext-Catalog-Revised-v2.txt)
is the wording source for this batch. Its 169 entries were compared against
5df49c2: 136 changed fixed-text entries were applied (138 literal occurrences),
two dynamic families received the requested environment-neutral fallback wording,
and unchanged text/templates were retained. This includes control-specific
overrides, shared defaults, context-menu help and taskbar Jump List descriptions.
The catalog's existing source coordinates refer to that baseline revision.

Runtime endpoint identities, profile/hole names, technical error details, scan
status and full actual playback-device names remain dynamic. Placeholder braces
and explanatory catalog notes are never displayed as literal tooltips. The
fallback scan states now use Guided scan in progress / Scan in progress; an
unavailable engine's tray description omits the old Test Mode prefix.

## Plug Board interaction

Activity headers (Active, Recently Active and Inactive) and individual app rows
retain their ordinary background and text colors when selected. Selection still
identifies the keyboard/context-menu target and is exposed to accessibility;
app assignments, routing policy, dragging and group expansion are unchanged.

The app-list container begins without a tooltip. Headers and empty space have
none; app rows alone show the revised drag hint, or full fresh playback-device
names over their status line. Leaving the list clears the hint. Publications
re-evaluate a stationary pointer so an app moving between groups cannot leave a
tooltip on a header. Accessible app help remains available without mouse hover.

The catalog explicitly supplies separate help for actual settings choices
(Routing Board, associated Profile, recent-activity interval and socket playback
device). Those controls retain that v2 help. The app-only rule applies to the
app-list hover surface and its collapsible activity headers.

## License

AudioHole License.rtf is replaced byte for byte with the owner's latest upload:
336,513 bytes; SHA-256
506e09b4fb6b0acf20c5f2478ea5cc4131630569ff6a979fb7eacf756cd087c9.
The same canonical file is embedded for first-run/About viewing, consumed by
Inno Setup and installed beside AudioHole.exe. Existing acceptance receipts,
GPLv3 text, original-asset terms and other supplied documents remain in place.

## Verification

All 306 portable/model tests, distribution-tool safeguards and the Windows x64
GUI cross-build passed locally. The revised fixed strings were audited against
the catalog, and the copied RTF's bytes/hash were checked. Native execution is
performed by Windows CI after publication; cross-compilation cannot run it.

Native host smoke now samples selected/unselected row backgrounds in both app
themes, checks accessibility selection, and dispatches real hover events over
all three activity headings, an app, its playback line, empty list space and
window leave. Existing keyboard/accessibility expansion and policy-isolation
assertions remain. The standalone module smoke expectations also follow the new
app-only tooltip behavior. Evidence artifacts remain opt-in.
