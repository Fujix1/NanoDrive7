"""Build and run ND7 NDSIF tests on Windows with the installed MSVC tools.

No device connection is used. Override --compiler/--sdk-include if needed.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x64/cl.exe')
p.add_argument('--sdk-include', default='C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt')
a = p.parse_args()
out = root / '.pio/ndsif_native'
out.mkdir(parents=True, exist_ok=True)
# Compile the real VGM register mapping/reset methods with hardware declarations.
# Extraction avoids pulling unrelated GNU-only VGM parsing code into MSVC.
s = (root / 'src/vgm.cpp').read_text(encoding='utf-8')
parts = []
for signature in ['void VGM::resetSerialState(', 'void VGM::writeSerialRegister(']:
    start = s.index(signature)
    end = s.index('{', start) + 1
    depth = 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    parts.append(s[start:end])
(out / 'register_methods.inc').write_text('\n'.join(parts), encoding='utf-8')
compiler = Path(a.compiler)
cpp_include = compiler.parents[3] / 'include'
cmd = [str(compiler), '/nologo', '/std:c++14', '/Od', '/GS-', '/Zl', '/LD', '/DNDEBUG',
       '/D_ITERATOR_DEBUG_LEVEL=0', '/utf-8', '/I' + str(root / 'tests/ndsif/stubs'),
       '/I' + str(root / 'include'), '/I' + str(root / 'lib/ndsif'), '/I' + str(out),
       '/I' + str(cpp_include), '/I' + a.sdk_include,
       str(root / 'lib/ndsif/ndsif.cpp'), str(root / 'tests/ndsif/bridge.cpp'),
       '/Fo' + str(out) + '/', '/link', '/NOENTRY', '/NODEFAULTLIB',
       '/OUT:' + str(out / 'protocol.dll'), '/IMPLIB:' + str(out / 'protocol.lib')]
env = os.environ.copy()
env['PATH'] = str(compiler.parent) + os.pathsep + env['PATH']
subprocess.run(cmd, cwd=root, env=env, check=True)
subprocess.run([sys.executable, str(root / 'tests/ndsif/test_protocol.py'),
                str(out / 'protocol.dll'), '-v'], cwd=root, check=True)
