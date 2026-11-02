"""Portable regression checks for Windows shell API return/ownership contracts.

These emulate return values only. Native extraction and rendering remain Windows
CI checks; no successful pixel/shell validation is claimed by these tests.
"""
import ctypes
from ctypes import wintypes
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts' / 'branding'))
from windows_icon_check import _extract_shell_icons
from windows_icon_check import _load_reference_icon


class ShellExtractionContract(unittest.TestCase):
    def test_reference_loads_numeric_resource_one_without_shell_query(self):
        def loader(module, resource, image_type, width, height, flags):
            self.assertEqual((module, resource.value, image_type, width, height, flags),
                             (99, 1, 1, 32, 32, 0))
            return 103

        owned = []
        self.assertEqual(_load_reference_icon(loader, 99, 32, 32, owned), 103)
        self.assertEqual([icon.value for icon in owned], [103])

    def test_missing_reference_is_rejected(self):
        owned = []
        with self.assertRaises(ValueError):
            _load_reference_icon(lambda *args: 0, 99, 32, 32, owned)
        self.assertEqual(owned, [])

    def extract(self, count, large=101, small=102):
        self.owned = []

        def api(filename, index, large_output, small_output, requested):
            self.assertEqual((filename, index, requested), ('audiohole.exe', 0, 1))
            ctypes.cast(large_output, ctypes.POINTER(wintypes.HANDLE))[0] = large
            ctypes.cast(small_output, ctypes.POINTER(wintypes.HANDLE))[0] = small
            return count

        return _extract_shell_icons(api, Path('audiohole.exe'), self.owned)

    def test_single_pair_success(self):
        self.assertEqual([icon.value for icon in self.extract(1)], [101, 102])

    def test_two_returned_handles_are_success(self):
        self.assertEqual([icon.value for icon in self.extract(2)], [101, 102])
        self.assertEqual([icon.value for icon in self.owned], [101, 102])

    def test_zero_and_error_results_are_rejected(self):
        for count in (0, 0xffffffff):
            with self.subTest(count=count), self.assertRaises(ValueError):
                self.extract(count)
            self.assertEqual([icon.value for icon in self.owned], [101, 102])

    def test_partial_results_are_rejected_and_retained_for_cleanup(self):
        for large, small in ((101, 0), (0, 102), (0, 0)):
            with self.subTest(large=large, small=small), self.assertRaises(ValueError):
                self.extract(2, large, small)
            self.assertEqual([icon.value for icon in self.owned],
                             [icon for icon in (large, small) if icon])


if __name__ == '__main__':
    unittest.main()
