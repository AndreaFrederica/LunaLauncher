from pathlib import Path
import sys

inp = Path(sys.argv[1]).read_text().strip()
out = Path(sys.argv[2])
out.write_text(f'#pragma once\n#define GENERATED_VERSION "{inp}"\n')
