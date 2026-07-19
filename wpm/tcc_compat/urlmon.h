#pragma once

/* Minimal URL Moniker declaration for TinyCC's compact WinAPI headers. */
HRESULT WINAPI URLDownloadToFileA(
    void *caller,
    LPCSTR url,
    LPCSTR file_name,
    DWORD reserved,
    void *status_callback
);
