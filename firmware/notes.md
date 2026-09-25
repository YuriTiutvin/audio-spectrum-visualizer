# Firmware notes

## Authorship

The firmware was written with AI assistance. The design constraints, the choice of every option recorded in [docs/design-decisions.md](../docs/design-decisions.md), all bench testing, the final tuning constants and the decision to ship this version were mine.

## Structure

[`fft_spectrum_visualizer.ino`](src/fft_spectrum_visualizer/fft_spectrum_visualizer.ino) is one file of 155 lines: a hardware block (pins, PWM settings), the band edges, a tuning table with one column per band, per-band state arrays, `captureSpectrum()`, `setup()` and `loop()`. There are no interrupts, timers or tasks; everything runs in `loop()`.

## One frame

1. **Capture.** 1,024 reads of `adc1_get_raw` on GPIO34, each paced by a busy-wait against `micros()` that asks for 32 kHz. A read takes about 43 µs, longer than the 31.25 µs slot, so the wait never engages and the loop runs at whatever the reads allow: 23,107–23,124 Hz measured. The real rate is computed every frame as (N − 1) / elapsed time, since N samples span N − 1 intervals.
2. **Spectrum.** Subtract the block mean (the microphone's DC offset), apply a Hann window, run a 1,024-point single-precision FFT, and replace each bin with its power, re² + im². Single precision matters: the ESP32's floating-point unit handles `float` only, and `double` would be emulated in software.
3. **Band levels.** For each band, the bin range is recomputed from that frame's measured rate. At 23,116 Hz a bin is 22.6 Hz wide: bass is bins 4–13, mid 14–177, treble 178–442. The level is the mean power of the bins that lie above the band's own mean, converted to decibels with 10·log10. Using only the upper bins stops a narrow sound from being averaged away across a wide band.
4. **Display mapping**, per band, below.
5. **Output.** Duty 0–1,023 to the band's PWM pin at 10 kHz, and every 200 ms one status line on the serial port.

## Display mapping

Each band has its own window in decibels, and the bar shows where the current level sits inside it.

- **Smoothing.** The level is smoothed with a 60 ms time constant.
- **Noise floor.** It steps down at 3 dB/s whenever the level is below it, and creeps up at 0.18 dB/s only while the level is within the band's gate above it, that is, only on frames already treated as silence. Music sits above the gate, so it can never drag the floor up and fade itself out. If a bar has been pinned at full scale for about 12 s, the floor is allowed to climb at 3 dB/s; this recovers from the gain being turned down and back up.
- **Window.** Bottom = floor + gate (6 / 4 / 2 dB for bass / mid / treble). Top = the band's recent peak + 2 dB; the peak follows rises with a 40 ms time constant and relaxes at 2.5 dB/s. The window is at most 10 / 12 / 26 dB wide, measured down from the top, and at least 6 / 5 / 3.5 dB. The top follows the music and the bottom follows the room; if both followed the music, the bar would always sit mid-scale.
- **Bar movement.** The bar jumps up instantly and falls at 55 / 55 / 30 dB/s.
- **Position.** pos = (level − bottom) / (top − bottom). Treble's position is raised to the power 0.75 to lift its middle, which makes treble easier to fill; bass and mid are unchanged. duty = pos × 1,023.

Because the duty is proportional to decibels and the driver chips are linear, each LED is one equal decibel step. Every constant is a ratio (dB, dB/s or seconds), and every rate is multiplied by the measured frame time, so nothing depends on the gain setting, the room's loudness or the frame rate.

## Timing

| Stage | Time | Basis |
|---|---|---|
| Capture, 1,024 samples at 23,116 Hz | 44.3 ms | calculated from the measured rate |
| Frame | about 51 ms (19.6 frames/s) | measured |
| FFT and processing, not sampled | about 6.7 ms | difference of the two |

About 14 % of the audio is never sampled, because nothing is captured while the FFT runs. A short hit can fall in that gap.

## Why the sampler stays as it is

The capture loop falls short of the 32 kHz it asks for, and that is a known, accepted trade, not an unfinished fix.

**What it costs.** The rate is set by how long each ADC read takes, 23.1 kHz, not by a clock. About 14 % of the audio falls in the gap while the FFT runs. Sample spacing is not perfectly even. And with no anti-aliasing filter, anything between 11.6 and 23.1 kHz folds back below 11.6 kHz.

**Why it is the right choice for this device.** The goal is three bars that look alive on a desk. On a room microphone the treble band is always the weakest, because music puts under 5 % of its energy there. At exactly this rate, 13–19 kHz folds onto the 4–10 kHz treble band, so the hi-hat and cymbal energy that sits up there lands on the treble bar. A missed 7 ms slice or a slightly uneven sample spacing is invisible on a 10-LED bar updated 20 times a second.

**What was tried.** Two DMA versions sampled continuously and on a steady clock. At 32 kHz the fold disappeared and the treble bar went nearly dead. At 23.1 kHz, with identical display code, the bars still looked worse, for reasons not isolated. Both were judged on the bench, against the version they would have replaced.

**When it would change.** Steady, continuous, alias-free sampling matters when the device has to measure a spectrum accurately, for example drawing it as a graph or checking a known tone lands in the right bin. That is a different device: a line input and an external audio ADC with its own anti-aliasing filter, and one shared, calibrated scale for every band. For three bars that are meant to look good, this sampler is the better one.

## Peripherals

| Peripheral | Settings |
|---|---|
| ADC1 channel 6 (GPIO34) | 12-bit, 11 dB attenuation, legacy one-shot reads |
| LEDC PWM | GPIO27 bass, GPIO26 mid, GPIO25 treble; 10 kHz, 10-bit |
| UART0 | 115,200 baud status line |

## Version history

The display mapping took many attempts. The first working sketch summed each band's raw FFT magnitudes with a per-bin noise gate and mapped them linearly; a held tone slowly faded as the per-bin floor adapted to it. A larger rewrite added logarithmic scaling, automatic gain and peak hold, and was then cut back because the bars looked like a meter rather than moving with the music, and music from a phone about a metre away did not register at all. The final line started again from the simple sketch and changed one mechanism at a time: the band level moved to decibels, the noise floor learned only from quiet frames, and the window top began following each band's peaks. The treble display curve came last, followed by my own tuning of the gates, fall rates and curve on the bench. This file is that tuned version, tidied up with the same constants: a test switch and unused code removed, and the status line changed to print the window actually in use.

Sampling through the ESP32's continuous-ADC (DMA) driver was tried twice and rejected on the bench. At 32 kHz the treble bar went almost dead: the hi-hat energy that folds into the treble band at 23.1 kHz landed above the band instead, and the bass band shrank from 10 bins to 7. A DMA version at 23.1 kHz, with identical display code, also looked worse; that cause was not isolated. DMA was shelved.

## Known limitations

- **Power-on.** The floor, peak and level all start at the first frame's value. The window can therefore start far from the room's level, and it sometimes needs time to settle. In some starts the floor sits far below the room and never rises. As I understand it, the floor rises only on frames within the gate above it; if it starts lower than that, no frame qualifies, and the stuck-floor escape fires only when a bar is pinned at full scale. Pressing EN restarts it with a better first frame. This has not been isolated further.
- **Bars are not comparable.** Each bar scales to its own noise and peaks, so a quiet band fills its bar the same way a loud one does. This is the design choice, not a fault.
- **Treble depends on aliasing.** Any change to the sample rate changes what folds into the treble band.
- **Sample timing** is set by the busy-wait loop and is not steady; it is invisible on the bars.

## Testing

No automated tests are published. Verification was on the bench, against the hardware and the serial output, and the firmware was not refactored after it passed; see [test/README.md](../test/README.md).
