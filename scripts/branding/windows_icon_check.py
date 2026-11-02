"""Native Windows shell checks used by verify_assets.py, without external modules.

The PE audit establishes resource 1's identity first. Compare the shell's actual
index-zero/associated icons to that resource, drawing both through the same GDI
path. No icon cache, association, shortcut or OS setting is modified. All returned
icons, bitmaps, memory DCs and module mappings are owned here and released before returning.
"""
import ctypes
from ctypes import wintypes
import struct


def _extract_shell_icons(extractor, executable, owned):
    """Extract index zero, recording even partial results for caller cleanup.

    Windows may count the two large/small handles separately. Success requires
    a non-error return and both handles, rather than an exact success count.
    Negative index -1 is deliberately avoided: it is an icon-count query.
    """
    large, small = wintypes.HANDLE(), wintypes.HANDLE()
    count = extractor(str(executable), 0, ctypes.byref(large), ctypes.byref(small), 1)
    owned.extend(icon for icon in (large, small) if icon.value)
    if count in (0, 0xffffffff) or not large.value or not small.value:
        raise ValueError(f'Windows failed to extract index-zero large/small icons (result {count})')
    return large, small


def _load_reference_icon(loader, module, width, height, owned):
    """Load owned IMAGE_ICON resource 1 directly, without shell index semantics."""
    icon = loader(module, ctypes.c_void_p(1), 1, width, height, 0)
    if not icon:
        raise ValueError('Windows failed to load canonical icon resource 1')
    owned.append(wintypes.HANDLE(icon))  # No LR_SHARED: caller must destroy it.
    return icon


def verify_shell_icons(executable):
    user = ctypes.WinDLL('user32', use_last_error=True)
    shell = ctypes.WinDLL('shell32', use_last_error=True)
    gdi = ctypes.WinDLL('gdi32', use_last_error=True)
    ole = ctypes.WinDLL('ole32')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    ole.CoInitializeEx.argtypes = [ctypes.c_void_p, wintypes.DWORD]
    ole.CoInitializeEx.restype = ctypes.c_long
    handle = wintypes.HANDLE
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, handle, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = handle
    kernel.FreeLibrary.argtypes = [handle]
    user.LoadImageW.argtypes = [handle, ctypes.c_void_p, wintypes.UINT,
                               ctypes.c_int, ctypes.c_int, wintypes.UINT]
    user.LoadImageW.restype = handle
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
    module = None
    try:
        # Map resources without loading executable code or running initialization.
        # Use the same explicit ID as the app, not ExtractIconEx's special -1 index.
        # LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE
        module = kernel.LoadLibraryExW(str(executable), None, 0x2 | 0x20)
        if not module:
            raise ctypes.WinError(ctypes.get_last_error())
        extracted = _extract_shell_icons(shell.ExtractIconExW, executable, owned)
        for position, (metric_x, metric_y) in enumerate(((11, 12), (49, 50))):
            width, height = user.GetSystemMetrics(metric_x), user.GetSystemMetrics(metric_y)
            expected = _load_reference_icon(user.LoadImageW, module, width, height, owned)
            reference = render(expected, width, height)
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
        if module:
            kernel.FreeLibrary(module)
        ole.CoUninitialize()
