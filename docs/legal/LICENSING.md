# AudioHole licensing

First-party source code, build scripts and technical documentation are licensed
under the GNU General Public License version 3 only (**GPL-3.0-only**), except
where an individual file carries different terms. See the root LICENSE for the
complete license. The owner selected GPLv3; no automatic permission for later
GPL versions is added. Copyright remains with the respective authors.

You may use, study, modify and redistribute the code, including commercially,
under the GPL's terms. Donations and charging for distribution are permitted.
There is no warranty, to the extent permitted by applicable law. Contributions
use the same license and DCO sign-off, without copyright assignment or a broad
relicensing agreement. A later licensing change would need the necessary rights;
it cannot revoke compliant recipients' existing GPL rights.

## Artwork and branding

The AudioHole logo, Norm and Horu artwork/animations and fictional application
icons are **excluded from this code-license declaration**. Their licensing remains
under review by the owner. Do not imply that this GPL declaration grants a new
asset, trademark or endorsement license. Preserve existing asset provenance and
third-party notices. Material Icons and Plug Board Material Symbols retain their
existing Apache-2.0 terms. The DCO has its own verbatim-copy terms.

## Dependencies and notices

The distribution's licenses directory retains the upstream terms:

| Component | License |
| --- | --- |
| wxWidgets | wxWindows Library Licence 3.1, including its LGPL exception |
| nlohmann/json | MIT |
| miniz | MIT |
| miniaudio | upstream MIT / Unlicense alternatives |
| Material Icons / Material Symbols | Apache-2.0 with notices |
| GoogleTest (tests only) | BSD-3-Clause |
| Inno Setup (optional installer) | upstream Inno Setup License |

These licenses permit integration with GPLv3; their notices and exceptions remain
in effect. Windows SDK/system libraries are supplied by the operating system and
are not redistributed in the package. Inno Setup builds the separate installer;
its upstream license permits distributing the generated installer.

## Binary releases and corresponding source

Every published runtime/installer artifact must provide its matching source
archive alongside the binaries, without an additional charge. The source archive
contains the exact first-party tree and the pinned dependency source used by the
build, including build scripts and license notices. See docs/development/SOURCE.md.
The build-info.json commit, version, build and source archive name identify the
matching source. Never substitute a newer branch tip for the source of an older
binary. If redistributing modified binaries, supply corresponding modified source
and preserve the GPL license and notices. Do not publish an artifact without its
source archive. Artwork terms remain separate and must be settled before a public
release of those assets.
