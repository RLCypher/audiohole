# Optional Windows installer

The distributed ZIP is a directly runnable portable application folder. Its main
folder contains AudioHole.exe, the optional versioned AudioHole-Setup executable,
a Guides and Manuals folder containing the user manual, quick-start and tuning
guides, manifesto, and AI disclosure, and the supporting runtime files. These
documents are included in both portable and installed copies. The owner-authored
AudioHole License.txt is displayed by the installer and included beside the app;
the complete GPLv3 text and original-asset policy are in licenses/GPL-3.0.txt and
licenses/assets.txt. Third-party notices and licenses/SOURCE.txt are retained.
Tutorial dialogue has one source at personalities/tutorial/tutorial.txt.
Engineering specifications, validation evidence, redundant embedded artwork,
and examples are excluded from the runtime payload. The docs folder contains only
build-info.json, used by installer version checks and the owned-file manifest.
Installation is optional:
extract the complete ZIP and run AudioHole.exe to use it without installing.

Build that folder with Inno Setup 7.1.1 and scripts/Build-Installer.ps1, using the
audited runtime ZIP produced by scripts/Package.ps1 as input. By default it writes
build/portable. An empty output folder receives the runtime files; an existing
output folder must match the input package byte for byte before setup is added.
The installer embeds only the clean runtime input, so it cannot recursively embed
itself. Test-PortableBundle.ps1 verifies that setup and AudioHole.exe are together
and that the folder contains no source archive or unexpected files. The installer
does not add a service, driver or startup entry. Launching after installation is optional.

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

CI builds Debug and Release installers after native tests and package validation,
then runs Test-Installer.ps1 against a temporary per-user destination. This checks
fresh installation, higher/equal/lower/malformed version decisions, manifest file
retirement, custom-file preservation, uninstall retention and an explicit purge
when no preexisting user data exists. Every setup pass verifies its current
registered uninstaller, and a further reinstall checks that the installer's own
Uninstall action resolves the same path. Uninstall logs are retained as evidence. CI also supplies the matching corresponding
source as a separate Source artifact on the same CI run. The portable binary ZIP
contains no source archive. Source upload must succeed before binary publication.

The lifecycle check waits for the temporary uninstall worker, not just its launcher.
Within the same 60-second deadline it requires the pinned Inno engine's success and
log-closed records and an exclusive read of the closed log. Data-removal assertions,
reinstallation and log collection happen only after that completion. A failed
worker still fails the check; temporary-file cleanup cannot mask that failure.
