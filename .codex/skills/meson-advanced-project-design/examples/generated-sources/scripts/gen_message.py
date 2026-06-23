from pathlib import Path
import sys

src = Path(sys.argv[1]).read_text().strip()
out = Path(sys.argv[2])
name = Path(sys.argv[1]).stem
out.write_text(
    '#pragma once\n'
    f'static inline const char* {name}_message() {{ return "{src}"; }}\n'
)
