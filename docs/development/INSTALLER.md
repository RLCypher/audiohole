# Optional Windows installer

Downloads are presented as AudioHole Installer, AudioHole Portable, AudioHole
ZIP-debug and AudioHole ZIP-source, followed by the display version and seven-character
commit. For example: AudioHole Installer v0.1-34aca5b.exe. The standalone Release
installer is downloaded directly as an EXE, using upload-artifact v7's archive:false
mode. The other three downloads are the original ZIP files, with no outer wrapper.
No Debug installer is built. Portable folders contain AudioHole.exe, the supplied
AudioHole License.rtf and README.txt, Guides and Manuals, licenses and runtime
resources. Neither installer nor source is bundled inside portable downloads.

The owner-authored RTF is embedded unchanged in the application, displayed by
Inno Setup, and installed beside AudioHole.exe. Both document replacements retain
the uploaded bytes. The complete GPLv3 text and original-asset policy remain in
licenses/GPL-3.0.txt and licenses/assets.txt. Native Rich Edit renders the portable
agreement, preserving typography with theme-readable ink. The same viewer opens
from About/Update without acceptance controls or receipt changes.

Build the installer with Inno Setup 7.1.1 and scripts/Build-Installer.ps1, using the
audited runtime ZIP produced by scripts/Package.ps1 as input. Its separate output
folder (build/installer by default) must be empty and contains only setup. The
compiler consumes temporary runtime staging; it never modifies the portable
folder. The compiler agreement and packaged RTF must match byte for byte.
Test-PortableBundle.ps1 rejects installers, source archives and unexpected files.
The optional installer does not add a service, driver or startup entry. Launching
after installation is optional. Ordinary use remains portable and non-administrator.

The default machine installation is C:\Program Files\AudioHole. A user-selected
folder is supported. Existing registered installations are located using Windows'
uninstall registration. A portable copy in the selected destination is detected
from AudioHole.exe. Existing versions are read exclusively from docs/build-info.json;
three numeric version components are compared numerically. A higher bundled
application version offers Update; equal/older or unavailable versions offer
Reinstall. Build number and commit identify the package but do not change that
policy. Malformed/missing metadata never invents a version.

The existing-installation page offers Update/Reinstall, Uninstall and Cancel.
An older portable copy with no file manifest must be removed manually; the
installer will not guess which files it owns. Current portable copies can have
their manifest-owned files removed while preserving data. Registered installations
use the uninstaller recorded in their Windows registration, validated against the
selected installation directory. Inno may allocate unins001.exe or a later number
while an earlier uninstaller finishes cleanup; no fixed filename is assumed. Reinstallation replaces owned application
files and retires obsolete manifest entries while preserving unrelated files and
all user data. Application files are never used as a place to save user state.
Exit AudioHole through its tray/menu before replacement. A running AudioHole
process or an unavailable process-status check blocks installation/removal.

installer/ReleaseNotes.txt is developer-authored copy shown before installation,
initially “Initial release of AudioHole”. Update it when schema compatibility,
rescanning or reconfiguration needs explanation. Do not infer migration advice
from the version number alone.

Desktop and Start menu shortcuts are selectable. The final page offers Launch
AudioHole and Exit. Launch uses the original unelevated user. The normal installer
requires elevation for Program Files; /CURRENTUSER supports a per-user installation
and automated tests without elevation. It does not alter the portable app's
ordinary non-administrator runtime.

Uninstall asks “Keep profiles, configurations, and settings?” and defaults to Yes.
No removes only the uninstalling account's %LOCALAPPDATA%\AudioHole directory,
including preferences, devices, profiles, Plug Board files and onboarding state.
Other Windows accounts' data is never deleted. Elevated operation under different
administrator credentials affects that administrator's data, not another user's;
retain the default and remove the intended user's data from their own account.
Unattended uninstall keeps data unless /PURGEDATA=1 is explicitly supplied.
Startup registration is removed only if it points to this exact installation.

CI builds a Release installer after native tests and package validation,
then runs Test-Installer.ps1 against a temporary per-user destination. This checks
fresh installation, higher/equal/lower/malformed version decisions, manifest file
retirement, custom-file preservation, uninstall retention and an explicit purge
when no preexisting user data exists. Every setup pass verifies its current
registered uninstaller, and a further reinstall checks that the installer's own
Uninstall action resolves the same path. Uninstall diagnostics are kept as ordinary test logs; evidence copies are opt-in. A dedicated source job fetches the pinned dependency sources and publishes one
matching Source artifact before either binary job can publish. The portable binary ZIP
contains no source archive. Source upload must succeed before binary publication.

The lifecycle check waits for the temporary uninstall worker, not just its launcher.
Within the same 60-second deadline it requires the pinned Inno engine's success and
log-closed records and an exclusive read of the closed log. Data-removal assertions,
reinstallation and log collection happen only after that completion. A failed
worker still fails the check; temporary-file cleanup cannot mask that failure.

The complete installer and uninstaller use Inno Setup modern dynamic styling,
following Windows light/dark mode. Portable first use shows the same embedded
agreement and records acceptance beside the tutorial flag. Registered installed
copies have already presented this agreement through setup. Replay does not
reset license acceptance. Artifact names use the display version and seven-character
commit prefix; full version/commit metadata remains in docs/build-info.json.
