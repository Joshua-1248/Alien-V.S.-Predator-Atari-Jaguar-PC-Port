#!/usr/bin/env python3
"""Reduce R102 read-only Jaguar reference-machine mailbox transactions to
one row per original GPU stage.  No model timestamps are hardware timings."""
import argparse
import csv
import sys


def parse(path):
    with open(path, newline='', encoding='utf-8') as f:
        return list(csv.DictReader(f))


def summarize(rows):
    phase_writes = [r for r in rows if r['master'] == 'GPU' and
                    r['action'] == 'write' and r['address'] == '007fb4']
    if [int(x['value'], 16) for x in phase_writes] != list(range(1, 17)):
        raise ValueError('GPU stages are incomplete, repeated or out of order; use a window containing all 16 stages')
    result = []
    for i, phase in enumerate(phase_writes):
        stage = i + 1
        first = int(phase['event'])
        last = int(phase_writes[i+1]['event']) if i < 15 else 10**12
        part = [r for r in rows if first < int(r['event']) < last]
        pubs = [r for r in part if r['master'] == 'GPU' and r['address'] == '007fa4']
        clears = [r for r in part if r['master'] == 'GPU' and r['address'] == '007fa8' and int(r['value'],16) == 0]
        if len(clears) != 1 or not pubs:
            raise ValueError(f'stage {stage}: expected a GPU image publish and exactly one GPU ACK clear, got {len(pubs)}/{len(clears)}')
        pub = pubs[-1]
        clear = clears[0]
        if int(pub['event']) >= int(clear['event']):
            raise ValueError(f'stage {stage}: publish must precede GPU ACK clear')
        acks = [r for r in part if r['master'] == '68000' and r['address'] == '007fa8' and int(r['value'],16) == 1 and int(r['event']) > int(clear['event'])]
        if not acks:
            raise ValueError(f'stage {stage}: missing CPU ACK after GPU clear')
        ack = acks[0]
        result.append({
            'stage': stage,
            'stage_event': first,
            'stage_model_field': phase['model_field'],
            'stage_model_halfline': phase['model_halfline'],
            'publish_event': pub['event'],
            'publish_model_field': pub['model_field'],
            'publish_model_halfline': pub['model_halfline'],
            'publish_image_pointer': pub['value'],
            'gpu_ack_clear_event': clear['event'],
            'gpu_ack_clear_model_field': clear['model_field'],
            'gpu_ack_clear_model_halfline': clear['model_halfline'],
            'cpu_ack_set_event': ack['event'],
            'cpu_ack_set_model_field': ack['model_field'],
            'cpu_ack_set_model_halfline': ack['model_halfline'],
        })
    return result


def main():
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('input_csv')
    a.add_argument('--output', help='CSV path (default standard output)')
    args = a.parse_args()
    rows = summarize(parse(args.input_csv))
    try:
        stream = open(args.output, 'w', newline='', encoding='utf-8') if args.output else sys.stdout
        with stream if args.output else __import__('contextlib').nullcontext(stream):
            w = csv.DictWriter(stream, fieldnames=rows[0].keys())
            w.writeheader()
            w.writerows(rows)
    except OSError as exc:
        a.error(str(exc))
    print('PASS: 16 consecutive GPU stage writes; each published an image, cleared ACK, and received a CPU ACK set. Timing is ONLY the JagMachine model.', file=sys.stderr)


if __name__ == '__main__':
    main()
