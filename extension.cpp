/**
 * csgo_steamfix - SourceMod Extension
 *
 * Patches the CS:GO dedicated server engine to allow clients using the
 * archived CS:GO build (Steam app 4465480) to connect to community servers.
 */

#include "extension.h"
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
  #include <windows.h>
  #include <psapi.h>
  #pragma comment(lib, "psapi.lib")
#else
  #include <sys/mman.h>
  #include <unistd.h>
  #include <dlfcn.h>
  #include <link.h>
#endif

CCSGOSteamFix g_SteamFix;
SMEXT_LINK(&g_SteamFix);

// Linux:   FF 24 85 ?? ?? ?? ?? 8D B4 26 ?? ?? ?? ?? 31 F6
static const unsigned char g_sigLinux[]  = { 0xFF, 0x24, 0x85, 0x00, 0x00, 0x00, 0x00, 0x8D, 0xB4, 0x26, 0x00, 0x00, 0x00, 0x00, 0x31, 0xF6 };
static const unsigned char g_maskLinux[] = { 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };
static const size_t g_sigLinuxLen = sizeof(g_sigLinux);

// Windows: FF 24 85 ?? ?? ?? ?? FF 75 ?? 68
static const unsigned char g_sigWin[]    = { 0xFF, 0x24, 0x85, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x75, 0x00, 0x68 };
static const unsigned char g_maskWin[]   = { 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF };
static const size_t g_sigWinLen = sizeof(g_sigWin);

static bool      g_patched    = false;
static uint32_t  g_origValue  = 0;
static uint32_t *g_patchAddr  = NULL;

static bool SetMemoryWritable(void *addr, size_t len)
{
#ifdef _WIN32
    DWORD oldProt;
    return VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProt) != 0;
#else
    long pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t base = (uintptr_t)addr & ~((uintptr_t)pageSize - 1);
    size_t totalLen = ((uintptr_t)addr + len) - base;
    return mprotect((void *)base, totalLen, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
#endif
}

static void *SigScan(void *base, size_t len, const unsigned char *sig, const unsigned char *mask, size_t sigLen)
{
    unsigned char *start = (unsigned char *)base;
    unsigned char *end   = start + len - sigLen;

    for (unsigned char *p = start; p <= end; ++p)
    {
        bool found = true;
        for (size_t i = 0; i < sigLen; ++i)
        {
            if (mask[i] != 0x00 && (p[i] != sig[i]))
            {
                found = false;
                break;
            }
        }
        if (found)
            return (void *)p;
    }
    return NULL;
}

struct ModuleInfo_t
{
    void  *base;
    size_t size;
};

#ifdef _WIN32

static ModuleInfo_t GetEngineModule()
{
    ModuleInfo_t info;
    info.base = NULL;
    info.size = 0;

    HMODULE hMod = GetModuleHandleA("engine.dll");
    if (!hMod)
        return info;

    MODULEINFO mi;
    memset(&mi, 0, sizeof(mi));
    if (GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi)))
    {
        info.base = mi.lpBaseOfDll;
        info.size = mi.SizeOfImage;
    }
    return info;
}

#else

static ModuleInfo_t g_engineInfo;

static int dl_iterate_callback(struct dl_phdr_info *info, size_t, void *)
{
    if (info->dlpi_name && strstr(info->dlpi_name, "engine_srv.so"))
    {
        uintptr_t lo = (uintptr_t)-1, hi = 0;
        for (int i = 0; i < info->dlpi_phnum; ++i)
        {
            if (info->dlpi_phdr[i].p_type == PT_LOAD)
            {
                uintptr_t segStart = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
                uintptr_t segEnd   = segStart + info->dlpi_phdr[i].p_memsz;
                if (segStart < lo) lo = segStart;
                if (segEnd   > hi) hi = segEnd;
            }
        }
        g_engineInfo.base = (void *)lo;
        g_engineInfo.size = (size_t)(hi - lo);
        return 1;
    }
    return 0;
}

static ModuleInfo_t GetEngineModule()
{
    g_engineInfo.base = NULL;
    g_engineInfo.size = 0;
    dl_iterate_phdr(dl_iterate_callback, NULL);
    return g_engineInfo;
}

#endif

static void LogMsg(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    smutils->LogMessage(myself, "%s", buf);
}

bool CCSGOSteamFix::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
    ModuleInfo_t eng = GetEngineModule();
    if (!eng.base || !eng.size)
    {
        snprintf(error, maxlength, "[APPID] failed to locate engine module");
        return false;
    }

#ifdef _WIN32

    void *hit = SigScan(eng.base, eng.size, g_sigWin, g_maskWin, g_sigWinLen);
    if (!hit)
    {
        snprintf(error, maxlength, "[APPID] signature not found (Windows)");
        return false;
    }

    uint32_t jtAddr = *(uint32_t *)((unsigned char *)hit + 3);
    uint32_t *jt    = (uint32_t *)jtAddr;

    unsigned char *jaInstr = (unsigned char *)hit - 6;
    int32_t rel32 = *(int32_t *)(jaInstr + 2);
    uint32_t successVA = (uint32_t)(uintptr_t)(jaInstr + 6) + rel32;

    g_patchAddr = &jt[3];
    g_origValue = jt[3];

    if (!SetMemoryWritable(g_patchAddr, sizeof(uint32_t)))
    {
        snprintf(error, maxlength, "[APPID] VirtualProtect failed");
        return false;
    }

    *g_patchAddr = successVA;
    g_patched = true;

#else

    void *hit = SigScan(eng.base, eng.size, g_sigLinux, g_maskLinux, g_sigLinuxLen);
    if (!hit)
    {
        snprintf(error, maxlength, "[APPID] signature not found (Linux)");
        return false;
    }

    uint32_t jtAddr = *(uint32_t *)((unsigned char *)hit + 3);
    uint32_t *jt    = (uint32_t *)(uintptr_t)jtAddr;

    g_patchAddr = &jt[4];
    g_origValue = jt[4];

    if (!SetMemoryWritable(g_patchAddr, sizeof(uint32_t)))
    {
        snprintf(error, maxlength, "[APPID] mprotect failed");
        return false;
    }

    *g_patchAddr = jt[0];
    g_patched = true;

#endif

    LogMsg("[APPID] engine patched!");
    return true;
}

void CCSGOSteamFix::SDK_OnUnload()
{
    if (g_patched && g_patchAddr)
    {
        SetMemoryWritable(g_patchAddr, sizeof(uint32_t));
        *g_patchAddr = g_origValue;
        g_patched = false;
        LogMsg("[APPID] engine restored to original state");
    }
}
