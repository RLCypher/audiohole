# Publication workflow and scope

Authorized repository: `https://github.com/RLCypher/audiohole.git` (existing `origin`). Target: `work`. Dedicated branch: `research/pipewire`. Publish as a draft pull request; do not merge automatically. Update the same branch/PR for subsequent milestones without force-pushing.

Initial publication parent: `766ed88f7d7786bfb90fb7a520da34a15de010d1`, verified from remote `work`. The supplied local checkout remains on its original `work` commit. Publication uses GitHub Git-data APIs to create a tree based on the remote work tree, a commit with that parent, and a branch reference; this is the connector equivalent of committing and pushing an isolated task checkout. It avoids writing another agent's files, switching the shared checkout, or relying on the unavailable shell proxy. Verify the remote reference and PR after the write before reporting success.

## Included material

- Research Markdown documents and the original assignment plus appended publication policy.
- Exact upstream commit/blob references in `upstream/MANIFEST.json`.
- Authored isolated experiment scripts and small CSV/JSON numerical results in `experiments/`.
- Research-local `.gitignore` excluding downloaded references, scratch files, and compiled artifacts.

The explicit publication file list is produced and checked by `experiments/verify_research.py --publication-files`. Downloaded `upstream/pipewire/` and `upstream/windows/`, `work/`, `experiments/build/`, caches, and binaries are excluded. A fresh checkout can retrieve the needed header references using `experiments/fetch_references.py --probe-only`; hashes are verified before use.

## Subsequent updates

1. Read remote `work`, the existing `research/pipewire` head, and its open draft PR. Review relevant changed baseline instructions before editing research.
2. Preserve the original assignment prefix and all subsequent standing instructions in AGENTS.md.
3. Prepare and validate only curated files under this directory; do not publish downloaded source, scratch/build files, credentials, or other agents' work.
4. Base a new commit on the existing research head. If work must be incorporated, do so without rewriting history or modifying out-of-scope files; stop on an unresolved scope conflict.
5. Update the branch with a checked expected head and `force=false`; inspect concurrent changes before retrying a rejected update. Do not change `work`.
6. Update the existing draft PR description around the current findings, evidence, limitations, and next experiment. Verify published paths, head, base, and draft status, then report commit SHA and PR URL.
