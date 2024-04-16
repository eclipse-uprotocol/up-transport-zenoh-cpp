#!/usr/bin/env python3

import sys

with open(sys.argv[1], 'r') as fh:
    lines = fh.readlines()

prior_time = None
for line in lines:
    fields = line.rstrip().split(' ')
    if fields[0] == '':
        break
    if fields[0] == 'Attaching':
        continue
    pid = int(fields.pop(0))
    tid = int(fields.pop(0))
    timestamp = int(fields.pop(0), 16)
    if prior_time is None:
        timestamp = prior_time
    # if len(fields) == 3:
    # else:
    print(f'{timestamp/1e9:12.6f} {pid} {tid} {fields}')

# while True:
#     txt = proc.stdout.readline()
#     if not txt:
#         break
#     txt = txt.rstrip().split(' ')
#     print(txt)
#     if txt[0] == 'Attaching':
#         break
#     pid = int(txt.pop(0))
#     tid = int(txt.pop(0))
#     timestamp = int(txt.pop(0), 16)
#     print(f'pid={pid} tid={tid} elapsed={timestamp/1e9:12.6f} {txt}')
