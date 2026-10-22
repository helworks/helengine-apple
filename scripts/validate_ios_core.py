#!/usr/bin/env python3
"""Compile the actual generated portable Core for both unsigned Apple SDKs in one fixed output."""
import argparse
import hashlib
import json
import pathlib
import platform
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--core-root', required=True, type=pathlib.Path)
parser.add_argument('--code-root', required=True, type=pathlib.Path)
parser.add_argument('--content-root', required=True, type=pathlib.Path)
arguments = parser.parse_args()
if platform.system() != 'Darwin':
    parser.error('Unsigned SDK validation requires the real macOS host')
root = pathlib.Path(__file__).resolve().parent.parent
output = pathlib.Path('/tmp/helengine/ios')
cmake = pathlib.Path.home()/'dev/helworks/tools/cmake-3.31.12-macos-universal/CMake.app/Contents/bin/cmake'
protected = [arguments.core_root/'helengine_core_unity.cpp', arguments.content_root/'build-manifest.json']
before = {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest() for path in protected}
results = []
for sdk in ['iphoneos', 'iphonesimulator']:
    configure = [str(cmake), '--fresh', '-S', str(root), '-B', str(output), '-G', 'Xcode',
                 '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_SYSROOT='+sdk, '-DCMAKE_OSX_ARCHITECTURES=arm64',
                 '-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0', '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO',
                 '-DHELENGINE_DIAGNOSTIC_ONLY=OFF', '-DHELENGINE_NATIVE_PROFILE=Debug',
                 '-DHELENGINE_CORE_CPP_ROOT='+str(arguments.core_root.resolve()),
                 '-DHELENGINE_CODE_ROOT='+str(arguments.code_root.resolve()),
                 '-DHELENGINE_CONTENT_ROOT='+str(arguments.content_root.resolve())]
    build = [str(cmake), '--build', str(output), '--config', 'Debug', '--target', 'helengine_ios',
             '-j', '4', '--', '-quiet', 'CODE_SIGNING_ALLOWED=NO']
    for name, command in [('player-configure.log', configure), ('player-build.log', build)]:
        with (output/name).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(sdk+' failed: '+(output/name).read_text()[-6000:])
    binary = output/('Debug-'+sdk)/'helengine_ios.app/helengine_ios'
    description = subprocess.check_output(['file', str(binary)], text=True).strip()
    if 'arm64' not in description:
        raise RuntimeError('Actual iOS executable is not arm64: '+description)
    results.append({'sdk': sdk, 'sdkVersion': subprocess.check_output(['xcrun', '--sdk', sdk, '--show-sdk-version'], text=True).strip(),
                    'executable': str(binary), 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                    'description': description, 'unsignedCompilation': 'passed', 'executed': False})
    print(sdk+' actual generated Core unsigned link passed', flush=True)
after = {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest() for path in protected}
if before != after:
    raise RuntimeError('SDK cache refresh changed the canonical generated Core or package manifest')
receipt = {'platform': 'ios', 'profile': 'Debug', 'appleCommit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
           'diagnosticOnly': False, 'generatedCoreRoot': str(arguments.core_root.resolve()),
           'canonicalInputsPreserved': before, 'results': results,
           'limitation': 'Compilation only. No signing, deployment, simulator launch or runtime shader validation was performed.'}
(output/'core-validation.json').write_text(json.dumps(receipt, indent=2)+'\n')
print('Current unsigned Core receipt: '+str(output/'core-validation.json'))
