#!/usr/bin/env python3
"""Return from every authored rendering scene through both actual SDL input paths without direct scene requests."""
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
manifest = json.loads((arguments.content_root/'build-manifest.json').read_text())
scenes = [entry['SceneId'] for entry in manifest['Scenes'] if entry['SceneId']!='DemoDiscMainMenu']
if len(scenes)!=14 or len(set(scenes))!=14:
    raise RuntimeError('Scene return validation requires every one of the fourteen authored rendering scenes')
results = []
with (output/'scene-return-validation.log').open('w') as log:
    for scene in scenes:
        for device in ['keyboard', 'gamepad']:
            run = subprocess.run([str(output/'demo_disc_scene_return_tests'), device, '--offscreen', '--frames', '120',
                                  '--content', str(arguments.content_root.resolve()), '--scene', scene], capture_output=True, text=True, timeout=60)
            transcript = run.stdout+run.stderr
            log.write(scene+' '+device+'\n'+transcript+'\n'); log.flush()
            metrics = {key: int(value) for key, value in re.findall(r'\b(returned_frame|returned_menu_items|scene_draw_frames|return_input_captures)=(-?\d+)', transcript)}
            valid = run.returncode==0 and metrics.get('returned_frame', -1)>=10 and metrics.get('returned_menu_items')==44 and metrics.get('scene_draw_frames')==10
            results.append({'sceneId': scene, 'device': device, 'passed': valid, 'metrics': metrics, 'output': transcript})
            print(('PASS ' if valid else 'FAIL ')+scene+' '+device+' '+str(metrics), flush=True)
receipt = {'platform': 'macos', 'appleCommit': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
           'windowCreated': False, 'verification': 'Actual scene load/update/draw followed by SDL Escape or virtual-controller East edge. Only authored generated gameplay requests the return transition; all 44 menu items must reappear.',
           'passed': sum(result['passed'] for result in results), 'total': len(results), 'scenes': results}
(output/'scene-return-validation.json').write_text(json.dumps(receipt, indent=2)+'\n')
raise SystemExit(0 if all(result['passed'] for result in results) else 1)
