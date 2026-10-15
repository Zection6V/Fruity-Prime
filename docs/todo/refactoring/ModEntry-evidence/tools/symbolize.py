"""Name the function (and line) at an RVA inside an executable, from its PDB.

    python -I symbolize.py <exe> <rva> [<rva> ...]
"""
import ctypes
import ctypes.wintypes as wt
import os
import sys

dbg = ctypes.WinDLL(os.environ.get("DBGHELP", os.path.join(
    r"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64", "dbghelp.dll")))


class SYMBOL_INFOW(ctypes.Structure):
    _fields_ = [("SizeOfStruct", wt.ULONG), ("TypeIndex", wt.ULONG), ("Reserved", ctypes.c_uint64 * 2),
                ("Index", wt.ULONG), ("Size", wt.ULONG), ("ModBase", ctypes.c_uint64), ("Flags", wt.ULONG),
                ("Value", ctypes.c_uint64), ("Address", ctypes.c_uint64), ("Register", wt.ULONG),
                ("Scope", wt.ULONG), ("Tag", wt.ULONG), ("NameLen", wt.ULONG), ("MaxNameLen", wt.ULONG),
                ("Name", ctypes.c_wchar * 2048)]


class IMAGEHLP_LINEW64(ctypes.Structure):
    _fields_ = [("SizeOfStruct", wt.DWORD), ("Key", ctypes.c_void_p), ("LineNumber", wt.DWORD),
                ("FileName", wt.LPWSTR), ("Address", ctypes.c_uint64)]


dbg.SymInitializeW.argtypes = [wt.HANDLE, wt.LPCWSTR, wt.BOOL]
dbg.SymLoadModuleExW.argtypes = [wt.HANDLE, wt.HANDLE, wt.LPCWSTR, wt.LPCWSTR, ctypes.c_uint64, wt.DWORD, ctypes.c_void_p, wt.DWORD]
dbg.SymLoadModuleExW.restype = ctypes.c_uint64
dbg.SymFromAddrW.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(SYMBOL_INFOW)]
dbg.SymGetLineFromAddrW64.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(wt.DWORD), ctypes.POINTER(IMAGEHLP_LINEW64)]

exe = os.path.abspath(sys.argv[1])
BASE = 0x140000000
handle = wt.HANDLE(0x4242)
dbg.SymSetOptions(0x2 | 0x10 | 0x80000000 >> 31)  # undecorate, lines
if not dbg.SymInitializeW(handle, os.path.dirname(exe), False):
    raise SystemExit("SymInitialize failed")
if not dbg.SymLoadModuleExW(handle, None, exe, None, BASE, 0, None, 0):
    raise SystemExit(f"SymLoadModuleEx failed: {ctypes.get_last_error()}")
for text in sys.argv[2:]:
    rva = int(text, 16)
    sym = SYMBOL_INFOW()
    sym.SizeOfStruct = 88
    sym.MaxNameLen = 2047
    disp = ctypes.c_uint64()
    name = "<no symbol>"
    if dbg.SymFromAddrW(handle, BASE + rva, ctypes.byref(disp), ctypes.byref(sym)):
        name = f"{sym.Name}+0x{disp.value:x}"
    line = IMAGEHLP_LINEW64()
    line.SizeOfStruct = ctypes.sizeof(IMAGEHLP_LINEW64)
    ld = wt.DWORD()
    where = ""
    if dbg.SymGetLineFromAddrW64(handle, BASE + rva, ctypes.byref(ld), ctypes.byref(line)):
        where = f"  {line.FileName}:{line.LineNumber}"
    print(f"0x{rva:x}  {name}{where}")
