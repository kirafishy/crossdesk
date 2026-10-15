#!/usr/bin/env python3
"""Check the packaged app, rather than only the files in the source project."""
import argparse
import json
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile
import zipfile

APP = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('hap', nargs='?', type=Path)
parser.add_argument('--mobile', action='store_true')
parser.add_argument('--bundle-name', default=json.loads((APP / 'AppScope/app.json5').read_text())['app']['bundleName'])
parser.add_argument('--desktop-permissions', action='store_true')
parser.add_argument('--host-permissions', action='store_true')
parser.add_argument('--cursor-position-permission', action='store_true')
args = parser.parse_args()
if args.hap is None:
    project = 'mobile-project' if args.mobile else 'slint-poc/desktop-project'
    args.hap = APP / '.native' / project / 'entry/build/default/outputs/default/entry-default-unsigned.hap'
sdk = Path(os.environ.get('DEVECO_SDK_HOME', '/Applications/DevEco-Studio.app/Contents/sdk'))
native = sdk / 'default/openharmony/native'
readelf = native / 'llvm/bin/llvm-readelf'
system_libraries = native / 'sysroot/usr/lib/aarch64-linux-ohos'


def require(condition, message):
    if not condition:
        raise SystemExit(message)


with zipfile.ZipFile(args.hap) as hap, tempfile.TemporaryDirectory(prefix='crossdesk-hap-') as temporary:
    require(hap.testzip() is None, 'Corrupt HAP archive')
    manifest = json.loads(hap.read('module.json'))
    expected_app = (json.loads((APP / 'AppScope/app.json5').read_text())['app']['bundleName']
                    if args.mobile else args.bundle_name)
    require(manifest['app']['bundleName'] == expected_app, 'Unexpected bundle name')
    require(set(manifest['module']['deviceTypes']) == ({'phone', 'tablet'} if args.mobile else {'2in1'}), 'Unexpected device types')
    permissions = {item['name'] for item in manifest['module'].get('requestPermissions', [])}
    if args.mobile:
        require(permissions == {'ohos.permission.INTERNET', 'ohos.permission.KEEP_BACKGROUND_RUNNING'}, 'Mobile must not declare screen capture or input-injection permissions')
        require(all('dataTransfer' in a.get('backgroundModes', []) for a in manifest['module']['abilities']), 'Missing mobile background mode')
    else:
        expected_permissions = {'ohos.permission.INTERNET', 'ohos.permission.FILE_ACCESS_PERSIST',
                                'ohos.permission.PREPARE_APP_TERMINATE'}
        for enabled, names in [
            (args.desktop_permissions, ['ohos.permission.HOOK_KEY_EVENT', 'ohos.permission.READ_PASTEBOARD']),
            (args.host_permissions, ['ohos.permission.CUSTOM_SCREEN_RECORDING', 'ohos.permission.CONTROL_DEVICE']),
            (args.cursor_position_permission, ['ohos.permission.INPUT_MONITORING'])]:
            if enabled:
                expected_permissions.update(names)
        require(permissions == expected_permissions, 'Desktop permissions do not match the selected build options')
        require(all(not a.get('backgroundModes') for a in manifest['module']['abilities']),
                'Desktop must not inherit mobile background modes')
    arkts = hap.read('ets/modules.abc')
    require(len(arkts) > 0, 'Missing compiled ArkTS')
    if args.mobile:
        require(b'pages/Mobile' in arkts and b'pages/Index' not in arkts and b'NativeSession' not in arkts,
                'Mobile package must contain only the mobile entry and controller')
        prefix = 'resources/rawfile/'
        require(prefix + 'THIRD_PARTY_NOTICES.txt' not in hap.namelist(), 'Redundant aggregate license file')
        for name in ['PRIVACY.md', 'LICENSE', 'OPEN_SOURCE_NOTICE.txt', 'SourceMetadata.json']:
            require(len(hap.read(prefix + name)) > 0, 'Missing mobile document: ' + name)
        catalog = json.loads(hap.read(prefix + 'ThirdPartyLicenses.json'))
        require({'minirtc', 'openh264', 'dav1d', 'openfec', 'ohos-llvm'} <= {c['id'] for c in catalog['components']}, 'Incomplete legal catalog')
        for component in catalog['components']:
            for document in component['documents']:
                require(hashlib.sha256(hap.read(prefix + document['asset'])).hexdigest() == document['sha256'], 'License content mismatch')
        metadata = json.loads(hap.read(prefix + 'SourceMetadata.json'))
        for document in metadata['buildDocuments']:
            require(len(hap.read(prefix + document['asset'])) > 0, 'Missing offline build document')
    else:
        require(b'pages/Index' in arkts and b'pages/Mobile' not in arkts and b'NativeSession' not in arkts,
                'Desktop must contain only the shared Slint entry')
    require('resources/rawfile/crossdesk-icons.ttf' not in hap.namelist(),
            'Unused ArkUI desktop icon font must not be packaged')
    libraries = [name for name in hap.namelist() if name.endswith('.so')]
    require(all(name.startswith('libs/arm64-v8a/') for name in libraries), 'Unexpected library ABI')
    packaged = {Path(name).name for name in libraries}
    expected_libraries = ({'libcrossdesk.so', 'libcrossdesk_harmony_core.so', 'libc++_shared.so'} if args.mobile
                          else {'libslint_desktop.so', 'libslint_cpp.so', 'libc++_shared.so'})
    require(packaged == expected_libraries, 'Missing native library or stale library from a retired entry')
    for name in libraries:
        data = hap.read(name)
        require(data[:6] == b'\x7fELF\x02\x01', f'{name}: expected ELF64 little endian')
        require(int.from_bytes(data[18:20], 'little') == 183, f'{name}: expected AArch64')
        require(int.from_bytes(data[16:18], 'little') == 3, f'{name}: expected shared object')
        path = Path(temporary) / Path(name).name
        path.write_bytes(data)
        dynamic = subprocess.check_output([readelf, '-d', path], text=True)
        for dependency in re.findall(r'\(NEEDED\).*?\[(.*?)\]', dynamic):
            # An absolute dependency could exist on this build machine and
            # still be impossible to resolve inside the device's app sandbox.
            require(dependency and '/' not in dependency and '\\' not in dependency,
                    f'{name}: runtime dependency must be a library name, not a path: {dependency}')
            require(dependency in packaged or (system_libraries / dependency).is_file(),
                    f'{name}: dependency is neither packaged nor in the HarmonyOS SDK: {dependency}')
            if args.mobile:
                require(dependency not in {'libnative_avscreen_capture.so', 'libohinput.so', 'libnative_image.so'},
                        f'{name}: unexpected PC capture/input dependency: {dependency}')
        if args.mobile and path.name == 'libcrossdesk_harmony_core.so':
            symbols = subprocess.check_output([readelf, '--dyn-syms', '--demangle', '--wide', path], text=True)
            for api in ['Command(', 'Poll(', 'Shutdown(', 'Capture::', 'Input::']:
                require('crossdesk::harmony::' + api not in symbols, 'Mobile core exports PC API: ' + api)
            for api in ['MobileCommand(', 'MobilePoll(', 'MobileShutdown(']:
                require('crossdesk::harmony::' + api in symbols, 'Missing mobile bridge API: ' + api)
        if not args.mobile and path.name == 'libslint_desktop.so':
            symbols = subprocess.check_output([readelf, '--dyn-syms', '--demangle', '--wide', path], text=True)
            for api in ['Renderer::', 'MobileCommand(', 'MobilePoll(']:
                require('crossdesk::harmony::' + api not in symbols, 'Desktop bundles unused mobile API: ' + api)
        sonames = re.findall(r'\(SONAME\).*?\[(.*?)\]', dynamic)
        require(sonames == [path.name], f'{name}: expected SONAME {path.name}, got {sonames}')
        for search_path in re.findall(r'\((?:RUNPATH|RPATH)\).*?\[(.*?)\]', dynamic):
            require(all(not item.startswith('/') for item in search_path.split(':')),
                    f'{name}: absolute runtime search path: {search_path}')
    print(f'HAP verified: {args.hap}')
    print(f'{"phone / tablet controller only" if args.mobile else "2in1 shared Slint desktop"}, compiled ArkTS, {len(libraries)} AArch64 libraries, resolved dependencies')
