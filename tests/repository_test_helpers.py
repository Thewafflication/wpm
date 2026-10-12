"""Windows filesystem fixtures confined to caller-owned temporary roots."""

import ctypes
from ctypes import wintypes
from contextlib import contextmanager
import struct

kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateFileW.argtypes = [
    wintypes.LPCWSTR,
    wintypes.DWORD,
    wintypes.DWORD,
    ctypes.c_void_p,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.HANDLE,
]
kernel.CreateFileW.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
kernel.DeviceIoControl.argtypes = [
    wintypes.HANDLE,
    wintypes.DWORD,
    ctypes.c_void_p,
    wintypes.DWORD,
    ctypes.c_void_p,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    ctypes.c_void_p,
]
kernel.DeviceIoControl.restype = wintypes.BOOL


@contextmanager
def read_lock(path):
    """Hold a read handle that denies writes and replacement."""
    handle = kernel.CreateFileW(str(path), 0x80000000, 1, None, 3, 0, None)
    if handle == wintypes.HANDLE(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        yield
    finally:
        kernel.CloseHandle(handle)


def junction(link, target):
    link.mkdir()
    substitute = ("\\??\\" + str(target)).encode("utf-16-le")
    display = str(target).encode("utf-16-le")
    paths = substitute + b"\0\0" + display + b"\0\0"
    payload = (
        struct.pack(
            "<IHHHHHH",
            0xA0000003,
            len(paths) + 8,
            0,
            0,
            len(substitute),
            len(substitute) + 2,
            len(display),
        )
        + paths
    )
    buffer = ctypes.create_string_buffer(payload)
    returned = wintypes.DWORD()
    handle = kernel.CreateFileW(
        str(link), 0x40000000, 3, None, 3, 0x02200000, None
    )
    if handle == wintypes.HANDLE(-1).value:
        link.rmdir()
        return False
    try:
        created = kernel.DeviceIoControl(
            handle,
            0x000900A4,
            buffer,
            len(payload),
            None,
            0,
            ctypes.byref(returned),
            None,
        )
    finally:
        kernel.CloseHandle(handle)
    if not created:
        link.rmdir()
    return bool(created)
