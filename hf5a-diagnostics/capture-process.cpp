#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Standalone Windows SDK diagnostic; never linked into AetherSDR or its tests.
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct WindowsContext { DWORD pid; std::wofstream* out; };
BOOL CALLBACK windowText(HWND window, LPARAM parameter)
{
    auto& context = *reinterpret_cast<WindowsContext*>(parameter);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != context.pid) return TRUE;
    wchar_t title[4096]{};
    wchar_t kind[256]{};
    GetWindowTextW(window, title, 4096);
    GetClassNameW(window, kind, 256);
    *context.out << L"HWND=" << window << L" class=" << kind
                 << L" visible=" << IsWindowVisible(window) << L" text=" << title << L'\n';
    return TRUE;
}
BOOL CALLBACK topWindow(HWND window, LPARAM parameter)
{
    windowText(window, parameter);
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid == reinterpret_cast<WindowsContext*>(parameter)->pid) {
        EnumChildWindows(window, windowText, parameter);
    }
    return TRUE;
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc == 2 && std::wstring(argv[1]) == L"--probe") {
        Sleep(120000);
        return 0;
    }
    if (argc != 4) return 2;
    const DWORD pid = wcstoul(argv[1], nullptr, 10);
    const std::filesystem::path output(argv[2]);
    std::filesystem::create_directories(output);
    std::wofstream log(output / L"threads.txt");
    std::wofstream windows(output / L"windows.txt");
    WindowsContext context{pid, &windows};
    EnumWindows(topWindow, reinterpret_cast<LPARAM>(&context));
    Handle process{OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid)};
    if (!process.value) { log << L"OpenProcess error=" << GetLastError() << L'\n'; return 3; }

    // Suspend every thread first: the test's own watchdog must not terminate it
    // while its evidence is being collected. The caller kills this PID afterwards.
    Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)};
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    std::vector<DWORD> threads;
    if (Thread32First(snapshot.value, &entry)) {
        do {
            if (entry.th32OwnerProcessID != pid) continue;
            threads.push_back(entry.th32ThreadID);
            Handle thread{OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID)};
            const DWORD previous = thread.value ? SuspendThread(thread.value) : DWORD(-1);
            log << L"suspend tid=" << entry.th32ThreadID << L" previous=" << previous << L'\n';
        } while (Thread32Next(snapshot.value, &entry));
    }
    Handle dump{CreateFileW((output / L"process.dmp").c_str(), GENERIC_WRITE,
                           0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    const auto flags = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo |
                                                MiniDumpWithUnloadedModules | MiniDumpWithFullMemoryInfo);
    const BOOL dumped = dump.value != INVALID_HANDLE_VALUE &&
        MiniDumpWriteDump(process.value, pid, dump.value, flags, nullptr, nullptr, nullptr);
    log << L"MiniDumpWriteDump=" << dumped << L" error=" << (dumped ? 0 : GetLastError()) << L'\n';
    log.flush();

    // Offline local PDBs and DLL exports only; no network symbol-server stalls.
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS |
                  SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    const BOOL symbols = SymInitializeW(process.value, argv[3], FALSE);
    log << L"SymInitialize=" << symbols << L'\n';
    Handle modules{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)};
    MODULEENTRY32W module{};
    module.dwSize = sizeof(module);
    if (Module32FirstW(modules.value, &module)) {
        do {
            const auto address = reinterpret_cast<DWORD64>(module.modBaseAddr);
            const DWORD64 loaded = SymLoadModuleExW(process.value, nullptr, module.szExePath,
                                                    nullptr, address, module.modBaseSize, nullptr, 0);
            log << L"module 0x" << std::hex << address << std::dec << L" size="
                << module.modBaseSize << L" symbols=" << loaded << L" " << module.szExePath << L'\n';
        } while (Module32NextW(modules.value, &module));
    }
    for (DWORD tid : threads) {
        log << L"\nTHREAD " << tid << L'\n';
        Handle thread{OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid)};
        CONTEXT registers{};
        registers.ContextFlags = CONTEXT_FULL;
        if (!thread.value || !GetThreadContext(thread.value, &registers)) {
            log << L"GetThreadContext error=" << GetLastError() << L'\n'; continue;
        }
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = registers.Rip;
        frame.AddrFrame.Offset = registers.Rbp;
        frame.AddrStack.Offset = registers.Rsp;
        frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
        for (int index = 0; index < 128 && frame.AddrPC.Offset; ++index) {
            alignas(SYMBOL_INFOW) unsigned char buffer[sizeof(SYMBOL_INFOW) + 4096 * sizeof(wchar_t)]{};
            auto* symbol = reinterpret_cast<SYMBOL_INFOW*>(buffer);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
            symbol->MaxNameLen = 4096;
            DWORD64 displacement = 0;
            IMAGEHLP_MODULEW64 owner{};
            owner.SizeOfStruct = sizeof(owner);
            SymGetModuleInfoW64(process.value, frame.AddrPC.Offset, &owner);
            log << index << L" 0x" << std::hex << frame.AddrPC.Offset << L" " << owner.ModuleName << L"!";
            if (SymFromAddrW(process.value, frame.AddrPC.Offset, &displacement, symbol)) {
                log << symbol->Name << L"+0x" << displacement;
            } else { log << L"<unresolved>"; }
            IMAGEHLP_LINEW64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD offset = 0;
            if (SymGetLineFromAddrW64(process.value, frame.AddrPC.Offset, &offset, &line)) {
                log << L" " << line.FileName << L":" << std::dec << line.LineNumber;
            }
            log << std::dec << L'\n';
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process.value, thread.value, &frame,
                             &registers, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
        }
        log.flush();
    }
    if (symbols) SymCleanup(process.value);
    log << L"Capture completed; target threads remain suspended until caller terminates this PID.\n";
    return dumped ? 0 : 4;
}
