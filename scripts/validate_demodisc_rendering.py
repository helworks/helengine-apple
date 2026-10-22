#!/usr/bin/env python3
"""Execute every packaged DemoDisc rendering scene through the actual Core and Metal GPU without windows."""
import argparse
import json
import pathlib
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--content-root', required=True, type=pathlib.Path)
parser.add_argument('--frames', type=int, default=3)
arguments = parser.parse_args()
if arguments.frames<3:
    parser.error('Rendering smoke validation requires at least three real update/draw frames')
root=pathlib.Path(__file__).resolve().parent.parent
output=pathlib.Path('/tmp/helengine/macos')
manifest=json.loads((arguments.content_root/'build-manifest.json').read_text())
scenes=[entry['SceneId'] for entry in manifest['Scenes'] if entry['SceneId']!='DemoDiscMainMenu']
if len(scenes)!=14 or len(set(scenes))!=14:
    raise RuntimeError('DemoDisc validation requires all 14 unique rendering scene IDs from the actual package manifest')
cpu_scenes={'ray_tracing_soft_shadows','ray_tracing_spheres','ray_tracing_teapot','software_path_tracer'}
binary=output/'helengine_macos.app/Contents/MacOS/helengine_macos'
results=[]
with (output/'rendering-scenes.log').open('w') as log:
    for scene in scenes:
        frame_count=max(1200,arguments.frames) if scene in cpu_scenes else arguments.frames
        executable=output/'demo_disc_trace_tests' if scene in cpu_scenes else binary
        command=[str(executable),'--offscreen','--frames',str(frame_count),'--content',str(arguments.content_root.resolve()),'--scene',scene]
        run=subprocess.run(command,capture_output=True,text=True,timeout=300)
        transcript=run.stdout+run.stderr
        log.write(scene+'\n'+transcript+'\n'); log.flush()
        fields=dict(re.findall(r'\b(core_frames|draw_calls|mesh_visible_samples|rgba_bytes|checksum|nonuniform|center_nonuniform|trace_spp|trace_rays|trace_nonfinite)=(\d+)',transcript))
        metrics={key:int(value) for key,value in fields.items()}
        valid=run.returncode==0 and metrics.get('core_frames')==frame_count and metrics.get('draw_calls',0)>0 and metrics.get('nonuniform')==1
        if scene in cpu_scenes:
            valid=valid and metrics.get('center_nonuniform')==1 and metrics.get('trace_spp',0)>=1 and metrics.get('trace_nonfinite')==0
        else:
            valid=valid and metrics.get('mesh_visible_samples',0)>0
        results.append({'sceneId':scene,'passed':valid,'exitCode':run.returncode,'metrics':metrics,'output':transcript})
        print(('PASS ' if valid else 'FAIL ')+scene+' '+str(metrics),flush=True)
receipt={'platform':'macos','appleCommit':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'framesPerScene':arguments.frames,'windowCreated':False,'contentRoot':str(arguments.content_root.resolve()),'passed':sum(entry['passed'] for entry in results),'total':len(results),'verification':'Actual generated Core load/update/draw/teardown, GPU readback, hardware indexed visibility for 3D scenes, central framebuffer variation for CPU-rendered texture scenes. This is smoke validation, not visual parity or performance certification.','scenes':results}
(output/'rendering-validation.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Current rendering receipt: '+str(output/'rendering-validation.json'))
sys.exit(0 if receipt['passed']==14 else 1)
