#!/usr/bin/env python3
"""Build the Slint PC client (default), or the ArkUI phone/tablet client (--mobile)."""
import argparse
from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[1]
CACHE = APP / '.native'
PACKAGE_REVISION = '2eedd731d1e7e377bc0eea970b9f0c120ca5f00d'


def run(args, *, cwd=APP, env=None):
    subprocess.run([str(arg) for arg in args], cwd=cwd, env=env, check=True)


def environment():
    studio = Path(os.environ.get('DEVECO_STUDIO_HOME', '/Applications/DevEco-Studio.app/Contents'))
    sdk = Path(os.environ.get('DEVECO_SDK_HOME', studio / 'sdk'))
    native = sdk / 'default/openharmony/native'
    if not (native / 'llvm/bin/clang++').is_file():
        raise SystemExit('Set DEVECO_SDK_HOME to the DevEco SDK directory containing default/openharmony/native.')
    xmake = os.environ.get('XMAKE_BIN') or shutil.which('xmake')
    if not xmake:
        raise SystemExit('Install Xmake >= 3.1.1 or set XMAKE_BIN.')
    output = subprocess.check_output([xmake, '--version'], text=True)
    version = re.search(r'xmake v(\d+)\.(\d+)\.(\d+)', output)
    if not version or tuple(map(int, version.groups())) < (3, 1, 1):
        raise SystemExit('Xmake >= 3.1.1 is required.')
    env = os.environ.copy()
    env.update(NODE_HOME=str(studio / 'tools/node'), DEVECO_SDK_HOME=str(sdk),
               JAVA_HOME=str(studio / 'jbr/Contents/Home'),
               XMAKE_PKG_INSTALLDIR=str(CACHE / 'packages'),
               XMAKE_GLOBALDIR=str(CACHE / 'global'), NO_COLOR='1')
    env['PATH'] = os.pathsep.join([str(CACHE / 'venv/bin'), str(studio / 'tools/node/bin'),
                                  str(studio / 'tools/ohpm/bin'),
                                  str(native / 'build-tools/cmake/bin'), env.get('PATH', '')])
    return studio, native, xmake, env


def bootstrap(env):
    if sys.version_info < (3, 10):
        raise SystemExit('Use Python >= 3.10 (for example /opt/homebrew/bin/python3).')
    venv = CACHE / 'venv'
    if not (venv / 'bin/python').exists():
        run([sys.executable, '-m', 'venv', venv])
    if not (venv / '.crossdesk-tools-v1').exists():
        run([venv / 'bin/python', '-m', 'pip', 'install', 'setuptools==80.9.0',
             'wheel==0.45.1', 'meson==1.12.1'], env=env)
        (venv / '.crossdesk-tools-v1').touch()
    repository = CACHE / 'global/.xmake/repositories/xmake-repo'
    if not (repository / '.git').exists():
        repository.parent.mkdir(parents=True, exist_ok=True)
        seed = Path.home() / '.xmake/repositories/xmake-repo'
        source = seed if (seed / '.git').exists() else 'https://github.com/xmake-io/xmake-repo.git'
        run(['git', 'clone', '--no-hardlinks', source, repository], env=env)
    run(['git', 'remote', 'set-url', 'origin', 'https://github.com/xmake-io/xmake-repo.git'], cwd=repository, env=env)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repository, text=True).strip()
    if revision != PACKAGE_REVISION:
        run(['git', 'fetch', 'origin', PACKAGE_REVISION], cwd=repository, env=env)
        run(['git', 'checkout', '--detach', PACKAGE_REVISION], cwd=repository, env=env)
    # Mark this pinned checkout as ready so Xmake's first-run repository update
    # cannot replace it with a mirror or advance it beyond PACKAGE_REVISION.
    (repository / 'updated').touch()


def mobile_project():
    """Stage only the mobile source tree; never carry stale PC files forward."""
    project = CACHE / 'mobile-project'
    project.mkdir(exist_ok=True)
    for name in ['entry/src', 'entry/libs']:
        shutil.rmtree(project / name, ignore_errors=True)
    for name in ['AppScope', 'hvigor', 'entry/src/main/cpp', 'entry/src/main/ets/mobile']:
        shutil.copytree(APP / name, project / name, dirs_exist_ok=True)
    for name in ['element/color.json', 'media/mobile_brand.png']:
        target = project / 'entry/src/main/resources/base' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(APP / 'entry/src/main/resources/base' / name, target)
    pages = project / 'entry/src/main/ets/pages'
    pages.mkdir(parents=True)
    shutil.copy2(APP / 'entry/src/main/ets/pages/Mobile.ets', pages / 'Mobile.ets')
    shutil.copytree(CACHE / 'mobile-libs', project / 'entry/libs')
    (project / 'native').mkdir(exist_ok=True)
    (project / 'native/session.h').unlink(missing_ok=True)
    shutil.copy2(APP / 'native/mobile_session.h', project / 'native/mobile_session.h')
    for name in ['hvigorfile.ts', 'oh-package.json5', 'entry/hvigorfile.ts', 'entry/build-profile.json5', 'entry/oh-package.json5']:
        shutil.copy2(APP / name, project / name)
    profile = json.loads((APP / 'build-profile.json5').read_text())
    profile['app']['signingConfigs'] = []
    for product in profile['app']['products']:
        product.pop('signingConfig', None)
    (project / 'build-profile.json5').write_text(json.dumps(profile, indent=2)+'\n')
    for name in ['entry/src/main/module.json5', 'entry/src/main/resources/base/profile/main_pages.json']:
        target = project / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(APP / name, target)
    return project


def build_native(native, xmake, env, *, mobile=False):
    if not (ROOT / 'deps/submodules/minirtc/src/api/minirtc.h').exists():
        raise SystemExit('Run git submodule update --init --recursive first.')
    bootstrap(env)
    run([xmake, 'f', '-P', APP / 'native', '-y', '-p', 'harmony', '-a', 'arm64-v8a',
         '-m', 'release', '--toolchain=hdk', '--sdk=' + str(native),
         '--policies=package.precompiled:n', '-o', CACHE / 'build'], cwd=APP / 'native', env=env)
    # The PC client links MiniRTC directly; it has no separate session core.
    run([xmake, '-P', APP / 'native', '-y', 'crossdesk_harmony_mobile' if mobile else 'minirtc'],
        cwd=APP / 'native', env=env)


@contextmanager
def build_lock():
    CACHE.mkdir(parents=True, exist_ok=True)
    with (CACHE / 'build.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        yield


def main():
    parser = argparse.ArgumentParser(description=__doc__,
        epilog='PC options (including permissions/install): python3 apps/harmony/scripts/build_desktop.py --help')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--native-only', action='store_true')
    group.add_argument('--hap-only', action='store_true', help='Use an already built native library')
    parser.add_argument('--release', action='store_true')
    parser.add_argument('--mobile', action='store_true', help='Build a controller-only phone/tablet HAP without PC permissions or signing material')
    args, desktop_args = parser.parse_known_args()
    if not args.mobile and not args.native_only:
        run([sys.executable, APP / 'scripts/build_desktop.py', *sys.argv[1:]], cwd=ROOT)
        return
    if desktop_args:
        parser.error('unrecognized arguments: ' + ' '.join(desktop_args))
    with build_lock():
        studio, native, xmake, env = environment()
        if not args.hap_only:
            build_native(native, xmake, env, mobile=args.mobile)
        if not args.native_only:
            if not (CACHE / 'mobile-libs/arm64-v8a/libcrossdesk_harmony_core.so').is_file():
                raise SystemExit('Build the mobile core first: python3 apps/harmony/scripts/build.py --mobile --native-only')
            project = mobile_project()
            run([sys.executable, APP / 'scripts/generate_mobile_resources.py', '--output',
                 project / 'entry/src/main/resources/rawfile'], env=env)
            run([studio / 'tools/ohpm/bin/ohpm', 'install'], cwd=project, env=env)
            run([studio / 'tools/hvigor/bin/hvigorw', '--mode', 'module', '-p', 'product=default',
                 '-p', 'module=entry@default', '-p', 'buildMode=' + ('release' if args.release else 'debug'),
                 'assembleHap', '--no-daemon'], cwd=project, env=env)
            for hap in sorted((project / 'entry/build/default/outputs').rglob('*.hap')):
                run([sys.executable, APP / 'tests/verify_hap.py', hap, '--mobile'], env=env)


if __name__ == '__main__':
    main()
