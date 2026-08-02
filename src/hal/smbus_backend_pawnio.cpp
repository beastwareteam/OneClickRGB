#include "smbus_backend_pawnio.h"

#include <windows.h>

#include <cstring>
#include <fstream>
#include <vector>

namespace hal {

namespace {

typedef HRESULT(__stdcall* pawnio_open_t)(PHANDLE);
typedef HRESULT(__stdcall* pawnio_load_t)(HANDLE, const UCHAR*, SIZE_T);
typedef HRESULT(__stdcall* pawnio_execute_t)(HANDLE, PCSTR, const ULONG64*, SIZE_T,
                                             PULONG64, SIZE_T, PSIZE_T);
typedef HRESULT(__stdcall* pawnio_close_t)(HANDLE);

pawnio_open_t    g_open    = nullptr;
pawnio_load_t    g_load    = nullptr;
pawnio_execute_t g_execute = nullptr;
pawnio_close_t   g_close   = nullptr;

std::string ExeDir() {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return ".";
    std::string dir(path, n);
    size_t pos = dir.find_last_of("\\/");
    return pos != std::string::npos ? dir.substr(0, pos) : ".";
}

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    std::streamsize size = f.tellg();
    if (size <= 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), size));
}

}  // namespace

PawnIoBackend::~PawnIoBackend() {
    Close();
}

bool PawnIoBackend::LoadLibraryAndSymbols() {
    if (dll_) return true;

    const std::string dir = ExeDir();
    const std::string candidates[] = {
        dir + "\\PawnIOLib.dll",
        dir + "\\dependencies\\PawnIO\\PawnIOLib.dll",
        "PawnIOLib.dll",
    };

    HMODULE mod = NULL;
    for (const auto& path : candidates) {
        mod = LoadLibraryA(path.c_str());
        if (mod) break;
    }
    if (!mod) {
        last_error_ = SmbusError::LibraryMissing;
        return false;
    }

    g_open    = (pawnio_open_t)GetProcAddress(mod, "pawnio_open");
    g_load    = (pawnio_load_t)GetProcAddress(mod, "pawnio_load");
    g_execute = (pawnio_execute_t)GetProcAddress(mod, "pawnio_execute");
    g_close   = (pawnio_close_t)GetProcAddress(mod, "pawnio_close");

    if (!g_open || !g_load || !g_execute || !g_close) {
        FreeLibrary(mod);
        last_error_ = SmbusError::LibraryInvalid;
        return false;
    }

    dll_ = mod;
    return true;
}

bool PawnIoBackend::LoadSmbusModule() {
    const std::string dir = ExeDir();
    const std::string candidates[] = {
        dir + "\\SmbusI801.bin",
        dir + "\\modules\\SmbusI801.bin",
        dir + "\\dependencies\\PawnIO\\modules\\SmbusI801.bin",
        "SmbusI801.bin",
    };

    std::vector<uint8_t> blob;
    bool found = false;
    for (const auto& path : candidates) {
        if (ReadFileBytes(path, blob)) { found = true; break; }
    }
    if (!found) {
        last_error_ = SmbusError::ModuleMissing;
        return false;
    }

    if (g_load((HANDLE)handle_, blob.data(), blob.size()) != S_OK) {
        last_error_ = SmbusError::ModuleLoadFailed;
        return false;
    }
    return true;
}

bool PawnIoBackend::Open() {
    if (handle_) return true;
    last_error_ = SmbusError::None;

    if (!LoadLibraryAndSymbols()) return false;

    HANDLE h = NULL;
    if (g_open(&h) != S_OK || !h) {
        last_error_ = SmbusError::DriverNotRunning;
        return false;
    }
    handle_ = h;

    if (!LoadSmbusModule()) {
        g_close(h);
        handle_ = nullptr;
        return false;
    }
    return true;
}

bool PawnIoBackend::OpenWithRetry(int attempts, int delay_ms) {
    if (attempts < 1) attempts = 1;
    for (int i = 0; i < attempts; ++i) {
        if (Open()) return true;
        // Only a not-yet-started service is worth waiting for; a missing file
        // or a bad DLL will still be missing in five seconds.
        if (last_error_ != SmbusError::DriverNotRunning) return false;
        if (i + 1 < attempts) ::Sleep(delay_ms);
    }
    return false;
}

void PawnIoBackend::Close() {
    if (handle_ && g_close) {
        g_close((HANDLE)handle_);
        handle_ = nullptr;
    }
    if (dll_) {
        FreeLibrary((HMODULE)dll_);
        dll_ = nullptr;
        g_open = nullptr; g_load = nullptr; g_execute = nullptr; g_close = nullptr;
    }
}

int PawnIoBackend::Xfer(uint8_t addr, SmbusRw rw, uint8_t cmd, int size,
                        SmbusData* data) {
    if (!handle_ || !g_execute) return -1;

    ULONG64 in[9] = {addr, (ULONG64)rw, cmd, (ULONG64)size, 0, 0, 0, 0, 0};
    if (data) std::memcpy(&in[4], data, sizeof(SmbusData));

    ULONG64 out[5] = {0};
    SIZE_T ret_sz = 0;
    HRESULT hr = g_execute((HANDLE)handle_, "ioctl_smbus_xfer", in, 9, out, 5, &ret_sz);
    if (data) std::memcpy(data, &out[0], sizeof(SmbusData));
    return hr == S_OK ? 0 : -1;
}

void PawnIoBackend::Sleep(int ms) {
    ::Sleep(ms);
}

PawnIoBackend& RealSmbus() {
    static PawnIoBackend instance;
    return instance;
}

}  // namespace hal
