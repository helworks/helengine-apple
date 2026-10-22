#!/usr/bin/env python3
"""Build the shared Apple native diagnostic in fixed platform outputs using the selected real Apple SDK."""
import argparse
import json
import os
import pathlib
import platform
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', required=True, choices=['macos', 'ios'])
parser.add_argument('--profile', default='Debug', choices=['Debug', 'Release', 'Profiler'])
parser.add_argument('--sdk', default='device', choices=['device', 'simulator'])
parser.add_argument('--diagnostic', action='store_true', help='Explicitly acknowledge this is not the generated game player')
arguments = parser.parse_args()
if platform.system() != 'Darwin':
    parser.error('Apple SDK builds must run from the actual git checkout on macOS')
if not arguments.diagnostic:
    parser.error('Use --diagnostic for native SDK/GPU checks; build the actual generated player through the project CLI or validate_core.py')
if arguments.profile == 'Profiler':
    parser.error('Profiler requires generated scope integration and an actual verified capture')
root = pathlib.Path(__file__).resolve().parent.parent
if not (root / '.git').exists():
    parser.error('Build source must be a real git checkout, never an exported archive')
if subprocess.check_output(['git', '-C', str(root), 'branch', '--show-current'], text=True).strip() != 'main':
    parser.error('This workspace is authorized for direct main builds')
lock = json.loads((root / 'toolchain.lock.json').read_text())
cmake = pathlib.Path.home() / 'dev/helworks/tools' / ('cmake-' + lock['cmake']['version'] + '-macos-universal') / 'CMake.app/Contents/bin/cmake'
if not cmake.is_file():
    parser.error('Install the pinned CMake archive listed in toolchain.lock.json before building: ' + str(cmake))
sdk = 'macosx' if arguments.target == 'macos' else ('iphoneos' if arguments.sdk == 'device' else 'iphonesimulator')
sdk_version = subprocess.check_output(['xcrun', '--sdk', sdk, '--show-sdk-version'], text=True).strip()
if sdk_version != '26.2':
    parser.error('Selected Apple SDK differs from the validated lock: ' + sdk_version)
output = pathlib.Path('/tmp/helengine') / arguments.target
previous = output / 'native-build-settings.json'
settings = {'sdk': sdk, 'profile': arguments.profile, 'architecture': 'arm64'}
if previous.is_file() and json.loads(previous.read_text()).get('sdk') != sdk:
    if output.resolve() not in [pathlib.Path('/private/tmp/helengine/ios'), pathlib.Path('/tmp/helengine/ios')]:
        raise RuntimeError('Refusing to clean unexpected output path')
    shutil.rmtree(output)
output.mkdir(parents=True, exist_ok=True)
configure = [str(cmake), '-S', str(root), '-B', str(output), '-DHELENGINE_DIAGNOSTIC_ONLY=ON', '-DHELENGINE_NATIVE_PROFILE=' + arguments.profile, '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_BUILD_TYPE=' + arguments.profile]
if arguments.target == 'ios':
    configure += ['-G', 'Xcode', '-DCMAKE_SYSTEM_NAME=iOS', '-DCMAKE_OSX_SYSROOT=' + sdk, '-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0', '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO']
else:
    configure += ['-DCMAKE_OSX_SYSROOT=macosx']
previous.write_text(json.dumps(settings, indent=2) + '\n')
with (output / 'configure.log').open('w') as log:
    result = subprocess.run(configure, stdout=log, stderr=subprocess.STDOUT)
if result.returncode:
    print((output / 'configure.log').read_text()[-4000:], file=sys.stderr)
    sys.exit(result.returncode)
build = [str(cmake), '--build', str(output), '--config', arguments.profile, '-j', '6']
if arguments.target == 'ios':
    build += ['--', '-quiet', 'CODE_SIGNING_ALLOWED=NO']
with (output / 'build.log').open('w') as log:
    result = subprocess.run(build, stdout=log, stderr=subprocess.STDOUT)
if result.returncode:
    print((output / 'build.log').read_text()[-4000:], file=sys.stderr)
    sys.exit(result.returncode)
test_output = 'Not executed: unsigned iOS compilation does not prove device or simulator execution.'
if arguments.target == 'macos':
    result = subprocess.run([str(cmake.parent / 'ctest'), '--test-dir', str(output), '--output-on-failure'], capture_output=True, text=True)
    test_output = result.stdout + result.stderr
    (output / 'tests.log').write_text(test_output)
    print(test_output)
    if result.returncode:
        sys.exit(result.returncode)
receipt = {'target': arguments.target, 'sdk': sdk, 'sdkVersion': sdk_version, 'profile': arguments.profile, 'architecture': 'arm64', 'commit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(), 'diagnosticOnly': True, 'unsigned': arguments.target == 'ios', 'tests': test_output}
(output / 'validation.json').write_text(json.dumps(receipt, indent=2) + '\n')
print('Native diagnostic build verified: ' + str(output))
