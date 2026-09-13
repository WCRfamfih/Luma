"""Native GPU evidence: soft volume, isolated motion controls and transparency."""
from pathlib import Path
from PIL import Image,ImageChops
import math,json,statistics
root=Path(__file__).resolve().parents[1];folder=root/'artifacts';checks=[];metrics={}
def verify(name,condition):
 print(('PASS ' if condition else 'FAIL ')+name);checks.append({'check':name,'pass':bool(condition)})
def frame(name):return Image.open(folder/f'{name}.png').convert('RGBA')
def alpha(name):return frame(name).getchannel('A')
def delta(a,b,box=(180,180,380,380)):return sum(ImageChops.difference(a,b).crop(box).getdata())
def profile(im):
 result=[]
 for j in range(180):
  angle=j*math.tau/180;c,s=math.cos(angle),math.sin(angle)
  samples=[im.getpixel((round(280+r*c),round(280+r*s))) for r in range(135,246)]
  # A variable-width inward fold can move the brightest row while the outer
  # silhouette stays fixed. Measure the outer coverage edge, not peak light.
  result.append(max((135+i for i,a in enumerate(samples) if a>30),default=135))
 return result
for prefix in ['sphere','elastic-rotation']:
 for i in range(4):
  a=alpha(f'{prefix}-{i}');box=a.point(lambda v:255 if v>50 else 0).getbbox();w,h=box[2]-box[0],box[3]-box[1]
  verify(f'Full soft volume at {prefix} orientation {i}',.80<w/h<1.25 and min(w,h)>350)
  centre=list(a.crop((180,180,380,380)).getdata())
  verify(f'Translucent interior at {prefix} orientation {i}',statistics.mean(centre)<45 and sum(x>=230 for x in centre)/len(centre)<.03)
verify('3D rotation changes internal material projection',delta(alpha('sphere-0'),alpha('sphere-2'))>10000)
coupled=[profile(alpha(f'coupling-{k}')) for k in range(3)]
low=statistics.mean(abs(a-b) for a,b in zip(coupled[0],coupled[1]));high=statistics.mean(abs(a-b) for a,b in zip(coupled[0],coupled[2]));metrics['coupling_displacement_ratio']=low/high
verify('20% wave coupling produces only a small part of full rim distortion',high>3 and .08<low/high<.40)
scaling=profile(alpha('scale-audio'));ratio=statistics.median(a/b for a,b in zip(scaling,coupled[0]));metrics['audio_scale_ratio']=ratio
verify('Music still expands the body when wave coupling is zero',1.06<ratio<1.13)
verify('Outer shape evolves independently of music and waves',delta(alpha('coupling-0'),alpha('shape-autonomous'),(40,40,520,520))>100000)
base=alpha('bounce-off');kick=[alpha(f'bounce-{k}') for k in range(8)];movement=[delta(base,x) for x in kick];metrics['bounce_pixel_changes']=movement
verify('Independent inner kick moves particles with no travelling waves',max(movement)>20000)
verify('Kick return persists after initial transient has ended',movement[3]>15000)
verify('Independent springs settle back toward their original anchors',movement[-1]<max(movement)*.75)
verify('Inner kick leaves the exterior pixels unchanged',delta(base,kick[1],(0,0,560,75))==0)
verify('Zero inner density removes all central particles and fold emission',alpha('coupling-1').crop((180,180,380,380)).getextrema()[1]==0)
origin=alpha('origin-base');radii=[]
for k in range(3):
 a=alpha(f'origin-{k}');weight=distance=0
 for y in range(90,470):
  for x in range(90,470):
   r=math.hypot(x-280,y-280)
   # Include the approaching outer wavefront; a 170 px crop would truncate
   # the final expanding ring and incorrectly measure only its trailing wake.
   if r>190:continue
   v=max(0,a.getpixel((x,y))-origin.getpixel((x,y))-10);weight+=v;distance+=v*r
 radii.append(distance/max(1,weight))
metrics['interior_wave_radii']=radii
verify('Front-facing pole creates a wave inside the rim and expands outward',radii[0]<95 and radii[1]>radii[0]+20 and radii[2]>radii[1]+10)
centre=list(alpha('origin-0').crop((220,220,340,340)).getdata())
verify('Interior wave origin does not create a solid white hotspot',sum(x>220 for x in centre)/len(centre)<.03)
plain,glow,hot=[frame(f'bloom-{i}') for i in range(3)];outer=[];core=[]
for y in range(560):
 for x in range(560):
  b,g,h=plain.getpixel((x,y)),glow.getpixel((x,y)),hot.getpixel((x,y))
  if b[3]==0:outer.append(g[3])
  if g[3]>180:core.append(h[0]-g[0])
verify('Bloom propagates beyond the original particle footprint',sum(outer)>50000)
verify('Dense colored particles heat toward white',len(core)>100 and statistics.mean(core)>10)
verify('Bloom preserves fully transparent corners',hot.getpixel((0,0))[3]==0 and hot.getpixel((559,559))[3]==0)
rings=[[],[],[]];pa=plain.getchannel('A');ga=glow.getchannel('A')
for j in range(180):
 angle=j*math.tau/180;c,s=math.cos(angle),math.sin(angle)
 occupied=[r for r in range(135,240) if pa.getpixel((round(280+r*c),round(280+r*s)))>100]
 if not occupied:continue
 edge=max(occupied)
 for k,offset in enumerate([5,15,25]):rings[k].append(ga.getpixel((round(280+(edge+offset)*c),round(280+(edge+offset)*s))))
means=[statistics.mean(x) for x in rings];metrics['edge_relative_glow_alpha']=means
verify('Glow diminishes away from the deformed edge',means[0]>means[1]>means[2])
verify('Relative glow attenuation accelerates with distance',means[2]/means[1]<means[1]/means[0])
(folder/'render-properties.json').write_text(json.dumps({'checks':checks,'metrics':metrics},indent=2))
print(json.dumps(metrics));assert all(x['pass'] for x in checks),'Native render property failure'
