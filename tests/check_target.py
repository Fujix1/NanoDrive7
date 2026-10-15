"""Syntax-check changed production units using existing PlatformIO include metadata."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
data = json.loads((root / '.pio/build/esp32-s3-devkitc-1/idedata.json').read_text())
flags = [flag for flag in data['cxx_flags'] if flag not in ['-MMD', '-O3', '-Os']]
args = [data['cxx_path'], *flags, '-fsyntax-only', '-Wall']
args += ['-D' + define for define in data['defines']]
args += ['-I' + include for include in data['includes']['build']]
args += ['-I' + str(root / 'lib/ndsif')]
args += [str(root / name) for name in ['src/serialman.cpp', 'lib/ndsif/ndsif.cpp',
    'src/main.cpp', 'src/input.cpp', 'src/config.cpp', 'src/disp.cpp', 'src/vgm.cpp',
    'lib/SI5351/SI5351.cpp']]
subprocess.run(args, cwd=root, check=True)
print('Target syntax checks passed (production headers and dependencies).')
