#!/usr/bin/env python3
"""Run host-side protocol/input tests against declarations from the installed SDK."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[1]
SDK = Path(os.environ.get('DEVECO_SDK_HOME', '/Applications/DevEco-Studio.app/Contents/sdk'))
HEADERS = SDK / 'default/openharmony/native/sysroot/usr/include'


def run(args):
    subprocess.run([str(arg) for arg in args], check=True, cwd=ROOT)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--desktop', action='store_true', help='Also test the Slint desktop keyboard adapter')
parser.add_argument('--mobile', action='store_true', help='Test mobile controller models, lifecycle and shared Android protocol')
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix='crossdesk-harmony-test-') as temporary:
    work = Path(temporary)
    for directory in ['multimodalinput', 'info', 'multimedia', 'ohaudio', 'hilog', 'native_window', 'native_buffer', 'native_image']:
        (work / directory).symlink_to(HEADERS / directory, target_is_directory=True)
    common = ['c++', '-std=c++17', '-Wno-availability', '-Wno-unknown-attributes',
              '-I' + str(work), '-I' + str(ROOT / 'libs/wire/include'), '-I' + str(APP / 'native'),
              '-I' + str(ROOT / 'apps/desktop/src/platform/common/input')]
    run(common + [APP / 'tests/input_test.cpp', APP / 'native/input.cpp', '-o', work / 'input_test'])
    run([work / 'input_test'])
    packages = APP / '.native/packages'
    headers = list(packages.glob('n/nlohmann_json/*/*/include'))
    if not headers:
        raise SystemExit('Build the native dependencies first to obtain nlohmann_json.')
    yuv_headers = list(packages.glob('l/libyuv/*/*/include'))
    if not yuv_headers:
        raise SystemExit('Build the native dependencies first to obtain libyuv.')
    run([*common, '-I' + str(yuv_headers[0]), '-I' + str(ROOT / 'deps/submodules/minirtc/src/api'),
         APP / 'tests/capture_test.cpp', APP / 'native/capture.cpp', '-o', work / 'capture_test'])
    run([work / 'capture_test'])
    print('Capture authorization timing, frames/audio, privacy pause/resume and cleanup tests passed.')
    sources = list((ROOT / 'libs/wire/src').glob('*.cpp'))
    for name in ['wire_test', 'keyboard_state_wire_test']:
        run(common + ['-I' + str(headers[0]), *sources,
                      ROOT / 'libs/wire/tests' / (name + '.cpp'), '-o', work / name])
        run([work / name])
    if args.mobile:
        studio = Path(os.environ.get('DEVECO_STUDIO_HOME', '/Applications/DevEco-Studio.app/Contents'))
        for test in ['mobile_models_test.cjs', 'mobile_lifecycle_test.cjs', 'mobile_platform_test.cjs', 'mobile_store_test.cjs']:
            run([studio / 'tools/node/bin/node', APP / 'tests' / test,
                 studio / 'tools/ohpm/node_modules/typescript/lib/typescript.js',
                 APP / 'entry/src/main/ets/mobile'])
        android = ROOT / 'apps/android'
        for test in ['controller_protocol_test', 'video_pipeline_test']:
            run([*common, '-pthread', '-I' + str(headers[0]), '-I' + str(android / 'native'),
                 '-I' + str(ROOT / 'deps/submodules/minirtc/src/api'),
                 android / 'test/native' / (test + '.cpp'), ROOT / 'libs/wire/src/remote_action.cpp', '-o', work / test])
            run([work / test])
        run([*common, '-I' + str(ROOT / 'libs/controller/include'),
             APP / 'tests/mobile_file_transfer_test.cpp', ROOT / 'libs/wire/src/file_transfer.cpp',
             '-o', work / 'mobile_file_transfer_test'])
        run([work / 'mobile_file_transfer_test'])
    if args.desktop:
        studio = Path(os.environ.get('DEVECO_STUDIO_HOME', '/Applications/DevEco-Studio.app/Contents'))
        run([studio / 'tools/node/bin/node', APP / 'tests/text_input_host_test.cjs',
             studio / 'tools/ohpm/node_modules/typescript/lib/typescript.js',
             APP / 'desktop-port/TextInputHost.ets', APP / 'desktop-port/KeyboardHost.ets',
             APP / 'desktop-port/DesktopHost.ets', APP / 'desktop-port/DesktopAccess.ets'])
        run([studio / 'tools/node/bin/node', APP / 'tests/desktop_window_host_test.cjs',
             studio / 'tools/ohpm/node_modules/typescript/lib/typescript.js',
             APP / 'desktop-port/DesktopHost.ets'])
        run([studio / 'tools/node/bin/node', APP / 'tests/autostart_host_test.cjs',
             studio / 'tools/ohpm/node_modules/typescript/lib/typescript.js',
             APP / 'desktop-port/AutostartHost.ets'])
        cache = APP / '.native/slint-poc'
        run(['c++', '-std=c++20', '-I' + str(APP / 'desktop-port'),
             '-I' + str(cache / 'slint-1.17.1/api/cpp/include'), '-I' + str(cache / 'include'),
             APP / 'tests/desktop_keyboard_test.cpp', '-o', work / 'desktop_keyboard_test'])
        run([work / 'desktop_keyboard_test'])
        run(['c++', '-std=c++20', '-I' + str(APP / 'desktop-port'), '-I' + str(headers[0]),
             APP / 'tests/desktop_platform_test.cpp', '-o', work / 'desktop_platform_test'])
        run([work / 'desktop_platform_test'])
        desktop = ROOT / 'apps/desktop/src'
        includes = [APP / 'desktop-port', headers[0], desktop, desktop / 'screen_capturer',
                    desktop / 'speaker_capturer', desktop / 'device_controller',
                    desktop / 'common', desktop / 'gui', ROOT / 'deps/submodules/minirtc/src/api']
        run([*common, '-std=c++20', *['-I' + str(p) for p in includes],
             APP / 'tests/desktop_devices_test.cpp', APP / 'desktop-port/devices.cpp',
             APP / 'desktop-port/keyboard_hook.cpp', *sources,
             '-o', work / 'desktop_devices_test'])
        run([work / 'desktop_devices_test'])
        run([*common, '-std=c++20', *['-I' + str(p) for p in includes],
             APP / 'tests/keyboard_hook_test.cpp', APP / 'desktop-port/keyboard_hook.cpp',
             '-o', work / 'keyboard_hook_test'])
        run([work / 'keyboard_hook_test'])
        print('Slint displays, cursor shapes/position/wire/presentation, pointer cancellation, capture switching/rollback and authorization tests passed.')
    print('HarmonyOS input adapter and shared wire tests passed.')
