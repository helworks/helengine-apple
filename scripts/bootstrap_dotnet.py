#!/usr/bin/env python3
"""Install the engine's exact official .NET SDK on the approved Apple host without administrator privileges."""
import hashlib
import json
import pathlib
import subprocess
import tarfile
import urllib.request

root = pathlib.Path.home()
setup = pathlib.Path('/tmp/helengine/macos/setup')
setup.mkdir(parents=True, exist_ok=True)
metadata = json.load(urllib.request.urlopen('https://builds.dotnet.microsoft.com/dotnet/release-metadata/9.0/releases.json'))
files = [file for release in metadata['releases'] for sdk in release.get('sdks', []) if sdk['version'] == '9.0.308' for file in sdk['files'] if file['rid'] == 'osx-arm64' and file['name'].endswith('tar.gz')]
if len(files) != 1:
    raise RuntimeError('Official metadata did not identify exactly one .NET SDK 9.0.308 osx-arm64 archive')
archive = setup / 'dotnet-sdk.tar.gz'
urllib.request.urlretrieve(files[0]['url'], archive)
digest = hashlib.sha512(archive.read_bytes()).hexdigest()
locked_digest = '8d4b21ffa8047ebc4459b22a6090e4221fc8b3cab45ab638dcc61f1a5eb3bf2dff9f34db305b40c795ae2baf1732ba15f91ce5dfbd718a9f445561594d9c219e'
if digest.lower() != files[0]['hash'].lower() or digest.lower() != locked_digest:
    raise RuntimeError('Official .NET archive SHA-512 mismatch')
destination = root / '.dotnet'
destination.mkdir(exist_ok=True)
with tarfile.open(archive, 'r:gz') as package:
    for member in package.getmembers():
        if not (destination / member.name).resolve().is_relative_to(destination.resolve()):
            raise RuntimeError('SDK archive path escapes installation directory')
        if member.issym() and not (destination / member.name).parent.joinpath(member.linkname).resolve().is_relative_to(destination.resolve()):
            raise RuntimeError('SDK archive symbolic link escapes installation directory')
    package.extractall(destination)
launcher = root / '.local/bin/dotnet'
launcher.parent.mkdir(parents=True, exist_ok=True)
if launcher.is_symlink() and launcher.resolve() != destination / 'dotnet':
    raise RuntimeError('Existing dotnet launcher points at a different installation')
if not launcher.exists():
    launcher.symlink_to(destination / 'dotnet')
subprocess.run([str(destination / 'dotnet'), '--list-sdks'], check=True)
engine = root / 'dev/helworks/helengine'
if (engine / '.git').is_dir():
    subprocess.run(['git', '-C', str(engine), 'status', '--short', '--branch'], check=True)
else:
    subprocess.run(['git', 'clone', '--quiet', '--branch', 'main', 'https://github.com/helworks/helengine.git', str(engine)], check=True)
print('SDK_SHA512=' + digest)
print('ENGINE_CHECKOUT=' + str(engine))
archive.unlink()
