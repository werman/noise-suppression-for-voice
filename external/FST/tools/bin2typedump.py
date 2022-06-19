#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# print binary data as floats/doubles/ints/pointers

# Copyright © 2019, IOhannes m zmölnig, IEM

# This file is part of FST
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with striem.  If not, see <http://www.gnu.org/licenses/>.

import struct

# cmdline arguments
def parseCmdlineArgs():
    import argparse

    # dest='types', action='append_const',
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-d",
        "--double",
        dest="format",
        action="store_const",
        const=("d", "%16.5g"),
        help="decode as double",
    )
    parser.add_argument(
        "-f",
        "--float",
        dest="format",
        action="store_const",
        const=("f", "%16.5g"),
        help="decode as float",
    )
    parser.add_argument(
        "-i",
        "--int",
        dest="format",
        action="store_const",
        const=("i", "%16d"),
        help="decode as 32bit signed int",
    )
    parser.add_argument(
        "-s",
        "--short",
        dest="format",
        action="store_const",
        const=("h", "%8d"),
        help="decode as 16bit signed int",
    )
    parser.add_argument(
        "-p",
        "--pointer",
        dest="format",
        action="store_const",
        const=("P", "0x%016X"),
        help="decode as pointer",
    )
    parser.add_argument("files", nargs="*")
    args = parser.parse_args()
    return args


def chunks(l, n):
    """Yield successive n-sized chunks from l."""
    for i in range(0, len(l), n):
        yield l[i : i + n]


def data2strings(data, structformat, printformat="%s"):
    return [printformat % v[0] for v in struct.iter_unpack(structformat, data)]


def print_data_as(data, structformat, printformat="%s"):
    datasize = struct.calcsize(structformat)
    chunksize = max(int(16 / datasize), 1)

    for line in chunks(data2strings(data, structformat, printformat), chunksize):
        print(" ".join(line))


def _main():
    args = parseCmdlineArgs()
    for filename in args.files:
        with open(filename, "rb") as f:
            data = f.read()
        print_data_as(data, args.format[0], args.format[1])


if __name__ == "__main__":
    _main()
