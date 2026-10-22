#!/usr/bin/env python3
"""Build and run actual generated Core inputs without opening a window; overwrite one current receipt."""
import argparse
import json
import pathlib
import platform
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--core-root', required=True, type=pathlib.Path)
parser.add_argument('--code-root', required=True, type=pathlib.Path)
parser.add_argument('--content-root', required=True, type=pathlib.Path)
parser.add_argument('--profile', choices=['Debug', 'Release', 'Profiler'], default='Debug')
parser.add_argument('--demodisc', action='store_true', help='Verify all 44 authored menu items and actual SDL input replay over 300 frames')
arguments = parser.parse_args()
if platform.system() != 'Darwin':
    parser.error('Core/Metal validation requires the real macOS host')
root = pathlib.Path(__file__).resolve().parent.parent
if not (root / '.git').is_dir():
    parser.error('Validation requires the actual git checkout')
output = pathlib.Path('/tmp/helengine/macos')
output.mkdir(parents=True, exist_ok=True)
cmake = pathlib.Path.home() / 'dev/helworks/tools/cmake-3.31.12-macos-universal/CMake.app/Contents/bin/cmake'
commands = [
    ('player-configure.log', [str(cmake), '-S', str(root), '-B', str(output), '-DHELENGINE_DIAGNOSTIC_ONLY=OFF', '-DHELENGINE_NATIVE_PROFILE='+arguments.profile, '-DCMAKE_BUILD_TYPE='+arguments.profile, '-DHELENGINE_CORE_CPP_ROOT='+str(arguments.core_root.resolve()), '-DHELENGINE_CODE_ROOT='+str(arguments.code_root.resolve()), '-DHELENGINE_CONTENT_ROOT='+str(arguments.content_root.resolve())]),
    ('player-build.log', [str(cmake), '--build', str(output), '--target', 'helengine_macos'] + (['demo_disc_menu_tests'] if arguments.demodisc else []) + ['-j', '6']),
]
for log_name, command in commands:
    with (output / log_name).open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((output / log_name).read_text()[-4000:], file=sys.stderr)
        sys.exit(result.returncode)
binary = output / ('demo_disc_menu_tests' if arguments.demodisc else 'helengine_macos.app/Contents/MacOS/helengine_macos')
command = [str(binary), '--offscreen', '--frames', '300', '--content', str(arguments.content_root.resolve())]
result = subprocess.run(command, capture_output=True, text=True)
transcript = result.stdout + result.stderr
(output / 'player-run.log').write_text(transcript)
print(transcript, end='')
if result.returncode:
    sys.exit(result.returncode)
player = output / 'helengine_macos.app/Contents/MacOS/helengine_macos'
invalid = subprocess.run([str(player), '--offscreen', '--frames', '1', '--content', str(arguments.content_root.resolve()), '--scene', '__absent_scene_for_native_failure_test__'], capture_output=True, text=True)
(output / 'player-invalid-scene.log').write_text(invalid.stdout + invalid.stderr)
if invalid.returncode != 1 or 'Requested scene is absent' not in invalid.stderr or 'shutdown also failed' in invalid.stderr:
    raise RuntimeError('Unknown packaged scene did not fail cleanly through actual Core startup/teardown')
receipt = {
    'platform': 'macos', 'architecture': 'arm64', 'profile': arguments.profile,
    'sdkVersion': subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-version'], text=True).strip(),
    'appleCommit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
    'engineCommit': subprocess.check_output(['git', '-C', str(root.parent / 'helengine'), 'rev-parse', 'HEAD'], text=True).strip(),
    'generatedCoreRoot': str(arguments.core_root.resolve()), 'codeRoot': str(arguments.code_root.resolve()),
    'contentRoot': str(arguments.content_root.resolve()), 'diagnosticOnly': False,
    'windowCreated': False, 'frames': 300, 'demoDiscMenuValidation': arguments.demodisc,
    'actualCoreOutput': transcript, 'unknownSceneFailureAndTeardown': 'passed',
    'renderingScenesValidated': False, 'profilerValidated': False,
}
(output / 'core-validation.json').write_text(json.dumps(receipt, indent=2) + '\n')
print('Current generated-core receipt: ' + str(output / 'core-validation.json'))
