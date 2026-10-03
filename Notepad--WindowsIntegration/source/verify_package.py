"""Validate generated files without importing registry settings."""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
expected_exe = sys.argv[2] if len(sys.argv) > 2 else r'C:\Tools\Notepad--\Notepad--.exe'


def parse(name):
    data = (root / name).read_bytes()
    assert data.startswith(b'\xff\xfe'), 'REG files must be UTF-16LE with BOM'
    lines = data.decode('utf-16').splitlines()
    assert lines[0] == 'Windows Registry Editor Version 5.00'
    sections = {}
    current = None
    for line in lines[1:]:
        if not line or line.startswith(';'):
            continue
        if line.startswith('['):
            assert line.endswith(']'), line
            current = line[1:-1]
            assert current.lstrip('-').startswith('HKEY_CURRENT_USER\\'), current
            assert 'UserChoice' not in current, current
            assert current not in sections, current
            sections[current] = []
        else:
            assert current is not None, line
            assert re.fullmatch(r'(?:@|"(?:\\.|[^"\\])*")=(?:"(?:\\.|[^"\\])*"|hex\(0\):|-)', line), line
            sections[current].append(line)
    return sections


registered = parse('manual/Register-Notepad--.reg')
removed = parse('manual/Unregister-Notepad--.reg')
base = 'HKEY_CURRENT_USER\\Software\\Classes\\'
own = 'NotepadMinusMinusPortable.Text'
expected_delete = {
    '-' + base + own,
    '-HKEY_CURRENT_USER\\Software\\NotepadMinusMinusPortable',
    '-' + base + '*\\shell\\NotepadMinusMinusPortable.Edit',
}
assert {x for x in removed if x.startswith('-')} == expected_delete
extensions = {x[len(base):].split('\\')[0] for x in registered if x.startswith(base + '.')}
assert len(extensions) == 22, extensions
assert not extensions.intersection({'.exe', '.bat', '.cmd', '.ps1', '.reg', '.lnk', '.html'})
for ext in extensions:
    key = base + ext + '\\OpenWithProgids'
    assert registered[key] == ['"' + own + '"=hex(0):']
    assert removed[key] == ['"' + own + '"=-']
    assert base + ext not in registered, 'Do not overwrite extension default values'
for key in (base + own + '\\shell\\open\\command', base + '*\\shell\\NotepadMinusMinusPortable.Edit\\command'):
    line, = registered[key]
    value = re.sub(r'\\([\\"])', r'\1', line[3:-1])
    assert value == '"' + expected_exe + '" "%1"', value
assert len(registered['HKEY_CURRENT_USER\\Software\\NotepadMinusMinusPortable\\Capabilities\\FileAssociations']) == 22
for name in (
    '01-Set-Defaults.cmd', '02-Check-Defaults.cmd', '03-Restore-Defaults.cmd',
    'optional/01-Replace-Notepad.cmd', 'optional/02-Restore-Notepad.cmd',
    'manual/Open-DefaultApps.cmd', 'tools/AssociationBridge.exe',
    'tools/NotepadRedirect.exe', 'tools/BatchDefaults.ps1',
    'tools/NotepadReplacement.ps1', 'tools/Generate-RegistryFiles.ps1',
    'tools/Get-EditorPath.ps1', 'settings.example.json',
    'source/sfta/SFTA.ps1', 'README.md', 'docs/BATCH_DEFAULTS.md',
):
    assert (root / name).is_file(), name
for launcher in list(root.glob('*.cmd')) + list((root / 'optional').glob('*.cmd')):
    script, = re.findall(r'-File "%~dp0([^"]+)"', launcher.read_text(encoding='utf-8'))
    assert (launcher.parent / script).resolve().is_file(), (launcher, script)
assert {p.name for p in root.glob('*.cmd')} == {
    '01-Set-Defaults.cmd', '02-Check-Defaults.cmd', '03-Restore-Defaults.cmd',
}
print('Package checks passed: launcher paths, 22 candidate associations, quoted commands, scoped uninstall; no UserChoice writes.')
