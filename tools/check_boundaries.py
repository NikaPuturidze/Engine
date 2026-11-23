"""Check architectural include boundaries, including relative-path escape attempts."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'src'
# Protected modules may use the standard library, never an SDK via an obscure header.
STANDARD_HEADERS = set('''algorithm any array atomic barrier bit bitset cassert cctype
cerrno cfenv cfloat charconv chrono cinttypes ciso646 climits clocale cmath codecvt
compare complex concepts condition_variable coroutine csetjmp csignal cstdarg cstddef
cstdint cstdio cstdlib cstring ctgmath ctime cuchar cwchar cwctype deque exception
execution expected filesystem format forward_list fstream functional future generator
initializer_list iomanip ios iosfwd iostream istream iterator latch limits list locale
map mdspan memory memory_resource mutex new numbers numeric optional ostream print
queue random ranges ratio regex scoped_allocator semaphore set shared_mutex source_location
span spanstream sstream stack stacktrace stdexcept stdfloat stop_token streambuf string
string_view strstream syncstream system_error thread tuple type_traits typeindex typeinfo
unordered_map unordered_set utility valarray variant vector version'''.split())
errors = []
for path in sorted(SRC.rglob('*')):
    if path.suffix not in ('.h', '.cpp', '.in'):
        continue
    area = path.relative_to(SRC).parts[0]
    allowed = {'api': {'api'}, 'game': {'api', 'game'},
               'simulation': {'api', 'simulation'},
               'engine': {'api', 'simulation', 'engine'}}.get(area, set())
    for number, line in enumerate(path.read_text().splitlines(), 1):
        match = re.match(r'\s*#\s*include\s*["<]([^">]+)[">]', line)
        if not match:
            if re.match(r'\s*#\s*include\b', line):
                errors.append(f'{path.relative_to(ROOT)}:{number}: macro includes bypass dependency checks')
            continue
        include = match[1].replace('\\', '/')
        target = (SRC / include).resolve()
        if not target.exists():
            target = (path.parent / include).resolve()
        if target.is_relative_to(SRC.resolve()) and target.exists():
            dependency = target.relative_to(SRC.resolve()).parts[0]
            if dependency not in allowed:
                errors.append(f'{path.relative_to(ROOT)}:{number}: {area} cannot include {dependency}: {include}')
        elif area in {'api', 'game', 'simulation'} and include not in STANDARD_HEADERS:
            errors.append(f'{path.relative_to(ROOT)}:{number}: non-standard dependency: {include}')
        elif line.lstrip().startswith('#include "') and not target.exists() and include != 'cube_shader.h':
            errors.append(f'{path.relative_to(ROOT)}:{number}: unresolved local include: {include}')

if errors:
    print('\n'.join(errors), file=sys.stderr)
    sys.exit(1)
print('Architecture boundaries passed (game -> api; simulation -> api; engine -> simulation/api).')
