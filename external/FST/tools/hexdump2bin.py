#!/usr/bin/env python3

import sys

for fname in sys.argv[1:]:
  with open(fname) as f:
    for line in f.readlines():
        data = bytes([int(_,16) for _ in line.split()])
        sys.stdout.buffer.write(data)
