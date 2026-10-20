"""Run focused correctness checks with Clang against QMK's GCC build commands."""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

entries = json.loads(Path('compile_commands.json').read_text())
for entry in entries:
    args = entry.pop('arguments', None)
    if args is None:
        args = shlex.split(entry.pop('command'))
    # GCC ARM accepts this obsolete option; Clang ARM rejects it.
    entry['arguments'] = [arg for arg in args if arg != '-mno-thumb-interwork']
with tempfile.TemporaryDirectory(prefix='air96-clang-tidy-') as tmp:
    (Path(tmp) / 'compile_commands.json').write_text(json.dumps(entries))
    subprocess.run([
        'clang-tidy', '-p', tmp,
        '--checks=-*,clang-analyzer-core.*,bugprone-sizeof-expression,bugprone-suspicious-memset-usage',
        '--warnings-as-errors=clang-analyzer-*,bugprone-*',
        '--header-filter=keyboards/air96_v2/ansi/.*',
        *[str(p) for p in sorted(Path('keyboards/air96_v2/ansi').glob('*.c'))],
    ], check=True)
