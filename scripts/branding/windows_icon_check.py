"""Native Windows shell checks used by verify_assets.py, without external modules.

The PE audit establishes resource 1's identity first. Compare the shell's actual
index-zero/associated icons to that resource, drawing both through the same GDI
path. No icon cache, association, shortcut or OS setting is modified. All returned
icons, bitmaps and memory DCs are owned here and released before returning.
"""
import ctypes
from ctypes import wintypes
import struct


def verify_shell_icons(executable):
    user = ctypes.WinDLL('user32', use_last_error=True)
    shell = ctypes.WinDLL('shell32', use_last_error=True)
    gdi = ctypes.WinDLL('gdi32', use_last_error=True)
    ole = ctypes.WinDLL('ole32')
    ole.CoInitializeEx.argtypes = [ctypes.c_void_p, wintypes.DWORD]
    ole.CoInitializeEx.restype = ctypes.c_long
    handle = wintypes.HANDLE
    user.DestroyIcon.argtypes = [handle]
    user.DrawIconEx.argtypes = [handle, ctypes.c_int, ctypes.c_int, handle,
                               ctypes.c_int, ctypes.c_int, wintypes.UINT, handle, wintypes.UINT]
    user.GetSystemMetrics.argtypes = [ctypes.c_int]
    gdi.CreateCompatibleDC.argtypes = [handle]
    gdi.CreateCompatibleDC.restype = handle
    gdi.DeleteDC.argtypes = [handle]
    gdi.CreateDIBSection.argtypes = [handle, ctypes.c_void_p, wintypes.UINT,
                                    ctypes.POINTER(ctypes.c_void_p), handle, wintypes.DWORD]
    gdi.CreateDIBSection.restype = handle
    gdi.SelectObject.argtypes = [handle, handle]
    gdi.SelectObject.restype = handle
    gdi.DeleteObject.argtypes = [handle]
    shell.ExtractIconExW.argtypes = [wintypes.LPCWSTR, ctypes.c_int,
                                    ctypes.POINTER(handle), ctypes.POINTER(handle), wintypes.UINT]
    shell.ExtractIconExW.restype = wintypes.UINT

    class ShellFileInfo(ctypes.Structure):
        _fields_ = [('icon', handle), ('index', ctypes.c_int), ('attributes', wintypes.DWORD),
                    ('display_name', wintypes.WCHAR * 260), ('type_name', wintypes.WCHAR * 80)]

    shell.SHGetFileInfoW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD,
                                   ctypes.POINTER(ShellFileInfo), wintypes.UINT, wintypes.UINT]
    shell.SHGetFileInfoW.restype = ctypes.c_size_t

    def render(icon, width, height):
        dc = gdi.CreateCompatibleDC(None)
        if not dc:
            raise ctypes.WinError(ctypes.get_last_error())
        bitmap = previous = None
        try:
            # Top-down 32-bit BGRA buffer, initialized to opaque white. Compare
            # identical rendering paths, independent of HICON handle identity.
            info = ctypes.create_string_buffer(struct.pack(
                '<IiiHHIIiiII', 40, width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0))
            pixels = ctypes.c_void_p()
            bitmap = gdi.CreateDIBSection(dc, info, 0, ctypes.byref(pixels), None, 0)
            if not bitmap or not pixels.value:
                raise ctypes.WinError(ctypes.get_last_error())
            previous = gdi.SelectObject(dc, bitmap)
            if not previous or previous == ctypes.c_void_p(-1).value:
                raise ctypes.WinError(ctypes.get_last_error())
            ctypes.memset(pixels, 255, width * height * 4)
            if not user.DrawIconEx(dc, 0, 0, icon, width, height, 0, None, 3):  # DI_NORMAL
                raise ctypes.WinError(ctypes.get_last_error())
            gdi.GdiFlush()  # Complete GDI writes before reading shared DIB memory.
            return ctypes.string_at(pixels, width * height * 4)
        finally:
            if previous and previous != ctypes.c_void_p(-1).value:
                gdi.SelectObject(dc, previous)
            if bitmap:
                gdi.DeleteObject(bitmap)
            gdi.DeleteDC(dc)

    # SHGetFileInfo requires COM initialization on the calling thread.
    result = ole.CoInitializeEx(None, 2)  # COINIT_APARTMENTTHREADED
    if result < 0:
        raise OSError(f'Shell icon check COM initialization failed: HRESULT {result & 0xffffffff:08x}')
    owned = []
    try:
        def extract(index):
            large, small = handle(), handle()
            count = shell.ExtractIconExW(str(executable), index,
                                         ctypes.byref(large), ctypes.byref(small), 1)
            owned.extend(icon for icon in (large, small) if icon.value)
            if count != 1 or not large.value or not small.value:
                raise ValueError(f'Windows failed to extract application icon at index {index}')
            return large, small

        expected = extract(-1)  # Negative index selects numeric resource ID 1.
        extracted = extract(0)
        for position, (metric_x, metric_y) in enumerate(((11, 12), (49, 50))):
            width, height = user.GetSystemMetrics(metric_x), user.GetSystemMetrics(metric_y)
            reference = render(expected[position], width, height)
            if render(extracted[position], width, height) != reference:
                raise ValueError('Windows index-zero extraction differs from the canonical logo')
            info = ShellFileInfo()
            flags = 0x100 | position  # SHGFI_ICON, plus SHGFI_SMALLICON when position == 1.
            result = shell.SHGetFileInfoW(str(executable), 0, ctypes.byref(info),
                                          ctypes.sizeof(info), flags)
            if info.icon:
                owned.append(handle(info.icon))
            if not result or not info.icon:
                raise ValueError('Windows failed to retrieve the executable associated icon')
            if render(info.icon, width, height) != reference:
                raise ValueError('Windows associated executable icon differs from the canonical logo')
        print('PASS: Windows ExtractIconEx and associated large/small icons display the canonical logo')
    finally:
        for icon in owned:
            user.DestroyIcon(icon)
        ole.CoUninitialize()
