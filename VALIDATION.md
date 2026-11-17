# RMX16 validation

## Access and scope

The bench observes the generated audio at the converter or host boundaries. The included Ambience comparison uses one publicly downloadable stereo sample from Tomkin Mastering marked Ambience 2.1 s, 48 kHz. The source WAV was used locally and is absent from the repository and plug-in. The sample's absolute I/O gain, capture onset trim, exact hardware revision, filter positions, and repeatability are not specified. The trial uses 0 ms pre-delay and neutral filters. This sample was used to tune Ambience and is an in-sample comparison. The 194 capture Townhouse library and the rest of the Tomkin collection were identified as possible validation sources. Their commercial captures have not been acquired.

## Bench CSV

```sh
g++ -O3 -std=c++20 -Wall -Wextra tests/bench.cpp -o rmx_bench
./rmx_bench > measurements.csv
./rmx_bench ambience > ambience.csv
python tests/compare_ir.py capture.wav ambience.csv
```

The Python comparison requires NumPy and SciPy. Its capture start is a relative threshold and the exported WAV may be trimmed. Compare the onset-aligned metrics together with the recorded denominator. T20 is the sixty-decibel decay estimate extrapolated from a least-squares fit to the −5 to −25 dB part of the noise-adjusted Schroeder curve. EDT extrapolates the first 10 dB. Octave bands use the indicated center frequencies. Compare each with the 5% limit in linear units. See [the measured capture and model values](AMBIENCE-COMPARISON.csv).

| CSV record | Fields after record |
| --- | --- |
| program | One-based algorithm, name, control position (0, 0.5, or 1), peak, integrated squared amplitude. Freeze is tested in add mode. |
| echo | Output index, programmed delay in ms, measured peak onset in 40 kHz core samples. |
| delay | Host sample rate, reported fixed latency in samples, the peak sample of each output relative to that latency. |
| converter | Frequency in Hz, gain in dB, phase in degrees after host latency compensation, THD %, THD+N %, and in-band K2–K20 in dBc. |
| freeze | Hold and clear integrated squared amplitudes. |
| preset | Preset name, peak amplitude, stereo integrated squared amplitude, zero-lag L/R correlation. |

## Acceptance gaps

| Area | Implemented | Pending hardware evidence |
| --- | --- | --- |
| Ambience | Diffusion, early reflections, decay network, per-band filtering, stereo blend. | Some decay, stereo and early-energy metrics miss 5%. Repeat the captures at other settings and use independent hold-out measurements. |
| Remaining programs | 17 additional selectable algorithms with program-dependent ranges. | Match time-varying delay maps, damping, decay scaling, density, gate envelopes, reverberation phase, and stereo behavior across equipment revisions. |
| Converter | 12-bit ADC mantissa and four gain ranges converted to a nominal linear path, 16-bit DAC quantization, an 18 kHz bandwidth target, and a nominal 40 kHz internal sample rate. | Capture anti-aliasing and reconstruction phase, noise, headroom, output stage, fine distortion, and the exact selected clock. Original 28-bit arithmetic and microcode are absent. |
| Operation | Numeric entry, signed and fractional fields, nudge, pot, A/B Echo edit, power and the nine mainframe user stores. | The remote handset and barcode upload are outside the mainframe implementation. Verify program defaults, display cadence, control tapers and soft-program revision details on a matching unit. |
| Faceplate | Original rack proportions and layout, manually redrawn against a straight-on photo of serial 2952. | Artwork has no measured pixel tolerance. Verify the type, button profiles and lit display against a powered unit. |
| 25 presets | Each renders two active channels and passes finite-output checks. | Audition against actual vocal, guitar, bass and drum tracks. |

For hardware acceptance, log the unit serial, ROM/RAM revision, controls and calibrated loopback. Sweep decay, pre-delay and both filter controls at fixed levels. Capture repeated IRs across phases of the modulated programs, plus sustained tones and transients for Echo, Chorus and the analog path. Compare independently held-out captures for decay time, early-energy curves, spectral decay, correlation and timing. Measure phase and calibrated distortion in the Delay-through path separately.
