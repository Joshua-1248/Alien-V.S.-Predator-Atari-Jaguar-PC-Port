#!/usr/bin/env python3
"""Summarize read-only, page-aggregated Jaguar GPU-view DRAM access maps.

This is a map of *observed bus requests*, NOT automatic identification of
AVP level resources, not a ROM dump, and not evidence of pixel parity.
GPU requests and Blitter requests are intentionally reported separately.
"""
import argparse
import csv

HEADER = ('page','physical_start','gpu_reads','gpu_writes',
          'blitter_reads','blitter_writes','first_gpu_pc','last_gpu_pc')
COUNTS = HEADER[2:6]


def decode(stream):
    reader = csv.DictReader(stream)
    if tuple(reader.fieldnames or ()) != HEADER:
        raise ValueError('wrong or missing view access CSV header')
    last = -1
    for line, row in enumerate(reader, 2):
        if None in row or any(row[k] is None or row[k].strip()=='' for k in HEADER):
            raise ValueError(f'malformed record on line {line}')
        try:
            record = {k:int(row[k],16 if k in ('physical_start','first_gpu_pc','last_gpu_pc') else 10)
                      for k in HEADER}
        except ValueError as exc:
            raise ValueError(f'bad numeric value on line {line}') from exc
        page = record['page']
        if not last < page < 8192 or record['physical_start'] != page*256:
            raise ValueError(f'unsorted or invalid physical address on line {line}')
        if any(record[k]<0 or record[k] > (1<<64)-1 for k in COUNTS):
            raise ValueError(f'invalid touch counter on line {line}')
        if sum(record[k] for k in COUNTS)==0:
            raise ValueError(f'empty page on line {line}')
        if any(record[k]<0 or record[k] > 0xFFFFFF for k in ('first_gpu_pc','last_gpu_pc')):
            raise ValueError(f'invalid GPU PC on line {line}')
        last = page
        yield record


def ranges(rows, counter):
    """Coalesce physically adjacent pages with the selected access type."""
    result=[]
    for row in rows:
        if row[counter]==0:continue
        a=row['physical_start']
        if result and a==result[-1][1]:
            result[-1]=(result[-1][0],a+256,result[-1][2]+row[counter])
        else:
            result.append((a,a+256,row[counter]))
    return result


def report(rows, top=12):
    print(f'Observed {len(rows)} physical DRAM pages (256-byte granularity); no resource labels inferred.')
    for key in COUNTS:
        total=sum(r[key] for r in rows)
        segments=ranges(rows,key)
        print(f'\n{key}: {total} page touches across {len(segments)} contiguous physical spans')
        for a,b,n in sorted(segments,key=lambda x:(-x[2],x[0]))[:top]:
            print(f'  ${a:06X}..${b-1:06X}  touches={n}')
    print('\nLimitations: the 2 MiB DRAM mirror is collapsed; one GPU view only;')
    print('bus access hot spots do not establish resource types, lengths, or provenance.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv',help='GPU_SCENE_ACCESS_CSV output from gpu_pipeline_check')
    parser.add_argument('--top',type=int,default=12)
    args=parser.parse_args()
    if not 1<=args.top<=256:parser.error('--top must be between 1 and 256')
    try:
        with open(args.csv,newline='',encoding='ascii') as f:rows=list(decode(f))
    except (OSError,ValueError) as e:parser.error(str(e))
    report(rows,args.top)

if __name__=='__main__':main()
