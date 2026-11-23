"""Offline release gates using inert ZIP/EXE-shaped fixtures; never execute them."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

MODULE = Path(__file__).resolve().parents[2] / 'scripts' / 'PrepareRelease.py'
spec = importlib.util.spec_from_file_location('release_preparation', MODULE)
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleasePreparationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.commit = 'a' * 40
        self.version = '0.1.0'
        self.names = release.download_names(self.version, self.commit)
        self.runtime = {'AudioHole.exe': b'inert fixture', 'licenses/GPL-3.0.txt': b'license fixture'}
        self.metadata = {
            'format': 'audiohole-build-info', 'schema_version': 1, 'product': 'AudioHole',
            'configuration': 'Release', 'source_dirty': False, 'commit': self.commit,
            'version': self.version, 'release_tag': 'v0.1.0',
            'release_repository': release.REPOSITORY,
            'built_at_utc': '2026-11-22T00:00:00Z', 'build_date_utc': '2026-11-22',
            'vintage': 'November 22, 2026',
            'runtime_files': [*self.runtime, 'docs/build-info.json'],
        }
        for key, name in zip(('installer_file', 'portable_archive', 'debug_archive', 'source_archive'), self.names):
            self.metadata[key] = name
        (self.directory / self.names[0]).write_bytes(b'MZ inert installer fixture; never execute')
        self.write_runtime(1, 'Release')
        self.write_runtime(2, 'Debug')
        self.source_metadata = {key: self.metadata[key] for key in (
            'source_dirty', 'commit', 'version', 'built_at_utc', 'build_date_utc', 'vintage')}
        self.write_source()

    def archive(self, name, files):
        with zipfile.ZipFile(self.directory / name, 'w') as archive:
            for member, data in files.items():
                archive.writestr('AudioHole/' + member, data)

    def write_runtime(self, index, configuration, changes=None):
        metadata = copy.deepcopy(self.metadata)
        metadata['configuration'] = configuration
        metadata.update(changes or {})
        self.archive(self.names[index], {**self.runtime, 'docs/build-info.json': json.dumps(metadata)})

    def write_source(self, omit=None):
        files = {'LICENSE': '', 'BUILDING.md': '', 'CMakeLists.txt': '',
                 'source-build-info.json': json.dumps(self.source_metadata)}
        for dependency in ('wxwidgets', 'nlohmann_json', 'miniz', 'miniaudio', 'googletest'):
            if dependency != omit:
                files['dependencies/' + dependency + '/LICENSE'] = 'inert source fixture'
        self.archive(self.names[3], files)

    def audit(self):
        return release.audit_assets(self.directory, self.version, self.commit)

    def test_exact_download_names_and_nonzero_patch(self):
        self.assertEqual(self.names, [
            'AudioHole Installer v0.1-aaaaaaa.exe', 'AudioHole Portable v0.1-aaaaaaa.zip',
            'AudioHole ZIP-debug v0.1-aaaaaaa.zip', 'AudioHole ZIP-source v0.1-aaaaaaa.zip'])
        self.assertIn('v0.1.2-', release.download_names('0.1.2', self.commit)[0])
        with self.assertRaises(ValueError):
            release.download_names(self.version, 'a' * 7)

    def test_accepts_only_matching_clean_debug_release_and_source(self):
        self.assertEqual(self.audit(), self.names)
        for changes in ({'source_dirty': True}, {'commit': 'b' * 40}, {'version': '0.1.1'},
                        {'configuration': 'Debug'}, {'vintage': 'Different date'},
                        {'installer_file': 'unrelated.exe'}, {'release_repository': 'other/repo'}):
            with self.subTest(changes=changes):
                self.write_runtime(1, 'Release', changes)
                with self.assertRaises(ValueError):
                    self.audit()
        self.write_runtime(1, 'Release')
        self.assertEqual(self.audit(), self.names)

    def test_rejects_missing_or_unexpected_downloads(self):
        extra = self.directory / 'unexpected.exe'
        extra.write_bytes(b'MZ')
        with self.assertRaises(ValueError):
            self.audit()
        extra.unlink()
        (self.directory / self.names[0]).unlink()
        with self.assertRaises(ValueError):
            self.audit()

    def test_rejects_wrapped_installer_and_bundled_source(self):
        (self.directory / self.names[0]).write_bytes(b'PK zip wrapper')
        with self.assertRaisesRegex(ValueError, 'Windows executable'):
            self.audit()
        (self.directory / self.names[0]).write_bytes(b'MZ fixture')
        self.runtime[self.names[3]] = b'bundled source fixture'
        self.write_runtime(1, 'Release')
        with self.assertRaisesRegex(ValueError, 'manifest'):
            self.audit()

    def test_source_must_include_all_corresponding_dependencies(self):
        self.write_source(omit='wxwidgets')
        with self.assertRaisesRegex(ValueError, 'wxwidgets'):
            self.audit()
        self.source_metadata['commit'] = 'b' * 40
        self.write_source()
        with self.assertRaisesRegex(ValueError, 'does not match'):
            self.audit()

    def test_equivalent_timestamp_serialization_matches(self):
        self.source_metadata['built_at_utc'] = '2026-11-22T00:00:00+00:00'
        self.write_source()
        self.assertEqual(self.audit(), self.names)

    def test_native_build_gate_rejects_untrusted_failed_and_mismatched_runs(self):
        run = {'head_sha': self.commit, 'head_branch': 'main', 'event': 'push',
               'path': '.github/workflows/windows.yml', 'status': 'completed',
               'conclusion': 'success', 'html_url': 'https://github.com/example-run'}
        jobs = {'jobs': [{'name': name, 'conclusion': 'success'} for name in (
            'Prepare matching source', 'build-and-test (debug)', 'build-and-test (release)')]}
        with patch.object(release, 'git_commit', return_value=self.commit):
            with patch.object(release, 'github', side_effect=[run, jobs]):
                release.check_run('123')
            for changes in ({'head_sha': 'b' * 40}, {'head_branch': 'untrusted'},
                            {'event': 'pull_request'}, {'conclusion': 'failure'},
                            {'status': 'in_progress'}, {'path': '.github/workflows/other.yml'}):
                with self.subTest(changes=changes):
                    with patch.object(release, 'github', return_value={**run, **changes}):
                        with self.assertRaises(ValueError):
                            release.check_run('123')
            with patch.object(release, 'github', side_effect=[run, {'jobs': []}]):
                with self.assertRaises(ValueError):
                    release.check_run('123')

    def test_run_id_cannot_be_a_url_or_shell_expression(self):
        with patch.object(release, 'github') as github:
            for run_id in ('', '0', '-1', 'https://example.com', '123; command'):
                with self.assertRaises(ValueError):
                    release.check_run(run_id)
            github.assert_not_called()

    def test_rejects_unsafe_and_duplicate_zip_members(self):
        self.archive(self.names[1], {'../escape': '', 'docs/build-info.json': '{}'})
        with self.assertRaisesRegex(ValueError, 'Unsafe ZIP'):
            self.audit()
        self.archive(self.names[1], {'docs/build-info.json': '{}'})
        with zipfile.ZipFile(self.directory / self.names[1], 'a') as archive:
            import warnings
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning)
                archive.writestr('AudioHole/docs/build-info.json', '{}')
        with self.assertRaisesRegex(ValueError, 'Duplicate ZIP'):
            self.audit()


if __name__ == '__main__':
    unittest.main()
