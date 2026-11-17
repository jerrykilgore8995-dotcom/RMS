# RMX16

RMX16 is a Windows x64 VST3 modeled on the original rackmount RMX 16. It generates the reverb, modulation, echoes, and special effects in real time. Measurement impulses are kept outside the signal path.

## Install and use

Download **RMX16-Windows-x64-VST3** from the [latest successful workflow](https://github.com/jerrykilgore8995-dotcom/RMS/actions/workflows/windows.yml). Extract the entire `RMX16.vst3` bundle to `C:\Program Files\Common Files\VST3`, then rescan in the DAW.

The original machine accepts one signal and provides two outputs. A stereo input to the plug-in is averaged for the wet engine. Use 100% Mix on an aux return. When inserting on a stereo track, Mix blends the processed return with latency-aligned dry stereo.

- **Program** selects the algorithm. Select the button, enter 1–18 on the keypad, then use **#**. Up and Down also step through programs.
- **Predelay**, **Decay Time**, **Low**, and **High** choose an editable field. The keypad and **#** apply direct entry. Use **\*** for a decimal in Decay Time, or for a minus sign in the Low/High fields. Nudge steps are 10 ms for standard pre-delay, 1 ms for Chorus separation, 0.1 seconds for decay, and one unit for Low/High. Unsupported fields are blank.
- **Pot** links the panel encoder to the selected field. **In** and **Out** are gain controls. **Power** mutes the wet stage and clears the running history.
- **Save** and **Recall** operate on slots 1–9. Enter the slot and press **#**. The DAW project stores these slots and the current settings.
- **Echo A/B** addresses the two delay/feedback/filter sets independently. The 1.2 s/400 ms RAM configuration is modeled. **Delay** returns converter through on output 1 and the delay on output 2.
- **Freeze** uses Decay 0.0 to clear, 0.1 to add signal, and 0.2 to hold with the input muted. Ambience at 9.9 with neutral filters also sustains.
- **Factory presets** in the lower strip provide 25 starting settings. The 18 algorithm selections remain accessible from the original-style panel. See [preset list](PRESETS.md).

The model is a preview. [Validation](VALIDATION.md) and the [Ambience capture comparison](AMBIENCE-COMPARISON.csv) document the current measurements. The chassis layout follows the original rack faceplate. The manufacturer logo and company text are removed.

## Build

```sh
cmake -S . -B build -A x64
cmake --build build --config Release --target RMX16_VST3 --parallel 3
```

CMake fetches JUCE 8.0.6. Set `RMX_JUCE_PATH` to use an existing checkout. The standalone DSP bench requires only a C++20 compiler.

## References

- [Rackmount RMX 16 User Manual, Issue 3](https://www.barryrudolph.com/recall/manuals/ams_neve_rmx_16.pdf)
- [RMX 16 User Manual with expanded program index](https://www.manualslib.com/manual/3364679/Ams-Rmx-16.html)
- [Original rack faceplate, serial 2952](https://retrogearshop.com/products/ams-rmx-16-digital-reverberation-system-vintage-reverb-rackmount-effect-serviced)
- [Tomkin Mastering hardware IR collection and public Ambience sample](https://www.tomkinmastering.com/content.php?c=ams-rmx16-ir&l=e)
- [INHALT Townhouse RMX capture library and setting descriptions](https://gearspace.com/threads/townhouse-rmx-194-individual-impulse-response-library-of-the-ams-rmx-16-is-out.1273495/)
