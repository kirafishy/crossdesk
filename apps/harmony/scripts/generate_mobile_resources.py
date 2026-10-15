#!/usr/bin/env python3
"""Package source-backed, offline mobile legal and build documents."""
import hashlib
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import shutil

APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[1]
OUTPUT = APP / 'entry/src/main/resources/rawfile'


def git(*args, cwd=ROOT):
    return subprocess.check_output(['git', *args], cwd=cwd, text=True).strip()


def main(output=OUTPUT):
    spec = importlib.util.spec_from_file_location('android_notices', ROOT / 'apps/android/scripts/generate_notices.py')
    android = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(android)
    catalog = json.loads((ROOT / 'apps/ios/CrossDeskMobile/Resources/ThirdPartyLicenses.json').read_text())
    components = [dict(c) for c in catalog['components'] if c['id'] in android.COMPONENTS | {'crossdesk'}]
    additions = json.loads((ROOT / 'apps/android/licenses/additional-notices.json').read_text())
    components += [c for c in additions if c['id'] != 'android-ndk']
    native = Path(os.environ.get('DEVECO_SDK_HOME', '/Applications/DevEco-Studio.app/Contents/sdk')) / 'default/openharmony/native'
    components.append({'id': 'ohos-llvm', 'name': 'HarmonyOS LLVM runtime', 'license': 'Apache-2.0 WITH LLVM-exception',
        'version': 'SDK bundled', 'sourceURL': 'https://gitee.com/openharmony/third_party_llvm-project',
        'documents': [{'title': 'LLVM NOTICE', 'text': (native / 'llvm/NOTICE').read_text()}]})
    revision = git('rev-parse', 'HEAD')
    mini = git('rev-parse', 'HEAD', cwd=ROOT / 'deps/submodules/minirtc')
    source = f'https://github.com/kunkundi/crossdesk/tree/{revision}'
    notice = ('CrossDesk HarmonyOS is GPL-3.0-only; MiniRTC is LGPL-3.0-only. Other components retain their licenses.\n'
              'You may modify, rebuild and relink these libraries, then install a HAP signed with your own HarmonyOS key.\n'
              'Open-source licenses govern use, modification and redistribution; their warranty limitations apply.\n')
    output.mkdir(parents=True, exist_ok=True)
    # The catalog points to individual, hash-checked documents. A second full
    # concatenation was unused and duplicated every bundled license.
    (output / 'THIRD_PARTY_NOTICES.txt').unlink(missing_ok=True)
    shutil.rmtree(output / 'about', ignore_errors=True)
    for filename in ['LICENSE', 'PRIVACY.md']:
        (output / filename).write_text((ROOT / filename).read_text())
    (output / 'OPEN_SOURCE_NOTICE.txt').write_text(notice)
    for component in components:
        if component['id'] == 'crossdesk':
            component['sourceURL'] = source
            component['documents'] = [{'title': 'LICENSE', 'text': (ROOT / 'LICENSE').read_text()}, {'title': 'Open-source rights', 'text': notice}]
        if component['id'] in {'minirtc', 'inih', 'webrtc'}:
            component.update(version=mini, sourceURL=f'https://github.com/kunkundi/minirtc/tree/{mini}')
        docs = []
        for index, doc in enumerate(component['documents']):
            asset = f"about/licenses/{component['id']}/{index}.txt"
            target = output / asset
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(doc['text'])
            docs.append({'title': doc['title'], 'asset': asset, 'sha256': hashlib.sha256(doc['text'].encode()).hexdigest()})
        component['documents'] = docs
    (output / 'ThirdPartyLicenses.json').write_text(json.dumps({'schemaVersion': 1, 'components': components}, ensure_ascii=False, indent=2)+'\n')
    documents = []
    for i, (title, path) in enumerate([
        ('鸿蒙构建说明', APP / 'README.md'), ('移动端功能对照', APP / 'MOBILE_PARITY.md'),
        ('原生构建配方', APP / 'native/xmake.lua'), ('构建脚本', APP / 'scripts/build.py'),
        ('MiniRTC 构建配方', ROOT / 'deps/submodules/minirtc/xmake.lua'),
        ('OpenSSL 构建配方', ROOT / 'deps/submodules/minirtc/thirdparty/openssl/xmake.lua')]):
        if not path.exists(): continue
        asset = f'about/build/{i}.txt'; target = output / asset; target.parent.mkdir(parents=True, exist_ok=True); target.write_text(path.read_text())
        documents.append({'title': title, 'asset': asset})
    (output / 'SourceMetadata.json').write_text(json.dumps({'revision': revision, 'miniRTCRevision': mini, 'sourceURL': source,
        'isModified': bool(git('status', '--porcelain', '--untracked-files=normal')), 'buildDocuments': documents}, ensure_ascii=False, indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUTPUT)
    main(parser.parse_args().output)
