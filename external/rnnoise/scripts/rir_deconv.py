#!/usr/bin/python3

import sweep
import numpy as np
from numpy import fft
from scipy import signal
from scipy.io import wavfile
import sys

def extract_sweep(pilot, y, pilot_len, sweep_len, silence_len):
    pilot = np.concatenate([pilot, np.zeros(len(y)-len(pilot))])
    N = fft.rfft(pilot)
    Y = fft.rfft(y)
    xcorr = fft.irfft(Y * np.conj(N))
    pos = np.argmax(np.abs(xcorr[:sweep_len]))
    pilot_offset = sweep_len+pilot_len+2*silence_len
    pilot1 = y[pos:pos+pilot_len]
    pilot2 = y[pilot_offset+pos:pilot_offset+pos+pilot_len]
    drift_xcorr = fft.irfft(fft.rfft(pilot1) * np.conj(fft.rfft(pilot2)))
    drift = np.argmax(np.abs(drift_xcorr))
    if drift > pilot_len//2:
        drift = drift - pilot_len
    print(f"measured drift is {drift} samples ({100*drift/(pilot_len + sweep_len + 2*silence_len)})%");
    return y[pos+pilot_len+silence_len//2 : pos+pilot_len+silence_len+sweep_len-drift+silence_len//2]

def deconv_rir(pilot, x, y, Fs=48000, duration=60):
    pilot_len = Fs
    sweep_len=Fs*duration
    silence_len = Fs
    # Properly synchronize the signal and extract just the sweep
    y = extract_sweep(pilot, y, pilot_len, sweep_len, silence_len)
    x = np.concatenate([x, np.zeros(sweep_len)])
    y = np.concatenate([y, np.zeros(sweep_len-silence_len)])
    X = fft.rfft(x)
    Y = fft.rfft(y)
    # Truncate or pad depending on the drift
    if len(Y) >= len(X):
        Y = Y[:len(X)]
    else:
        Y = np.concatenate([Y, np.zeros(len(X)-len(Y))])
    # Do the actual deconvolution
    rir = fft.irfft(Y*np.conj(X)/(1.+X*np.conj(X)))
    # Chopping the non-causal part (before the direct path)
    direct = np.max(np.abs(rir))
    direct_pos = np.argmax(np.abs(rir))
    crop_pos = np.argwhere(np.abs(rir[:direct_pos+1]) > .02*direct)[0][0]
    rir = rir[crop_pos:]
    # Chopping the everything that's buried in the noise
    noise_floor = np.mean(rir[Fs*10:Fs*20]**2)
    smoothed = signal.lfilter(np.array([.002]), np.array([1, -.998]), rir[:Fs*10]**2)
    rir_length = np.argwhere(smoothed > 15*noise_floor)[-1][0]
    rir = rir[:rir_length]
    # Normalize
    rir = rir/np.sqrt(np.sum(rir**2))
    return rir


if __name__ == '__main__':
    duration=60
    #Re-compute the sweep that was played
    sine = sweep.compute_sweep(duration)
    #Load recorded signal
    _,mic=wavfile.read(sys.argv[1])
    #Re-compute pilot sequence
    pilot=sweep.compute_sweep(1.)

    rir = deconv_rir(pilot, sine, mic, duration=duration)
    rir.astype('float32').tofile(sys.argv[2])
