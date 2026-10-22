#!/usr/bin/env python3
"""Inspect actual staged iOS application and debug Metal SDK provenance without launching it."""
import argparse
import hashlib
import json
import pathlib
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', required=True, choices=['iphoneos', 'iphonesimulator'])
parser.add_argument('--require-context', action='store_true', help='Require immutable SDK provenance stamped by the shared cook pipeline')
arguments = parser.parse_args()
output = pathlib.Path('/tmp/helengine/ios')
package = output/'package'
bundle = package/'helengine_ios.app'
actual = subprocess.check_output(['plutil', '-extract', 'DTPlatformName', 'raw', '-o', '-', str(bundle/'Info.plist')], text=True).strip()
expected = b'iPhoneOS26.2.sdk' if arguments.sdk=='iphoneos' else b'iPhoneSimulator26.2.sdk'
other = b'iPhoneSimulator26.2.sdk' if arguments.sdk=='iphoneos' else b'iPhoneOS26.2.sdk'
libraries = []
for path in sorted((package/'cooked').rglob('*')):
    if not path.is_file():
        continue
    data = path.read_bytes()
    if b'MTLB' not in data:
        continue
    libraries.append({'path': str(path.relative_to(package)), 'sha256': hashlib.sha256(data).hexdigest(),
                      'metalLibraryHeaders': data.count(b'MTLB'), 'expectedSdkMarkers': data.count(expected), 'otherSdkMarkers': data.count(other)})
valid = actual==arguments.sdk and bool(libraries) and all(entry['expectedSdkMarkers']>0 and entry['otherSdkMarkers']==0 for entry in libraries)
context = json.loads((package/'build-manifest.json').read_text()).get('CookContext')
if arguments.require_context:
    native = context.get('NativeSdk') if isinstance(context, dict) else None
    valid_context = isinstance(native, dict) and context.get('PlatformId')=='ios' and native.get('ToolchainId')=='apple-metal' and native.get('Name')==arguments.sdk and native.get('Version')=='26.2' and bool(native.get('CompilerIdentity'))
    if valid_context:
        identity = json.loads(context['CacheIdentity'])
        valid_context = identity.get('PlatformId')=='ios' and identity.get('NativeSdk')==native
    valid = valid and valid_context
receipt_path = output/'sdk-package-validation.json'
receipt = json.loads(receipt_path.read_text()) if receipt_path.exists() else {'platform': 'ios', 'executed': False, 'sdkEvidence': {}}
known_valid = receipt.setdefault('knownValidSdkEvidence', {})
for sdk, evidence in receipt['sdkEvidence'].items():
    if evidence.get('passed'):
        known_valid[sdk] = evidence
    else:
        receipt['sdkMismatchRegressionEvidence'] = evidence
receipt['sdkEvidence'][arguments.sdk] = {'passed': valid, 'bundlePlatform': actual, 'packageRoot': str(package),
    'executableSha256': hashlib.sha256((bundle/'helengine_ios').read_bytes()).hexdigest(),
    'cookContext': context, 'requiredCapturedContext': arguments.require_context,
    'authoredShaderAssets': libraries, 'verification': 'Actual staged debug shader assets contain raw MTLB headers and the selected SDK debug provenance; opposite SDK provenance must be absent.'}
receipt['sdkEvidence'][arguments.sdk]['cacheSignals'] = [line for line in (output/'build-cli.log').read_text().splitlines() if 'scene-package-cache' in line]
if valid:
    known_valid[arguments.sdk] = receipt['sdkEvidence'][arguments.sdk]
else:
    receipt['sdkMismatchRegressionEvidence'] = receipt['sdkEvidence'][arguments.sdk]
receipt['currentPackagedSdk'] = actual
receipt_path.write_text(json.dumps(receipt, indent=2)+'\n')
print(json.dumps(receipt['sdkEvidence'][arguments.sdk], indent=2))
raise SystemExit(0 if valid else 1)
