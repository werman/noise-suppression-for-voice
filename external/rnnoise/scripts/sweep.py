#!/usr/bin/python3

import scipy.io.wavfile as wav
import numpy as np
import sys

def compute_sweep(T, Fs=48000, F0=100):
    F1=Fs//2
    b=np.log((F1+F0)/F0)/T
    a=F0/b
    n=np.arange(T*Fs)
    t = n/Fs
    y=0.9*np.sin(2*np.pi*a*(np.exp(b*t)-b*t-1))
    return y

def compute_sequence(T, Fs=48000, F0=100):
    noise = compute_sweep(1, Fs, F0)
    zeros = np.zeros(Fs)
    sine = compute_sweep(T, Fs, F0)
    sequence = np.concatenate([zeros, noise, zeros, sine, zeros, noise, zeros])
    return np.round(32768*sequence).astype('int16')

if __name__ == '__main__':

    filename = sys.argv[1]
    Fs = 48000
    seq = compute_sequence(60, Fs=Fs)
    wav.write(filename, Fs, seq)
