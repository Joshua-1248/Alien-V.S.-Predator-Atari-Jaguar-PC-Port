#!/usr/bin/env python3
"""Compare a locally generated Jaguar reference-machine title trace.

Run the project's original-ROM reference machine separately (private input):
  jagref /private/AVP.jag 570 6 /private/title_fields
  python3 tools/audit_retail_title_envelopes_r79.py /private/title_fields

Uses only standard-library Python, reads generated P6 PPMs, prints numeric
brightness estimates. It does not extract, bundle or export any game pixels.
Model timing is not a guarantee of physical-Jaguar cycle accuracy.
"""
import sys
from pathlib import Path

def image(root,field):
    path=root/f'frame_{field:05d}.ppm'
    with path.open('rb') as fh:
        if fh.readline().strip()!=b'P6': raise ValueError(f'{path}: not P6')
        dims=fh.readline().split()
        if len(dims)!=2: raise ValueError(f'{path}: unsupported PPM header')
        w,h=map(int,dims)
        if fh.readline().strip()!=b'255': raise ValueError(f'{path}: not 8-bit PPM')
        data=fh.read()
    if len(data)!=w*h*3: raise ValueError(f'{path}: truncated/oversized')
    return w,h,data

def dot3(a,b,select=None):
    if select is None:return sum(int(x)*int(y) for x,y in zip(a,b))
    return sum(int(a[i])*int(b[i]) for i in select)

def roi_indices(w,h,y0,y1,x0=0,x1=320):
    if w!=320 or h<y1: raise ValueError('expected 320-wide original title')
    return (i for y in range(y0,y1) for i in range((y*w+x0)*3,(y*w+x1)*3))

def difference(a,b):
    return [int(x)-int(y) for x,y in zip(a,b)]

def main(root):
    image_cache={n:image(root,n) for n in (348,354,360,366,372,378,384,396,
                                        408,420,426,432,438,444,450,456,462,468)}
    w,h,full_bg=image_cache[396]
    bg_roi=tuple(roi_indices(w,h,100,190,0,150))
    bg_den=dot3(full_bg,full_bg,bg_roi)
    print('Background (RGB least-squares brightness versus field 396):')
    bg={}
    for n in (348,354,360,366,372,378,384):
        b=image_cache[n][2];v=dot3(b,full_bg,bg_roi)/bg_den
        bg[n]=v
        print(f'  field {n}: {v:.4f}')
    if not(0<=bg[348]<0.08 and 0.06<bg[354]<0.30 and
           0.28<bg[366]<0.66 and 0.72<bg[384]<1.07):
        raise AssertionError('model background brightness unexpectedly changed')
    pred_roi=tuple(roi_indices(w,h,105,180))
    target=difference(image_cache[468][2],full_bg)
    denominator=dot3(target,target,pred_roi)
    if denominator<=0: raise AssertionError('reference logo signal missing')
    print('PREDATOR (least-squares image difference versus field 468):')
    pred={}
    for n in (408,420,426,432,438,444,450,456,462,468):
        diff=difference(image_cache[n][2],full_bg)
        v=dot3(diff,target,pred_roi)/denominator
        pred[n]=v
        print(f'  field {n}: {v:.4f}')
    if not(pred[420]>pred[432] and pred[438]<pred[450]<pred[456]<pred[462]
           and abs(pred[468]-1)<0.001):
        raise AssertionError('model PREDATOR two-wave brightness missing')
    print('PASS original-ROM Jaguar-model background and PREDATOR temporal envelopes')

if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit(f'usage: {sys.argv[0]} <private-jagref-PPM-dir>')
    main(Path(sys.argv[1]))
