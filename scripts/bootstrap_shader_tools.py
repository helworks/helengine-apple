#!/usr/bin/env python3
"""Build immutable official shaderc and SPIRV-Cross tools locally without installing any GPU runtime."""
import json
import pathlib
import subprocess

tools = pathlib.Path.home() / 'dev/helworks/tools'
output = pathlib.Path('/tmp/helengine/macos/toolchain')
output.mkdir(parents=True, exist_ok=True)
cmake = tools / 'cmake-3.31.12-macos-universal/CMake.app/Contents/bin/cmake'
dependencies = [
    {'name': 'spirv-cross', 'url': 'https://github.com/KhronosGroup/SPIRV-Cross.git', 'tag': 'vulkan-sdk-1.4.328.1', 'commit': '7affe74d77f93a622bb5002789d5332d32e512ee', 'flags': ['-DSPIRV_CROSS_ENABLE_TESTS=OFF', '-DSPIRV_CROSS_ENABLE_C_API=OFF']},
    {'name': 'shaderc', 'url': 'https://github.com/google/shaderc.git', 'tag': 'v2025.3', 'commit': 'eb0bc20d0c31320fe834e4ee8eb02807292b9703', 'flags': ['-DSHADERC_SKIP_TESTS=ON', '-DSHADERC_SKIP_EXAMPLES=ON', '-DSHADERC_SKIP_COPYRIGHT_CHECK=ON', '-DSPIRV_SKIP_TESTS=ON', '-DBUILD_TESTING=OFF']}
]
for dependency in dependencies:
    source = tools / (dependency['name'] + '-' + dependency['tag'])
    install = tools / dependency['name']
    if not (source / '.git').exists():
        subprocess.run(['git', 'clone', '--quiet', '--depth', '1', '--branch', dependency['tag'], dependency['url'], str(source)], check=True)
    actual = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != dependency['commit']:
        raise RuntimeError('Dependency commit does not match immutable lock: ' + dependency['name'])
    if dependency['name'] == 'shaderc':
        subprocess.run(['python3', 'utils/git-sync-deps'], cwd=source, check=True)
    build = output / dependency['name']
    with (output / (dependency['name'] + '.log')).open('w') as log:
        subprocess.run([str(cmake), '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_INSTALL_PREFIX=' + str(install)] + dependency['flags'], stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run([str(cmake), '--build', str(build), '-j', '6'], stdout=log, stderr=subprocess.STDOUT, check=True)
        subprocess.run([str(cmake), '--install', str(build)], stdout=log, stderr=subprocess.STDOUT, check=True)
    print(dependency['name'] + ' installed from ' + actual + ' at ' + str(install), flush=True)
(output / 'dependencies.json').write_text(json.dumps(dependencies, indent=2) + '\n')
