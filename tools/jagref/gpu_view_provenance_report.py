#!/usr/bin/env python3
"""Summarize observed 68000-writer PCs of pages consumed in a GPU view.

NO proprietary memory contents or resource identity. A *last* writer to a
256-byte page may have touched only one byte and may not be its producer.
"""
import argparse
import csv

HEADER = ('page', 'physical_start', 'gpu_reads', 'blitter_reads',
          'observed_cpu_writer', 'cpu_writes', 'first_pc68', 'last_pc68',
          'first_field', 'last_field')

def decode(stream):
    reader = csv.DictReader(stream)
    if tuple(reader.fieldnames or ()) != HEADER:
        raise ValueError('wrong or missing CPU provenance header')
    last_page = -1
    for line, raw in enumerate(reader, 2):
        if None in raw or any(raw[k] is None or not raw[k].strip() for k in HEADER):
            raise ValueError(f'malformed record on line {line}')
        try:
            record = {k: int(raw[k], 16 if k in ('physical_start', 'first_pc68', 'last_pc68') else 10)
                      for k in HEADER}
        except ValueError as exc:
            raise ValueError(f'invalid number on line {line}') from exc
        page = record['page']
        if not last_page < page < 8192 or record['physical_start'] != page * 256:
            raise ValueError(f'unsorted or invalid DRAM page on line {line}')
        if any(record[k] < 0 or record[k] > (1 << 64) - 1
               for k in ('gpu_reads', 'blitter_reads', 'cpu_writes', 'first_field', 'last_field')):
            raise ValueError(f'invalid counter/field on line {line}')
        if not (record['gpu_reads'] or record['blitter_reads']):
            raise ValueError(f'page not consumed by GPU/Blitter on line {line}')
        seen = record['observed_cpu_writer']
        if seen not in (0, 1):
            raise ValueError(f'invalid writer flag on line {line}')
        if (seen == 0 and any(record[k] != 0 for k in
            ('cpu_writes', 'first_pc68', 'last_pc68', 'first_field', 'last_field'))):
            raise ValueError(f'unobserved writer with write metadata on line {line}')
        if seen and (not record['cpu_writes'] or record['first_field'] > record['last_field']):
            raise ValueError(f'inconsistent CPU write history on line {line}')
        if any(record[k] > 0xFFFFFF for k in ('first_pc68', 'last_pc68')):
            raise ValueError(f'invalid 68000 PC on line {line}')
        last_page = page
        yield record

def report(rows, top=20):
    consumed = sum(r['gpu_reads'] + r['blitter_reads'] for r in rows)
    cpu_known = [r for r in rows if r['observed_cpu_writer']]
    unknown = [r for r in rows if not r['observed_cpu_writer']]
    print(f'GPU/Blitter consumed {len(rows)} pages ({consumed} touches).')
    print(f'Pre-view CPU write observed: {len(cpu_known)} pages; no observed CPU write: {len(unknown)}.')
    print('Pages have 256-byte granularity; writer PC is the Musashi-reported current PC.')
    print('Observed CPU write does not prove ownership, complete initialization or resource identity.')
    tally = {}
    for row in cpu_known:
        k = row['last_pc68']
        if k not in tally:
            tally[k] = [0, 0, 0]
        tally[k][0] += 1
        tally[k][1] += row['gpu_reads']
        tally[k][2] += row['blitter_reads']
    print(f'\nLast-observed 68000 writer PC: {len(tally)} distinct values')
    for pc, (pages, gr, br) in sorted(tally.items(),
                                      key=lambda item: (-item[1][0], item[0]))[:top]:
        print(f'  ${pc:06X}: {pages} consumed pages, GPU reads={gr}, Blitter reads={br}')
    if unknown:
        print('\nFirst pages with no observed pre-view CPU write:')
        for r in unknown[:top]:
            print(f"  ${r['physical_start']:06X} GPU reads={r['gpu_reads']} Blitter reads={r['blitter_reads']}")
    print('\nNo resource addresses, layout sizes, producer identities, or ROM parity inferred.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', help='GPU_SCENE_PROVENANCE_CSV output')
    parser.add_argument('--top', type=int, default=20)
    args = parser.parse_args()
    if not 1 <= args.top <= 256:
        parser.error('--top must be between 1 and 256')
    try:
        with open(args.csv, newline='', encoding='ascii') as stream:
            rows = list(decode(stream))
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    report(rows, args.top)

if __name__ == '__main__':
    main()
