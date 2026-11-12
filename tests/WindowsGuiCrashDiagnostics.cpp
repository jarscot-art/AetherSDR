// Test executables only. Preserve native failure semantics and a small dump;
// install before main so an oversized main-frame stack probe is observable.
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>

namespace {
LONG handling = 0;
LONG WINAPI recordCrash(EXCEPTION_POINTERS* exception)
{
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ACCESS_VIOLATION
        && code != EXCEPTION_ILLEGAL_INSTRUCTION) { return EXCEPTION_CONTINUE_SEARCH; }
    if (InterlockedExchange(&handling, 1)) { return EXCEPTION_CONTINUE_SEARCH; }
    std::fprintf(stderr, "NATIVE_EXCEPTION code=0x%08lX address=%p\n", code,
                 exception->ExceptionRecord->ExceptionAddress);
    HMODULE module = nullptr;
    char moduleName[MAX_PATH]{};
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                          | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(exception->ExceptionRecord->ExceptionAddress), &module)) {
        GetModuleFileNameA(module, moduleName, MAX_PATH);
        std::fprintf(stderr, "FAULT_MODULE %s offset=0x%llX\n", moduleName,
            static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(exception->ExceptionRecord->ExceptionAddress)
                                            - reinterpret_cast<ULONG_PTR>(module)));
    }
    if (const char* directory = std::getenv("AETHER_TEST_CRASH_DIR")) {
        char file[MAX_PATH]{};
        std::snprintf(file, sizeof(file), "%s\\crash-%lu.dmp", directory, GetCurrentProcessId());
        HANDLE output = CreateFileA(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exception, FALSE};
            const bool saved = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), output,
                static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo), &info, nullptr, nullptr);
            std::fprintf(stderr, "MINIDUMP saved=%d error=%lu file=%s\n", saved, saved ? 0UL : GetLastError(), file);
            CloseHandle(output);
        }
    }
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
struct Install {
    Install() {
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        std::setvbuf(stderr, nullptr, _IONBF, 0);
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        ULONG guarantee = 64 * 1024;
        const bool reserved = SetThreadStackGuarantee(&guarantee);
        AddVectoredExceptionHandler(1, recordCrash);
        std::fprintf(stderr, "GUI_DIAGNOSTICS before main; stack guarantee=%d\n", reserved);
    }
} install;
}
#endif
