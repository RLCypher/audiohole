# v0.1.1 maintenance

## Installer license contrast

- [x] Fix the license agreement's text colors in the Windows installer.

Reported after v0.1 went live: the dark installer displays black agreement text
on a dark background, making the license difficult to read.

Inno Setup preserves explicit black RTF ink even when its wizard becomes dark.
The installer now overrides only rendered foreground color for the complete
agreement, retaining bold, fonts and paragraphs. The application verifies its
complete rendered range too. `docs/legal/AudioHole License.rtf` stays byte-for-byte
unchanged; Release portable and Debug use the same viewer.

Native CI checks the complete agreement's ink, heading and mixed bold/plain
formatting in Windows light, dark and high-contrast modes. Appearance settings
are restored after the tests. Inno resolves its dynamic style when setup starts;
changing Windows appearance while setup is open does not restyle the wizard.
Evidence captures remain opt-in.

The published v0.1 installer is retained. These changes belong to the unpublished
v0.1.1 release preparation.

## Settings: About/Update actions and issue reporting

- [x] Add a **Check for updates** button to Settings → About/Update. Clicking it
  immediately starts a manual update check using the existing updater, independent
  of the automatic-check preference. When a successful check finds no newer
  release, show a popup with this exact message: **Sorry boss, nothing new yet.**
  Include an **Ok** button that closes the popup when clicked.
  Keep check failures distinct from a successful no-update result.
- [x] Below the existing GitHub repository link, add this exact text:
  **Got issues? Bug reports? Tell me about them.** Follow it with a clickable link
  to <https://github.com/RLCypher/audiohole/issues>.

Native About/Update smoke dispatches the actual button through a fixture worker
in both app themes. It covers current/older/newer/skipped releases, automatic
checks disabled, a recent previous check, network errors, the exact no-update
popup and a real **Ok** click that closes it. Existing update actions, license
viewing, version identity and routing/editor invariants remain checked.

## Installer artwork

- [x] Replace the stock installer artwork with the original AudioHole logo.
  The header and square welcome/finish images use the canonical PNG directly;
  the existing application icon remains the setup/uninstall icon.

Release preparation includes native Debug/Release validation and an unpublished
v0.1.1 draft with matching source and checksums. The owner writes the main README.
