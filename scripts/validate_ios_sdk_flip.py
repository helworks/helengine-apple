#!/usr/bin/env python3
"""Validate immutable iOS SDK cooking across four real CLI builds without clearing cache."""
import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import sys


def managed_hashes(roots):
    """Identify every loaded CLI/plugin dependency without rebuilding between SDK selections."""
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for root in roots for path in sorted(root.glob('*.dll'))}


def main():
    """Build, inspect and preserve one current receipt, failing on any stale SDK payload."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', required=True)
    parser.add_argument('--default-font-source', required=True)
    arguments = parser.parse_args()
    output = pathlib.Path('/tmp/helengine/ios')
    engine = pathlib.Path.home()/'dev/helworks/helengine'
    apple = pathlib.Path(__file__).resolve().parent.parent
    cli = output/'managed/bin/helengine.buildcli/debug'
    wrapper = pathlib.Path('/tmp/helengine/macos/managed/bin/helengine.ios.builder/debug')
    baseline = managed_hashes([cli, wrapper])
    assert (cli/'helengine.buildcli.dll').is_file() and (wrapper/'helengine.ios.builder.dll').is_file()
    receipt = {'platform': 'ios', 'executed': False, 'passed': False,
               'engineCommit': subprocess.check_output(['git', '-C', str(engine), 'rev-parse', 'HEAD'], text=True).strip(),
               'appleCommit': subprocess.check_output(['git', '-C', str(apple), 'rev-parse', 'HEAD'], text=True).strip(),
               'managedAssemblySha256': baseline, 'cacheCleared': False, 'cases': []}
    receipt_path = output/'sdk-flip-validation.json'
    environment = dict(os.environ, UseArtifactsOutput='true', ArtifactsPath=str(output/'managed'),
        HELENGINE_SOURCE_ROOT=str(engine), HELENGINE_BUILD_WORKSPACE_ROOT=str(output/'workspace'),
        HELENGINE_BUILD_CACHE_ROOT=str(output/'cache'), HELENGINE_BUILD_CONFIGURATION='Debug',
        HELENGINE_BUILD_PROFILE='debug', HELENGINE_APPLE_ROOT=str(apple))
    command = ['dotnet', str(cli/'helengine.buildcli.dll'), '--default-font-source', arguments.default_font_source,
               '--project', arguments.project, '--build', 'ios', '--build-profile', 'debug', '--output', str(output/'package')]
    try:
        for sdk in ['iphoneos', 'iphonesimulator', 'iphoneos', 'iphonesimulator']:
            print('Building and inspecting '+sdk, flush=True)
            environment['HELENGINE_APPLE_SDK'] = sdk
            with (output/'build-cli.log').open('w') as log:
                subprocess.run(command, cwd=engine, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
            subprocess.run([sys.executable, str(apple/'scripts/inspect_ios_package.py'), '--sdk', sdk, '--require-context'],
                           stdout=subprocess.DEVNULL, check=True)
            evidence = json.loads((output/'sdk-package-validation.json').read_text())['sdkEvidence'][sdk]
            cache = (output/'CMakeCache.txt').read_text().splitlines()
            evidence['nativeSysroot'] = next(line.split('=', 1)[1] for line in cache if line.startswith('CMAKE_OSX_SYSROOT:'))
            assert evidence['nativeSysroot']==sdk, 'Native compiler SDK differs from captured cook SDK'
            assert managed_hashes([cli, wrapper])==baseline, 'Managed compiler dependencies changed during SDK flip'
            receipt['cases'].append(evidence)
            receipt_path.write_text(json.dumps(receipt, indent=2)+'\n')
        first, second, third, fourth = receipt['cases']
        assert first['cookContext']['CacheIdentity']!=second['cookContext']['CacheIdentity']
        for cold, warm in [(first, third), (second, fourth)]:
            assert cold['cookContext']==warm['cookContext'], 'Repeated SDK selection changed immutable identity'
            assert cold['authoredShaderAssets']==warm['authoredShaderAssets'], 'Warm SDK cache changed cooked Metal payload'
            assert any('scene-package-cache hit' in line for line in warm['cacheSignals']), 'Repeated SDK selection did not exercise warm cache'
        receipt['passed'] = True
        receipt['currentPackagedSdk'] = fourth['bundlePlatform']
        print('iOS SDK flip: 4/4 packages passed; same DLLs, isolated identities and warm cache hits', flush=True)
    finally:
        receipt_path.write_text(json.dumps(receipt, indent=2)+'\n')


if __name__ == '__main__':
    main()
