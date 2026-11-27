#!/usr/bin/env python3
"""Small two-shape 10 mm fit gauge, inspired by eliobou's tolerance tester.
Dependencies: numpy, scipy, networkx, trimesh, shapely, mapbox-earcut, manifold3d.
"""
from pathlib import Path
from math import pi
import json
import sys
import numpy as np
import trimesh
from shapely import set_precision
from shapely.geometry import box, Point
from shapely.ops import unary_union

OUT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parent
OUT.mkdir(exist_ok=True)
W, H, Z, DEPTH = 14.2, 16.6, 2.4, 0.4
COLS, ROWS = 6, 2
STEP = 0.05
SEGMENTS = {
 '0': 'abcdef', '1': 'bc', '2': 'abged', '3': 'abgcd', '4': 'fgbc',
 '5': 'afgcd', '6': 'afgecd', '7': 'abc', '8': 'abcdefg', '9': 'abfgcd'
}

def digit(x, y, d):
    # 0.6 mm-wide engraved strokes for 0.4 mm nozzle; they form closed, readable digits.
    width, height, sw = 2.3, 3.4, 0.6
    seg = {
        'a': box(x, y+height-sw, x+width, y+height),
        'b': box(x+width-sw, y+height/2-0.1, x+width, y+height),
        'c': box(x+width-sw, y, x+width, y+height/2+0.1),
        'd': box(x, y, x+width, y+sw),
        'e': box(x, y, x+sw, y+height/2+0.1),
        'f': box(x, y+height/2-0.1, x+sw, y+height),
        'g': box(x, y+height/2-sw/2, x+width, y+height/2+sw/2),
    }
    return unary_union([seg[c] for c in SEGMENTS[d]])

def plate(kind):
    blank = box(0, 0, W*COLS, H*ROWS)
    holes, legends = [], []
    for i in range(11):
        col, row = i % COLS, i // COLS
        cx, cy = col*W+W/2, row*H+6.45
        diameter = 10.0 + STEP*i
        if kind == 'kolo':
            # Inscribed polygon: max diameter shortfall < 0.001 mm.
            shape = Point(cx,cy).buffer(diameter/2, quad_segs=64)
        else:
            shape = box(cx-diameter/2, cy-diameter/2, cx+diameter/2, cy+diameter/2)
        holes.append(shape)
        label = f'{i*5:02d}'
        legends.extend([digit(col*W+4.55, row*H+12.2, label[0]),
                        digit(col*W+7.25, row*H+12.2, label[1])])
    assert all(blank.contains(h) for h in holes)
    assert all(h.distance(other) > 2 for i,h in enumerate(holes) for other in holes[i+1:])
    cut = set_precision(unary_union(legends), 0.001)
    # No marking overlaps a hole; engraving never reduces the nominal XY dimension.
    assert all(h.distance(cut) > 0.3 for h in holes)
    base = set_precision(blank.difference(unary_union(holes)), 0.001)
    lowmesh = trimesh.creation.extrude_polygon(base, Z, engine='earcut')
    cutters = []
    for shape in legends:
        cutter = trimesh.creation.extrude_polygon(set_precision(shape, 0.001).simplify(0.0001, preserve_topology=True), DEPTH+0.2, engine='earcut')
        cutter.apply_translation([0,0,Z-DEPTH])
        cutters.append(cutter)
    result = trimesh.boolean.difference([lowmesh, *cutters], engine='manifold')
    return result

def check_export(mesh, path):
    assert mesh.is_watertight and mesh.is_winding_consistent and mesh.volume > 0, path
    assert len(mesh.split(only_watertight=False)) == 1, path
    mesh.export(path)
    reread = trimesh.load_mesh(path, process=True)
    assert reread.is_watertight and reread.is_winding_consistent and reread.volume > 0
    return round(float(reread.volume), 2)

info = {}
for kind in ('kolo', 'kwadrat'):
    m = plate(kind)
    info[f'01_plytka_{kind}.stl'] = check_export(m, OUT/f'01_plytka_{kind}.stl')
    if kind == 'kolo':
        pin = trimesh.creation.cylinder(radius=5, height=4.0, sections=256)
    else:
        pin = trimesh.creation.box(extents=(10, 10, 4.0))
    # Place on the virtual build plate, matching exported plate convention.
    pin.apply_translation([-pin.bounds[0][0], -pin.bounds[0][1], -pin.bounds[0][2]])
    info[f'02_wkladka_{kind}.stl'] = check_export(pin, OUT/f'02_wkladka_{kind}.stl')

# Geometry verification of the exported files: each mid-plane hole has the intended
# diameter or width; compare contours against the expected centres, without slicer.
for kind in ('kolo','kwadrat'):
    obj=trimesh.load_mesh(OUT/f'01_plytka_{kind}.stl',process=True)
    path=obj.section(plane_origin=(0,0,1),plane_normal=(0,0,1))
    loops=path.discrete
    hole_loops=[]
    for ring in loops:
        xy=ring[:,:2]; span=np.ptp(xy,axis=0)
        if 9.9 < span[0] < 10.6 and 9.9 < span[1] < 10.6:
            hole_loops.append((xy[:,0].min()+span[0]/2,xy[:,1].min()+span[1]/2,span[0],span[1]))
    assert len(hole_loops)==11,(kind,len(hole_loops))
    for i in range(11):
        x=(i%6)*W+W/2; y=(i//6)*H+6.45
        found=min(hole_loops,key=lambda q: (q[0]-x)**2+(q[1]-y)**2)
        assert abs(found[2]-(10+i*STEP)) < .002 and abs(found[3]-(10+i*STEP)) < .002,(kind,i,found)
print(json.dumps({'volume_mm3':info,'holes_verified':22,'nominal_pin_mm':10},indent=2))
