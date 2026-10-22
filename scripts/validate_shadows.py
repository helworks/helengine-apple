#!/usr/bin/env python3
"""Verify real authored shadow receiver changes with deterministic simulation state and GPU readback."""
import argparse
import json
import pathlib
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--content-root', required=True, type=pathlib.Path)
arguments = parser.parse_args()
root = pathlib.Path(__file__).resolve().parent.parent
output = pathlib.Path('/tmp/helengine/macos')
results = []
with (output/'shadow-validation.log').open('w') as log:
    for scene in ['directional_shadow_plaza', 'pbr_shadow_theater']:
        run = subprocess.run([str(output/'shadow_scene_tests'), '--offscreen', '--frames', '3',
                              '--content', str(arguments.content_root.resolve()), '--scene', scene],
                             capture_output=True, text=True, timeout=60)
        transcript = run.stdout+run.stderr
        log.write(transcript+'\n')
        metrics = {key: int(value) for key, value in re.findall(r'\b(shadow_atlas_written_texels|authored_shadow_lights|authored_shadow_receivers|shadow_changed_center_pixels|shadow_restored|extra_controlled_draws)=(\d+)', transcript)}
        valid = run.returncode == 0 and metrics.get('shadow_changed_center_pixels', 0) >= 64 and metrics.get('shadow_restored') == 1
        results.append({'sceneId': scene, 'passed': valid, 'metrics': metrics, 'output': transcript})
        print(('PASS ' if valid else 'FAIL ')+scene+' '+str(metrics), flush=True)
receipt = {'platform': 'macos', 'appleCommit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
           'windowCreated': False, 'verification': 'Actual Core.Draw with no intervening update; shadows off must change central receiver pixels, shadows on must exactly restore the original GPU readback.',
           'scenes': results, 'passed': sum(result['passed'] for result in results), 'total': len(results)}
(output/'shadow-validation.json').write_text(json.dumps(receipt, indent=2)+'\n')
raise SystemExit(0 if all(result['passed'] for result in results) else 1)
