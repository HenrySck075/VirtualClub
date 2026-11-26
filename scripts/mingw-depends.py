# Modified from https://gitlab.kitware.com/cmake/cmake/-/work_items/20753#note_1022693

import argparse
import glob
import logging
import os
import os.path
from pathlib import Path
import re
import subprocess

logging.basicConfig(format='%(message)s')
log = logging.getLogger()

defprefix = '/usr/x86_64-w64-mingw32'
parser = argparse.ArgumentParser(description='Return list of DLL dependencies of a binary')
parser.add_argument('--objdump', metavar='PATH', default='/usr/bin/x86_64-w64-mingw32-objdump')
parser.add_argument('--prefix', metavar='PATH', default=defprefix)
parser.add_argument('--windir', metavar='PATH', default=os.getenv('HOME', '') + '/.wine/drive_c/windows')
parser.add_argument(
    '--pre-exclude-regex',
    metavar='REGEX',
    action='append',
    default=[],
    help='Exclude DLL by name before resolution (repeatable, like PRE_EXCLUDE_REGEXES)',
)
parser.add_argument(
    '--post-exclude-regex',
    metavar='REGEX',
    action='append',
    default=[],
    help='Exclude resolved DLL by path after resolution (repeatable, like POST_EXCLUDE_REGEXES)',
)
parser.add_argument(
    '--userlib',
    action='append',
    default=[],
    help='Add a user library path for lookup (repeatable)',
)
parser.add_argument('--debug', action='store_true', default=False)
parser.add_argument('--error', default=False, action='store_true')
parser.add_argument('exe', metavar='EXE')
args = parser.parse_args()

objdump = args.objdump
userpaths = ['.', args.prefix + '/bin']  # . is also for the binary itself
userpaths.extend(args.userlib)
userlibs = {}
visited = set()

pre_exclude = [re.compile(pat) for pat in args.pre_exclude_regex]
post_exclude = [re.compile(pat) for pat in args.post_exclude_regex]

if args.debug:
    log.setLevel(logging.DEBUG)

syslibs = {
    os.path.basename(p).lower(): str(p)
    for d in [args.windir + '/syswow64', args.windir + '/system32']
    for p in glob.glob(d + '/*.dll')
}


def matches_any(patterns, value):
    return any(p.search(value) for p in patterns)


def resolve_obj(obj, tryLowercase=True):
    if obj in syslibs:
        return None
    if tryLowercase and obj.lower() in syslibs:
        return None
    if obj.startswith('/'):
        return obj
    for p in userpaths:
        ap = p + '/' + obj
        if os.path.exists(ap):
            return ap
    if tryLowercase:
        return resolve_obj(obj.lower(), tryLowercase=False)
    if args.error:
        raise RuntimeError(f'Unable to resolve {obj}, aborting (--error).')
    log.warning(f'Unable to resolve {obj}, proceeding (--error not given).')
    syslibs[obj] = None
    return None


def add_userlibs(obj, lev=0):
    if lev > 0 and matches_any(pre_exclude, obj):
        log.debug((lev * '  ') + f'[PRE_EXCLUDE] {obj}')
        return

    pth = resolve_obj(obj)
    if pth is None:
        return

    norm_pth = os.path.normpath(pth)
    if norm_pth in visited:
        return
    visited.add(norm_pth)

    excluded_post = lev > 0 and matches_any(post_exclude, pth)
    if excluded_post:
        log.debug((lev * '  ') + f'[POST_EXCLUDE] {pth}')
    else:
        if lev > 0:
            userlibs[obj] = pth
        log.debug((lev * '  ') + pth)

    out = subprocess.check_output([objdump, '-p', pth]).decode('utf-8')
    deps = {
        m.group(1)
        for line in out.split('\n')
        if (m := re.match(r'\s*DLL Name: (.*)$', line))
    }
    for dep in deps:
        if (dep in syslibs) or (dep.lower() in syslibs):
            continue
        add_userlibs(dep, lev=lev + 1)


add_userlibs(args.exe)
log.info(','.join(sorted(userlibs.keys())))
log.info('SIZE %g MB\n' % (sum(Path(p).stat().st_size for p in userlibs.values()) / 2**20))
print(';'.join(sorted(userlibs.values())))
