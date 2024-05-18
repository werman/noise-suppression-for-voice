#!/usr/bin/python

from __future__ import print_function

import numpy as np
import h5py
import sys

data = np.fromfile(sys.argv[1], dtype='float32');
data = np.reshape(data, (int(sys.argv[2]), int(sys.argv[3])));
h5f = h5py.File(sys.argv[4], 'w');
h5f.create_dataset('data', data=data)
h5f.close()
