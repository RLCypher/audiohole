# Distribution cleanup and inactive-source disclosure

## Runtime layout

The portable folder keeps AudioHole.exe, its optional setup executable, the
owner-supplied README.txt, and AudioHole License.rtf together. The installer shows
that same owner-supplied agreement. Full software and asset terms remain separate
at licenses/GPL-3.0.txt and licenses/assets.txt, alongside upstream notices and
the corresponding-source instructions in licenses/SOURCE.txt.

Guides and Manuals contains the user manual, Speedrunning the Tutorial, Advanced
Buffer Tuning Guide, The AudioHole Manifesto, and the revised AI disclosure as text
documents. Uploaded documents and the two authored manuals are copied verbatim;
Git attributes preserve their bytes on Windows checkouts.

The sole tutorial authoring source is personalities/tutorial/tutorial.txt. It
consolidates the original lesson and Plug Board insert. GenerateTutorial.py reads
that file and generates the existing inert compiled catalogue. Comparison against
the previous catalogue verified all 88 beats, choices, variants and expressions
are identical. The 13 named phases retain their MSVC literal-size limits.

The binary download excludes engineering specifications, validation evidence,
example profiles, the package-authoring README, provenance copies and redundant
branding/fallback image files. These remain available in the source tree where
needed for builds, verification or maintenance. Branding and fallback artwork are
embedded in AudioHole.exe; runtime avatar files, tutorial app icons, the UI font,
private scanner helper and third-party notices remain included.

The docs directory in the download contains only generated build-info.json.
The installer, uninstaller and package checks use this metadata for versioning
and ownership; it is generated after the audited Runtime installation. Matching
source remains a separate download, never embedded in the portable binary ZIP.

## Plug Board behavior

Activating an Inactive group enables the board's inactive-source preference when
needed and expands that socket. It preserves the activity interval, application
assignments, policy state and settings-drawer position. Later clicks collapse or
expand individual groups. Pointer, keyboard and accessibility actions share this
behavior. Neither the audio engine nor the board storage schema changes.

## Verification

- Portable model build and all 297 model/contract tests passed.
- GenerateTutorial.py --check and CheckPlugBoardInputs.py passed.
- All eight supplied/authored document copies matched their inputs byte for byte.
- Distribution-tool fixtures passed: exact file lists, verbatim README, metadata,
  installer colocation, nonrecursive payload, source exclusion, mixed-build and
  missing-file rejection, overwrite protection and staging cleanup.
- The complete Windows cross-build and the supplemental module smoke source
  compiled. A real cross-built Runtime ZIP contained exactly 744 approved files;
  its guides, agreements, GPL, asset policy and tutorial script matched their
  source files. No source code/archive or engineering docs were bundled.
- Native package smoke launches the actual cleaned package without external
  branding/fallback image copies. Embedded GUI smoke covers each socket's
  inactive disclosure with a closed drawer, keyboard collapse, accessible
  expansion and unchanged assignments/activity interval/policy writes. The module
  smoke also checks that inactive application rows become visible exactly once.

Native Windows GUI/installer execution is performed by the Debug and Release CI
jobs. Cross-compilation is supplemental and does not establish native execution.
