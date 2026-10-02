"""Create a local Notepad-- SDK; run in an x64 MSVC developer shell.

Only downloads pinned public headers and generates an import library from the
user's installed host DLL. Does not install or replace host binaries.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
from urllib.request import urlopen

REVISION = "91105f68b74382128f3313ac5af8accdc77de918"
BASE = f"https://gitee.com/cxasm/notepad--/raw/{REVISION}/src/qscint/src/Qsci"
HEADERS = {
    "qscicommand.h": "16d8e291df5006d15092a412145a01e2c9eb4b11b7de5490474d664ecf8ed64f",
    "qscidocument.h": "68c39d7fbeed76130a1ca1a99b699abadefc3a9cf9791a8ce4e18d6fd435d81d",
    "qsciglobal.h": "e35d271c9f38adb6dc3d564a98cca62b9699a4897f5b50b489278cdc25702f41",
    "qsciscintilla.h": "0fa9eed95831b94e3c3dd7977bbea27abd9c604591283c7aa0f2b33a289a67d4",
    "qsciscintillabase.h": "6e17d161886ac43be392277f97450783c44e9a399e2d171c2862bd5d647b3366",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", type=Path, required=True, help="Notepad-- 3.9.0 x64 directory")
    parser.add_argument("--out", type=Path, default=Path(".build-tools/ndd-sdk"))
    args = parser.parse_args()
    dumpbin = shutil.which("dumpbin.exe")
    librarian = shutil.which("lib.exe")
    if not dumpbin or not librarian:
        parser.error("Run from an x64 MSVC developer shell (dumpbin.exe and lib.exe required).")
    dll = args.host.resolve() / "qmyedit_qt5.dll"
    if not dll.is_file():
        parser.error(f"Missing host DLL: {dll}")
    # Read the PE machine field; fail before creating SDK files for a wrong host.
    binary = dll.read_bytes()
    offset = int.from_bytes(binary[0x3C:0x40], "little")
    if binary[offset:offset + 4] != b"PE\0\0" or binary[offset + 4:offset + 6] != b"\x64\x86":
        parser.error("Expected an x64 PE DLL.")
    result = subprocess.run([dumpbin, "/nologo", "/exports", str(dll)],
                            check=True, capture_output=True, text=True)
    symbols = re.findall(r"^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(\S+)", result.stdout, re.M)
    for required in ("SendScintillaPtrResult@QsciScintillaBase", "textChanged@QsciScintilla", "staticMetaObject@QsciScintilla"):
        if not any(required in symbol for symbol in symbols):
            parser.error(f"Host DLL does not export required interface: {required}")
    out = args.out.resolve()
    include = out / "include/Qsci"
    include.mkdir(parents=True, exist_ok=True)
    (out / "lib").mkdir(exist_ok=True)
    for name, expected in HEADERS.items():
        target = include / name
        if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == expected:
            continue
        with urlopen(f"{BASE}/{name}", timeout=30) as response:
            data = response.read().decode("utf-8-sig").replace("\r\n", "\n").encode("utf-8")
        if hashlib.sha256(data).hexdigest() != expected:
            raise RuntimeError(f"Header checksum mismatch: {name} (received {hashlib.sha256(data).hexdigest()})")
        target.write_bytes(data)
    definition = out / "qmyedit_qt5.def"
    definition.write_text("LIBRARY qmyedit_qt5.dll\nEXPORTS\n" + "\n".join(symbols) + "\n", encoding="ascii")
    subprocess.run([librarian, "/nologo", "/machine:x64", f"/def:{definition}",
                    f"/out:{out / 'lib/qmyedit_qt5.lib'}"], check=True)
    manifest = {"header_revision": REVISION, "header_sha256": HEADERS,
                "host_dll": str(dll), "host_dll_sha256": hashlib.sha256(binary).hexdigest()}
    (out / "provenance.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"SDK ready: {out}")


if __name__ == "__main__":
    main()
