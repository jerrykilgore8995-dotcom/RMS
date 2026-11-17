"""Measure a matching stereo RMX16 Ambience 2.1 s capture against the rendered model.

Run rmx_bench ambience > ambience.csv to make the latency-compensated model IR.
The optional hardware WAV is a local measurement input and is never distributed.
"""
import argparse
import csv
import sys

import numpy as np
from scipy import signal
from scipy.io import wavfile


def normalise_channels(samples):
    """Remove DC using the quiet final half-second."""
    return samples - np.median(samples[-min(len(samples) // 5, 24000):], axis=0)


def decay_fit(samples, rate, upper, lower):
    power = np.mean(samples ** 2, axis=1)
    length = min(len(power) // 8, rate // 4)
    noise = np.mean(power[-length:])
    integral = np.cumsum(power[::-1])[::-1] - noise * np.arange(len(power), 0, -1)
    db = 10 * np.log10(np.maximum(1e-30, integral / max(1e-30, integral.max())))
    valid = np.flatnonzero((db < upper) & (db >= lower))
    if len(valid) < 32:
        return np.nan
    slope = np.polyfit(valid / rate, db[valid], 1)[0]
    return -60 / slope if slope < 0 else np.nan


def metrics(x, rate):
    power = np.mean(x ** 2, axis=1)
    threshold = power.max() * 1e-4
    start = np.flatnonzero(power >= threshold)[0]
    aligned = x[start:]
    yield "onset rel file ms", start * 1000 / rate
    yield "peak rel onset ms", np.argmax(power[start:]) * 1000 / rate
    yield "EDT s", decay_fit(aligned, rate, 0, -10)
    yield "T20 broadband s", decay_fit(aligned, rate, -5, -25)
    for center in (125, 250, 500, 1000, 2000, 4000, 8000):
        cut = np.array([center / np.sqrt(2), center * np.sqrt(2)])
        filt = signal.butter(4, cut, btype="bandpass", fs=rate, output="sos")
        yield f"T20 {center} Hz s", decay_fit(signal.sosfilt(filt, aligned, axis=0), rate, -5, -25)
    for ms in (10, 35, 80, 200):
        duration = int(rate * ms / 1000)
        yield f"energy first {ms} ms fraction", np.sum(aligned[:duration] ** 2)/np.sum(aligned ** 2)
    yield "zero-lag LR correlation", np.corrcoef(aligned.T)[0, 1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference")
    parser.add_argument("render")
    args = parser.parse_args()
    fs, reference = wavfile.read(args.reference)
    model = np.loadtxt(args.render, delimiter=",")
    if reference.ndim != 2 or reference.shape[1] != 2:
        parser.error("a two-channel hardware capture is required")
    ref = dict(metrics(normalise_channels(reference.astype(float)), fs))
    approximation = dict(metrics(normalise_channels(model), 48000))
    writer = csv.writer(sys.stdout, lineterminator="\n")
    writer.writerow(("metric", "capture", "model", "relative_absolute_error_pct"))
    for name, target in ref.items():
        value = approximation[name]
        error = abs((value-target)/target) * 100 if target and "onset rel file" not in name else np.nan
        writer.writerow((name, f"{target:.7g}", f"{value:.7g}", f"{error:.6g}"))


if __name__ == "__main__":
    main()
