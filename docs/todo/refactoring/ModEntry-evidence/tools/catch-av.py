"""Run a command under the Win32 debug API until it raises an access violation,
then print every thread's symbolized stack at that moment.

    python -I catch-av.py <exe> <cwd> <max runs> -- <args...>

No debugger needs installing: the system dbghelp.dll reads the build's own
PDBs (Release builds here carry them). Used to explain the exit-time
0xC0000005 seen on fast setup-after commands.
"""
import ctypes
import ctypes.wintypes as wt
import os
import sys

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
# The system dbghelp cannot read current MSVC PDBs; the Windows Kits one can.
DBGHELP = os.environ.get("DBGHELP", r"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\dbghelp.dll")
dbg = ctypes.WinDLL(DBGHELP, use_last_error=True)

DEBUG_ONLY_THIS_PROCESS = 0x2
INFINITE = 0xFFFFFFFF
DBG_CONTINUE = 0x00010002
DBG_EXCEPTION_NOT_HANDLED = 0x80010001
EXCEPTION_DEBUG_EVENT, CREATE_THREAD_DEBUG_EVENT, CREATE_PROCESS_DEBUG_EVENT = 1, 2, 3
EXIT_THREAD_DEBUG_EVENT, EXIT_PROCESS_DEBUG_EVENT, LOAD_DLL_DEBUG_EVENT = 4, 5, 6
ACCESS_VIOLATION = 0xC0000005
CONTEXT_FULL = 0x10000B


class EXCEPTION_RECORD(ctypes.Structure):
    _fields_ = [("ExceptionCode", wt.DWORD), ("ExceptionFlags", wt.DWORD), ("ExceptionRecord", ctypes.c_void_p),
                ("ExceptionAddress", ctypes.c_void_p), ("NumberParameters", wt.DWORD),
                ("ExceptionInformation", ctypes.c_uint64 * 15)]


class EXCEPTION_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("ExceptionRecord", EXCEPTION_RECORD), ("dwFirstChance", wt.DWORD)]


class CREATE_THREAD_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("hThread", wt.HANDLE), ("lpThreadLocalBase", ctypes.c_void_p), ("lpStartAddress", ctypes.c_void_p)]


class CREATE_PROCESS_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("hFile", wt.HANDLE), ("hProcess", wt.HANDLE), ("hThread", wt.HANDLE), ("lpBaseOfImage", ctypes.c_void_p),
                ("dwDebugInfoFileOffset", wt.DWORD), ("nDebugInfoSize", wt.DWORD), ("lpThreadLocalBase", ctypes.c_void_p),
                ("lpStartAddress", ctypes.c_void_p), ("lpImageName", ctypes.c_void_p), ("fUnicode", wt.WORD)]


class LOAD_DLL_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("hFile", wt.HANDLE), ("lpBaseOfDll", ctypes.c_void_p), ("dwDebugInfoFileOffset", wt.DWORD),
                ("nDebugInfoSize", wt.DWORD), ("lpImageName", ctypes.c_void_p), ("fUnicode", wt.WORD)]


class EXIT_PROCESS_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("dwExitCode", wt.DWORD)]


class U(ctypes.Union):
    _fields_ = [("Exception", EXCEPTION_DEBUG_INFO), ("CreateThread", CREATE_THREAD_DEBUG_INFO),
                ("CreateProcessInfo", CREATE_PROCESS_DEBUG_INFO), ("ExitProcess", EXIT_PROCESS_DEBUG_INFO),
                ("LoadDll", LOAD_DLL_DEBUG_INFO), ("pad", ctypes.c_byte * 176)]


class DEBUG_EVENT(ctypes.Structure):
    _fields_ = [("dwDebugEventCode", wt.DWORD), ("dwProcessId", wt.DWORD), ("dwThreadId", wt.DWORD), ("u", U)]


class STARTUPINFO(ctypes.Structure):
    _fields_ = [("cb", wt.DWORD), ("lpReserved", wt.LPWSTR), ("lpDesktop", wt.LPWSTR), ("lpTitle", wt.LPWSTR),
                ("dwX", wt.DWORD), ("dwY", wt.DWORD), ("dwXSize", wt.DWORD), ("dwYSize", wt.DWORD),
                ("dwXCountChars", wt.DWORD), ("dwYCountChars", wt.DWORD), ("dwFillAttribute", wt.DWORD),
                ("dwFlags", wt.DWORD), ("wShowWindow", wt.WORD), ("cbReserved2", wt.WORD),
                ("lpReserved2", ctypes.c_void_p), ("hStdInput", wt.HANDLE), ("hStdOutput", wt.HANDLE),
                ("hStdError", wt.HANDLE)]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [("hProcess", wt.HANDLE), ("hThread", wt.HANDLE), ("dwProcessId", wt.DWORD), ("dwThreadId", wt.DWORD)]


class ADDRESS64(ctypes.Structure):
    _fields_ = [("Offset", ctypes.c_uint64), ("Segment", wt.WORD), ("Mode", ctypes.c_int)]


class STACKFRAME64(ctypes.Structure):
    _fields_ = [("AddrPC", ADDRESS64), ("AddrReturn", ADDRESS64), ("AddrFrame", ADDRESS64), ("AddrStack", ADDRESS64),
                ("AddrBStore", ADDRESS64), ("FuncTableEntry", ctypes.c_void_p), ("Params", ctypes.c_uint64 * 4),
                ("Far", wt.BOOL), ("Virtual", wt.BOOL), ("Reserved", ctypes.c_uint64 * 3), ("KdHelp", ctypes.c_byte * 512)]


class SYMBOL_INFOW(ctypes.Structure):
    _fields_ = [("SizeOfStruct", wt.ULONG), ("TypeIndex", wt.ULONG), ("Reserved", ctypes.c_uint64 * 2),
                ("Index", wt.ULONG), ("Size", wt.ULONG), ("ModBase", ctypes.c_uint64), ("Flags", wt.ULONG),
                ("Value", ctypes.c_uint64), ("Address", ctypes.c_uint64), ("Register", wt.ULONG),
                ("Scope", wt.ULONG), ("Tag", wt.ULONG), ("NameLen", wt.ULONG), ("MaxNameLen", wt.ULONG),
                ("Name", ctypes.c_wchar * 1024)]


class IMAGEHLP_LINEW64(ctypes.Structure):
    _fields_ = [("SizeOfStruct", wt.DWORD), ("Key", ctypes.c_void_p), ("LineNumber", wt.DWORD),
                ("FileName", wt.LPWSTR), ("Address", ctypes.c_uint64)]


k32.WaitForDebugEvent.argtypes = [ctypes.POINTER(DEBUG_EVENT), wt.DWORD]
k32.ContinueDebugEvent.argtypes = [wt.DWORD, wt.DWORD, wt.DWORD]
k32.GetThreadContext.argtypes = [wt.HANDLE, ctypes.c_void_p]
dbg.SymInitializeW.argtypes = [wt.HANDLE, wt.LPCWSTR, wt.BOOL]
dbg.SymLoadModuleExW.argtypes = [wt.HANDLE, wt.HANDLE, wt.LPCWSTR, wt.LPCWSTR, ctypes.c_uint64, wt.DWORD, ctypes.c_void_p, wt.DWORD]
dbg.SymLoadModuleExW.restype = ctypes.c_uint64
dbg.SymFromAddrW.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(SYMBOL_INFOW)]
dbg.SymGetLineFromAddrW64.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(wt.DWORD), ctypes.POINTER(IMAGEHLP_LINEW64)]
dbg.SymGetModuleBase64.argtypes = [wt.HANDLE, ctypes.c_uint64]
dbg.SymGetModuleBase64.restype = ctypes.c_uint64
dbg.SymFunctionTableAccess64.argtypes = [wt.HANDLE, ctypes.c_uint64]
dbg.SymFunctionTableAccess64.restype = ctypes.c_void_p
dbg.StackWalk64.argtypes = [wt.DWORD, wt.HANDLE, wt.HANDLE, ctypes.POINTER(STACKFRAME64), ctypes.c_void_p,
                            ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]


k32.GetFinalPathNameByHandleW.argtypes = [wt.HANDLE, wt.LPWSTR, wt.DWORD, wt.DWORD]


def path_of(handle):
    buf = ctypes.create_unicode_buffer(1024)
    n = k32.GetFinalPathNameByHandleW(handle, buf, 1024, 0)
    path = buf.value if n else None
    return path[4:] if path and path.startswith("\\\\?\\") else path


EXE_BASE = [0]


def describe(hproc, address):
    sym = SYMBOL_INFOW()
    sym.SizeOfStruct = 88  # sizeof(SYMBOL_INFOW) on x64, with its one-character Name
    sym.MaxNameLen = 1023
    disp = ctypes.c_uint64()
    name = f"0x{address:x}"
    if EXE_BASE[0] and 0 <= address - EXE_BASE[0] < 0x10000000:
        name += f" (exe+0x{address - EXE_BASE[0]:x})"
    if dbg.SymFromAddrW(hproc, address, ctypes.byref(disp), ctypes.byref(sym)):
        name = f"{sym.Name}+0x{disp.value:x}"
    line = IMAGEHLP_LINEW64()
    line.SizeOfStruct = ctypes.sizeof(IMAGEHLP_LINEW64)
    ld = wt.DWORD()
    if dbg.SymGetLineFromAddrW64(hproc, address, ctypes.byref(ld), ctypes.byref(line)):
        name += f"  [{os.path.basename(line.FileName)}:{line.LineNumber}]"
    return name


def stack(hproc, hthread, limit=40):
    raw = (ctypes.c_byte * (1232 + 16))()
    base = (ctypes.addressof(raw) + 15) & ~15
    ctypes.c_uint32.from_address(base + 0x30).value = CONTEXT_FULL
    if not k32.GetThreadContext(hthread, ctypes.c_void_p(base)):
        return ["<no context>"]
    frame = STACKFRAME64()
    frame.AddrPC.Offset = ctypes.c_uint64.from_address(base + 0xF8).value
    frame.AddrFrame.Offset = ctypes.c_uint64.from_address(base + 0xA0).value
    frame.AddrStack.Offset = ctypes.c_uint64.from_address(base + 0x98).value
    for a in (frame.AddrPC, frame.AddrFrame, frame.AddrStack):
        a.Mode = 3
    lines = []
    fta = ctypes.cast(dbg.SymFunctionTableAccess64, ctypes.c_void_p)
    gmb = ctypes.cast(dbg.SymGetModuleBase64, ctypes.c_void_p)
    for _ in range(limit):
        if not dbg.StackWalk64(0x8664, hproc, hthread, ctypes.byref(frame), ctypes.c_void_p(base), None, fta, gmb, None):
            break
        if frame.AddrPC.Offset == 0:
            break
        lines.append(describe(hproc, frame.AddrPC.Offset))
    return lines


k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]


def scan(hproc, hthread, size=0x6000):
    raw = (ctypes.c_byte * (1232 + 16))()
    base = (ctypes.addressof(raw) + 15) & ~15
    ctypes.c_uint32.from_address(base + 0x30).value = CONTEXT_FULL
    if not k32.GetThreadContext(hthread, ctypes.c_void_p(base)):
        return ["<no context>"]
    rsp = ctypes.c_uint64.from_address(base + 0x98).value
    buf = (ctypes.c_uint64 * (size // 8))()
    got = ctypes.c_size_t()
    k32.ReadProcessMemory(hproc, ctypes.c_void_p(rsp), buf, size, ctypes.byref(got))
    out = []
    for i in range(got.value // 8):
        value = buf[i]
        if EXE_BASE[0] and 0x1000 <= value - EXE_BASE[0] < 0x4000000:
            out.append(f"rsp+0x{i * 8:x}: exe+0x{value - EXE_BASE[0]:x}")
    return out


def run_once(exe, cwd, args):
    si = STARTUPINFO()
    si.cb = ctypes.sizeof(si)
    pi = PROCESS_INFORMATION()
    cmd = ctypes.create_unicode_buffer(" ".join(f'"{a}"' if " " in a else a for a in [exe, *args]))
    if not k32.CreateProcessW(None, cmd, None, None, False, DEBUG_ONLY_THIS_PROCESS, None, cwd, ctypes.byref(si), ctypes.byref(pi)):
        raise OSError(ctypes.get_last_error())
    threads, hproc, report = {}, None, None
    event = DEBUG_EVENT()
    while True:
        k32.WaitForDebugEvent(ctypes.byref(event), INFINITE)
        status = DBG_CONTINUE
        code = event.dwDebugEventCode
        if code == CREATE_PROCESS_DEBUG_EVENT:
            info = event.u.CreateProcessInfo
            hproc = info.hProcess
            EXE_BASE[0] = info.lpBaseOfImage or 0
            threads[event.dwThreadId] = info.hThread
            dbg.SymSetOptions(0x2 | 0x4 | 0x10)  # undecorate, deferred loads, line numbers
            dbg.SymInitializeW(hproc, os.path.dirname(exe), False)
            dbg.SymLoadModuleExW(hproc, info.hFile, path_of(info.hFile) or exe, None, info.lpBaseOfImage or 0, 0, None, 0)
        elif code == CREATE_THREAD_DEBUG_EVENT:
            threads[event.dwThreadId] = event.u.CreateThread.hThread
        elif code == EXIT_THREAD_DEBUG_EVENT:
            threads.pop(event.dwThreadId, None)
        elif code == LOAD_DLL_DEBUG_EVENT:
            info = event.u.LoadDll
            dbg.SymLoadModuleExW(hproc, info.hFile, path_of(info.hFile), None, info.lpBaseOfDll or 0, 0, None, 0)
        elif code == EXCEPTION_DEBUG_EVENT:
            rec = event.u.Exception.ExceptionRecord
            ex_code = rec.ExceptionCode & 0xFFFFFFFF
            if ex_code == ACCESS_VIOLATION and event.u.Exception.dwFirstChance and report is None:
                kind = {0: "read", 1: "write", 8: "execute"}.get(rec.ExceptionInformation[0], "?")
                report = [f"ACCESS VIOLATION ({kind} of 0x{rec.ExceptionInformation[1]:x}) at "
                          f"{describe(hproc, rec.ExceptionAddress or 0)} on thread {event.dwThreadId}",
                          f"live threads: {len(threads)}"]
                report.append(f"--- faulting thread {event.dwThreadId}")
                report += ["  " + l for l in stack(hproc, threads[event.dwThreadId])]
                report.append("  raw stack scan (candidate return addresses into the exe, innermost first):")
                report += ["    " + l for l in scan(hproc, threads[event.dwThreadId])]
                for tid, handle in threads.items():
                    if tid != event.dwThreadId:
                        report.append(f"--- thread {tid}")
                        report += ["  " + l for l in stack(hproc, handle, 12)]
            if ex_code not in (0x80000003, 0x4000001F):
                status = DBG_EXCEPTION_NOT_HANDLED
        elif code == EXIT_PROCESS_DEBUG_EVENT:
            exit_code = event.u.ExitProcess.dwExitCode
            k32.ContinueDebugEvent(event.dwProcessId, event.dwThreadId, status)
            return exit_code, report
        k32.ContinueDebugEvent(event.dwProcessId, event.dwThreadId, status)


def main():
    exe, cwd, runs = sys.argv[1], sys.argv[2], int(sys.argv[3])
    args = sys.argv[sys.argv.index("--") + 1:]
    for i in range(1, runs + 1):
        code, report = run_once(exe, cwd, args)
        print(f"run {i}: exit {code & 0xFFFFFFFF:#x}")
        if report:
            print("\n".join(report))
            return 0
    print("no access violation caught")
    return 1


if __name__ == "__main__":
    sys.exit(main())
