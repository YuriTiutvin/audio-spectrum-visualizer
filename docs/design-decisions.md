# Design decisions

Every decision in the build, accepted and rejected, with the reasoning at the time. Numbered calculations referenced as "(calculation N)" are in the [calculation log](#calculation-log) at the end.

## Goal

A desk display that looks good with any music at any volume and distance: three bars that move independently, bass with the kick and treble with the hi-hats, dark in a quiet room. Accurate, comparable levels were explicitly not the goal. That decision shaped most of the others, and it was first made for the treble bar alone and then, at the end, for the whole design.

## Hardware

### 1 · Microcontroller: classic ESP32

**Decided:** ESP32-DevKitC-32UE (Xtensa dual-core, single-precision floating-point unit).
**Considered:** Arduino Uno or Nano (8-bit AVR); Teensy 4; ESP32-C3 or C6.
**Why:** a 1,024-point FFT needs floating point and memory. An 8-bit AVR fills its 2 KB of RAM with a 256-point fixed-point FFT and struggles to sample fast enough, so the work becomes fighting the chip. Teensy's audio library would supply the FFT bins ready-made and hide the part worth learning. The C3 and C6 are RISC-V parts without a floating-point unit, so float maths runs in software many times slower; an ESP32-C6 board was considered and dropped for that reason. "ESP32" is a family, and the choice within it mattered more than the name.
**Traded:** more setup than an Arduino; less ready-made DSP than a Teensy.

### 2 · Microphone: MAX4466 with fixed gain

**Decided:** Adafruit 1063, an electret capsule with a MAX4466 preamp and a manual gain trimmer.
**Considered:** MAX9814, which has automatic gain control.
**Why:** automatic gain flattens the difference between loud and quiet, which is what the bars exist to show. It also raises the noise floor on quiet passages ("breathing") and would fight any automatic scaling in firmware: two control loops acting on the same signal. A fixed gain gives a signal that can be characterised.
**Traded:** the gain has to be set by hand, once.

### 3 · Microphone supply: 3.3 V

**Decided:** power the microphone board from the ESP32's 3.3 V pin.
**Why:** the preamp output idles at half its supply and can swing rail to rail. At 5 V it would idle at 2.5 V and could reach 5 V, beyond the ADC pin's limit. At 3.3 V it idles near 1.65 V and stays in range. This is a safety requirement, not a preference. The ADC curve measured later on a same-model ESP32 puts the silent-room reading, about 1,940 counts, at about 1.66 V (calculation 10).

### 4 · Microphone gain: maximum

**Decided:** leave the trimmer at maximum.
**Why:** with the trimmer at mid-position, the loudest music spanned 1,600–2,280 counts; at maximum, 1,250–2,530 on the same music. Music played louder than normal listening, at the real microphone position, spanned 600–3,290, clear of both rails. A cough at point-blank range did clip, but that is not how the device is used. The trimmer's range is only about 25× to 125× (Adafruit's product page), so it is a fine adjustment, and at maximum the display looked best.
**Traded:** the loudest peaks reach about 2.69 V, where the ESP32's ADC starts to read high (calculation 10).

### 5 · Display driver: "LM3915", then linear with the logarithm in firmware

**First decision:** LM3915 bar-graph drivers, which switch at 3 dB steps, so a linear signal would come out logarithmic in hardware.
**Considered then:** LM3914 (linear) with the logarithm in firmware. Both are real design choices; the hardware version was chosen as a component-selection decision.
**What happened:** the chips, marked LM3915N-1 and bought as a multipack, switched at evenly spaced voltages. No wiring can make a genuine LM3915 linear, so they are counterfeit linear dies (Failure analysis A).
**Final decision:** keep the chips as linear drivers and produce the logarithm in firmware: the firmware converts each band to decibels and sends a voltage proportional to its position in a decibel window, so each LED on the linear ladder is one equal decibel step. Genuine LM3915s are obsolete and not trustworthy from cheap sources, and this path needs no hardware change.
**Traded:** the display's scale depends on the firmware; the hardware gives no protection if the firmware is wrong.

### 6 · Bar mode

**Decided:** tie MODE (pin 9) to V+ for bar mode.
**Considered:** dot mode, a single moving LED.
**Why:** a filled bar reads as a level; a dot reads as a peak marker.

### 7 · LED current: R1 = 2.2 kΩ

**First decision:** R1 = 1.2 kΩ, about 10.4 mA per LED (calculation 6). The bars are rated 25–30 mA, and a design should never run at the absolute maximum. The plan was to choose R1 per band to balance the three colours.
**Final decision:** 2.2 kΩ for all three, about 5.7 mA per LED (calculation 6). No 1.2 kΩ resistor was on hand; 2.2 kΩ was tried on all three bars, gave enough brightness, and the three colours looked alike by eye, so one value serves all three.

**Traded:** a dimmer display than the parts allow, in exchange for margin and a lower supply current (calculation 7).

### 8 · Full scale tied to 3.3 V

**Decided:** RHI (pin 6) to the 3.3 V rail directly; RLO (pin 4) to ground.
**Considered:** RHI from the chip's reference output through a 10 kΩ trimmer per band, to set full scale by eye.
**Why:** full scale then equals the PWM's maximum voltage exactly, and it no longer depends on the LED-current setting, which REF OUT also carries. All sensitivity adjustment moves to firmware. The chip runs from 5 V, so a 3.3 V RHI keeps the required headroom below V+.
**Traded:** LED 10's threshold sits at the very top of the input range. Measured, it turns on at 3.26 V, 40 mV below full scale, and stays solid at full-scale duty on all three bars.

### 9 · PWM and an RC filter as the DAC

**Decided:** LEDC PWM, 10-bit at 10 kHz, into 10 kΩ + 1 µF per channel (fc = 15.9 Hz).
**Considered:** a 50 kΩ trimmer in the filter to tune its corner by eye; a ripple-cancelling PWM DAC (an inverted PWM through a second RC branch); the ESP32's two 8-bit DACs on GPIO25 and GPIO26.
**Why:** the corner sets how fast the bar can move, not which pitches it shows. The display updates about 20 times a second, so a corner above that only passes frame-to-frame jitter; 15.9 Hz smooths it without visible lag (calculation 4). 10 kHz sits far above the corner and inside the LEDC limit (calculation 5). A ripple-cancelling DAC buys millivolts when one LED step is 330 mV. The ESP32 has only two DACs for three bands, which would have made one channel different from the other two on the board and in the parts list.
**Measured:** on the oscilloscope the 3.3 V square wave became a steady level at the driver input; at 1 V/div the ripple was below the scope's resolution.

### 10 · One bypass capacitor per driver

**Decided:** 10 µF from V+ to ground, soldered at each chip.
**Considered:** one shared 10 µF capacitor at the 5 V input.
**Why:** the datasheet warns that the chip can oscillate in bar mode without supply bypassing. A capacitor helps only the chip it sits next to; the trace inductance between a shared capacitor and a distant chip defeats it.

### 11 · Power: USB-C breakout with CC resistors

**Decided:** Adafruit USB-C breakout (4090), VBUS to the ESP32's 5V pin. It already carries 5.1 kΩ from CC1 and from CC2 to ground.
**Considered:** VBUS and ground only; sensing the charger's advertised current or using a USB-PD trigger chip.
**Why:** without CC termination many USB-C chargers, especially PD-capable ones, see nothing attached and never switch VBUS on. Both CC pins need a resistor because the plug is reversible. Requesting more than default current is a separate negotiation that this device does not need (calculation 7). A PD trigger board can negotiate 9–20 V, which would destroy the ESP32 and the drivers. The breakout measured 5.178 V before anything was connected.
**Rule:** never power from the DevKit's own USB port and the breakout at the same time; two 5 V sources would fight on one node.

### 12 · Grounding and layout

**Decided:** the driver and LED return and the microphone and ESP32 return run on separate wires to a single point; the microphone signal wire is short and routed away from the PWM wires.
**Why:** LED current switches with the music, up to about 170 mA (calculation 7). If it shared a wire with the microphone ground, the voltage drop across that wire would reach the ADC as audio, and the bars would react to themselves. PWM at 10 kHz sits just above the treble band.

### 13 · Construction: three stacked perfboards, no sockets

**Decided:** three perfboards on metal hex standoffs; every part, including the driver ICs, soldered directly.
**How:** the front board carries the LED bars and drivers; male-to-male headers carry the driver pins through to a middle board with the current-set resistors and bypass capacitors; a larger back board carries the ESP32, the USB-C breakout, the microphone board and the RC filters, with the single ground point on its back. The full layout is in [hardware/README.md](../hardware/README.md#construction).
**Why:** one board could not hold all the connections; the stack gives the room and a base wide enough to stand upright. Sockets would have been cheap insurance against the counterfeit chips, but none were on hand and the chips' only fault, linearity, is handled in firmware. The layout keeps the microphone gain trimmer reachable.
**Traded:** replacing a driver means desoldering it.

## System and signal chain

### 14 · Band edges: 80, 300, 4,000 and 10,000 Hz

**Decided:** bass 80–300 Hz, mid 300 Hz–4 kHz, treble 4–10 kHz.
**History:** the first plan was 260 Hz and 4 kHz, putting the 4–6 kHz presence region in treble so the treble bar stayed distinct. A later version moved the split to 2.8 kHz to stop the mid bar dominating, and capped treble at 12 kHz. The final edges set 80 Hz as the bottom of bass (below it, room rumble outweighs music, and 50/60 Hz mains hum falls outside the band) and 300 Hz as the split below most vocal fundamentals. Treble was moved up to start at 4 kHz in the revision that first looked clearly better on the bench, and I then set mid to run up to 4 kHz rather than leave 2.8–4 kHz in no band: more groups diluted each one, and no bar dominated. Treble stops at 10 kHz because above that the band collects mostly microphone hiss.
**Traded:** the 2–5 kHz presence region belongs to mid.

### 15 · Sample rate: whatever the blocking loop achieves, 23.1 kHz

**First decision:** 32 kHz, twice a 16 kHz top frequency.
**What happened:** each `adc1_get_raw` read takes about 43 µs, longer than the 31.25 µs slot, so the loop cannot reach 32 kHz; it runs at 23.1 kHz. `analogRead` was tried first and was no faster.
**Decided:** keep the blocking loop, measure the real rate every frame, and derive every band's bin range from it, so the edges stay in the right place (calculations 1 and 2).
**Considered:** the ESP32's continuous-ADC (DMA) driver, which samples steadily and keeps sampling during the FFT. It was built twice and rejected on the bench (entry 17).
**Traded:** about 14 % of the audio falls in the gap while the FFT runs; sample timing is not steady; the rate is fixed by read time, not chosen.
**Why this is the right choice for the goal:** the device exists to make three bars look alive, and this rate is what puts the hi-hats on the treble bar (entry 16). The costs are invisible on 10-LED bars updated 20 times a second. This was a decision that the sampler is sufficient for what the device is for, not an abandoned improvement. Steady, alias-free sampling becomes necessary only if the goal changes to measuring the spectrum, and then the hardware changes with it: a line input and an external audio ADC with its own anti-aliasing filter.

### 16 · No anti-aliasing filter

**Decided:** no analog low-pass in front of the ADC.
**Considered:** one resistor and one capacitor at about 10 kHz; raising the sample rate so that nothing audible folds.
**Why:** at 23.1 kHz, anything between 13.1 and 19.1 kHz reappears mirrored inside the 4–10 kHz treble band (calculation 3). The original plan was to handle aliasing in firmware. But music puts less than 5 % of its energy in treble, and the treble bar was the weakest of the three; the folded content is real treble, mostly hi-hats and cymbals, which can reach 19 kHz. With three bars, where in the band it lands does not matter. So the fold was kept as a deliberate boost to treble.
**Traded:** the treble bar depends on whatever the room contains above 11.6 kHz, and any change of sample rate changes it. Microphone hiss from the same range folds in too; it works because hi-hats put far more energy there than the hiss does. The fold would ruin any frequency-accuracy measurement.

### 17 · DMA sampling: tried and shelved

**Tried:** the continuous-ADC driver at 32 kHz, then at 23.1 kHz with identical display code.
**What happened:** both builds were flashed and compared on the bench with the version they would replace. At 32 kHz the treble bar went almost dead and the bars felt less lively. The folded hi-hat energy now landed above the 10 kHz treble edge and was ignored, and the bass band fell from 10 bins to 7 (calculation 2); bass lit up on room noise at settings that had kept it dark before. The 23.1 kHz version kept the same bins and the same fold and still looked worse; that cause was not isolated.
**Decided:** keep the blocking loop. The steadiness DMA offers matters for measuring a spectrum, not for three bars on a desk.

### 18 · Looks over accuracy: each bar scaled to itself

**Decided:** each band's window runs from that band's own noise floor to its own recent peak, with no shared reference.
**Why:** it makes quiet music fill the bars at any volume and distance, with no calibration and no quiet-room start-up step.
**Traded:** bar heights are not comparable between bands, and a quiet band fills its bar the same way a loud one does. An accurate display would need one fixed, calibrated reference for all three bands.

### 19 · Treble flattered deliberately

**Decided:** raise treble's bar position to the power 0.75 and let it fall more slowly (30 dB/s against 55).
**Considered:** boosting treble by a fixed number of decibels.
**Why:** the treble band arrives with far worse signal-to-noise than bass, because music puts little energy there while the microphone's hiss is spread evenly. A fixed decibel boost does nothing here: the noise floor and the peak both move with it, so it cancels out of the window. A curve on the bar position moves the bar while leaving 0 and 1, dark and full, where they are. Treble music is mostly short hits with gaps between them, so a slower fall carries each hit across the gap.
**Traded:** the treble bar is not a true reading of the treble band; it twitches more near the bottom and hangs longer after each hit.

### 20 · Firmware constants tuned by eye on the bench

**Decided:** the final gates, fall rates and treble curve were set by me on the bench, playing the same song at several levels and distances, including a voice across a 10 × 10 ft room.
**Why:** the goal is how the bars look, and that is judged by eye.

## Rejected

| Option | How far it got | Why rejected |
|---|---|---|
| Arduino Uno/Nano | analysis | 2 KB RAM, fixed-point FFT, slow sampling |
| Teensy 4 | analysis | the audio library hides the FFT |
| ESP32-C3 / C6 | a C6 board was considered | no floating-point unit |
| MAX9814 microphone | analysis | automatic gain flattens the dynamics |
| Genuine LM3915 | chosen and bought | the parts received were counterfeit; genuine ones are obsolete |
| Reference trimmer on RHI | built during bring-up | full scale tied to 3.3 V instead; its leftover wire caused a fault (B) |
| 50 kΩ trimmer in the RC filter | designed | fixed 10 kΩ committed |
| Ripple-cancelling PWM DAC | analysis | millivolt precision is pointless against 330 mV LED steps |
| ESP32 DAC outputs | analysis | only two DACs for three bands |
| FFT of 2,048 or 1,600 points | analysis | 2,048 halves the update rate; 1,600 is not a power of two (calculation 11) |
| 44.1 kHz sampling | analysis, later a shelved build | not needed for three bars |
| Anti-aliasing RC filter | idea | the fold carries useful treble (16) |
| USB-PD current negotiation | analysis | default 5 V is enough (calculation 7) |
| A floating peak LED over the bar | calculated | one analog input carries one number: the driver shows a bar or a dot, never both. Time-multiplexing the mode pin would need a smaller filter capacitor, faster PWM, a 5 V logic buffer on the mode pins and a lower R1; bar mode was kept |
| DMA sampling | two builds tested | looked worse on the bench (17) |

## Failure analysis

**A · Driver chips that were not what they said.**

*Symptom.* During single-channel bring-up on the breadboard, sweeping the driver's input slowly from a trimmer, the bar filled at a steady rate. A logarithmic driver should race through the first few LEDs and then slow down sharply near the top.

*Test.* A genuine LM3915 spaces its ten comparators 3 dB apart below full scale, so its thresholds bunch at the bottom: with full scale at 3.30 V, LED 1 switches at 0.15 V and LED 2 at 0.21 V, only 0.06 V apart, while LED 9 and LED 10 sit 0.96 V apart (calculation 8). A linear driver spaces every LED equally. Measuring two thresholds far apart settles it in minutes; the full ten-point measurement was then repeated on three chips from the same batch, driving pin 5 from a 10 kΩ trimmer and reading it with the FNIRSI 2C53T.

*Result.* All three chips switched at 0.31, 0.63, 0.97, 1.34, 1.63, 1.96, 2.28, 2.62, 2.96 and 3.26 V: a mean step of 0.328 V, no point more than 37 mV from a straight line. Plotted in decibels, a genuine LM3915's thresholds fall on a straight line of 3 dB steps; these do not ([test/comparison-01.png](../test/comparison-01.png)).

*Root cause.* The logarithmic spacing is set by the resistor ladder inside the die, so no external wiring can make a genuine LM3915 switch linearly. Chips printed "LM3915N-1" that behave exactly like a linear LM3914 are relabelled parts. They came as a cheap multipack; the LM3915 is obsolete, and obsolete analog parts from such sources are a known counterfeit risk.

*Options weighed.* Buying genuine LM3915s: obsolete, and old stock from similar sellers carries the same risk. Switching to marked LM3914s: electrically what these chips already are, so nothing would change. Keeping these chips and producing the logarithm in firmware: no hardware change, and the firmware already computes levels in decibels.

*Fix.* The last option (decision 5). The firmware sends each driver a voltage proportional to decibels, so the linear ladder shows one equal decibel step per LED. The display is logarithmic, but that now depends on the firmware alone; the hardware adds no protection if the firmware is wrong.

**B · Bar stopped at LED 8.** Symptom: with 3.3 V on the input the bar never passed LED 8, and LED 9 flickered. Root cause: RHI was still connected to REF OUT, a leftover from the trimmer setup, which set full scale at about 4 V. Fix: RHI to 3.3 V only; all ten LEDs light.

**C · Dot mode although MODE was tied to V+.** Symptom: after rewiring for the sound input, the display showed a moving dot; pin 9 and pin 3 both measured 4.8 V. Root cause: two other stray connections left from the rewiring. Fix: removing them restored bar mode. After any rewiring, every pin should carry only its intended connections.

**D · The first LED would not light on GPIO34.** Symptom: the upload succeeded, but the test LED stayed dark. An LED and resistor wired straight from 3.3 V to ground lit, which cleared the LED and the wiring. Root cause: GPIO34 is input-only on the ESP32; it has no output driver. Fix: move the test LED to GPIO23, and keep GPIO34 for the microphone.

**E · LED polarity unreadable with the meter.** Symptom: diode mode read open across a bar segment in both directions. Root cause: the meter's diode test supplies about 2 V, less than the 1.95–2.4 V the LEDs need, and a 20-pin bar pairs each anode with one specific cathode. Fix: 3.3 V through 330 Ω across a segment; it lit dimly at about 4 mA.

**F · Sample rate stuck well below 32 kHz.** Symptom: the measured rate was about 22–23 kHz, never 32 kHz. Method: time each block in firmware; swap `analogRead` for `adc1_get_raw`. Root cause: each read takes longer than the slot. Fix: decision 15.

**G · Treble dead at 32 kHz.** Decision 17.

## Open items

- Power-on: the noise floor can start far below the room's level and stay there until the board is reset ([firmware notes](../firmware/notes.md#known-limitations)).
- The cause of the 23.1 kHz DMA build's different behaviour was not found.
- The microphone capsule's protective cover lifted during soldering. The microphone board should be replaced.

## Calculation log

**1 · Frame timing.** N = 1,024. At the measured 23,116 Hz: bin width = 23,116 / 1,024 = 22.6 Hz; capture time = 1,024 / 23,116 = 44.3 ms; measured frame rate 19.6 per second, so a frame is about 51 ms and about 6.7 ms of it (14 %) is not sampled. Governs decisions 15 and 17.

**2 · Band bins.** k₀ = ⌊f_lo · N / Fs⌋ + 1, k₁ = ⌊f_hi · N / Fs⌋. At 23,116 Hz: bass 4–13 (10 bins), mid 14–177 (164), treble 178–442 (265; the band starts at 4,018 Hz). At 32,000 Hz (31.25 Hz bins) bass becomes bins 3–9, 7 bins. Governs decisions 14 and 17.

**3 · Aliasing.** Nyquist = 23,116 / 2 = 11,558 Hz. With no filter, a component at f between Nyquist and Fs appears at Fs − f: 23,116 − 13,116 = 10,000 Hz and 23,116 − 19,116 = 4,000 Hz, so 13.1–19.1 kHz folds onto exactly the 4–10 kHz treble band. Governs decision 16.

**4 · RC filter.** fc = 1 / (2π · 10,000 Ω · 1 µF) = 15.9 Hz; τ = 10 ms. Single-pole attenuation at 10 kHz ≈ 15.9 / 10,000 = 1/629; ripple ≈ 3.3 V / 629 ≈ 5.3 mV, against 330 mV per LED step. Governs decision 9.

**5 · PWM limit.** The LEDC clock allows f_pwm × 2^bits ≤ 80 MHz. 10,000 × 1,024 = 10.24 MHz; at 10 bits the ceiling is about 78 kHz. Governs decision 9.

**6 · LED current.** I_LED ≈ 10 × (1.25 V / R1) = 12.5 V / R1. R1 = 1.2 kΩ → 10.4 mA; R1 = 2.2 kΩ → 5.7 mA. Governs decision 7.

**7 · Supply current.** Measured 70.7 mA with all bars dark. All 30 LEDs at 5.7 mA add 171 mA: about 241 mA in total, against 237.5 mA measured at maximum volume with a few LEDs unlit. The original estimate, at 10 mA per LED and before any measurement, was 500–600 mA; it is superseded. Governs decisions 11 and 12.

**8 · Driver thresholds.** Linear (LM3914), RLO = 0, RHI = 3.3 V: LED n at n × 0.33 V. Logarithmic (LM3915), 3 dB per LED from 3.30 V down: each threshold is the next one divided by √2 ≈ 1.41, giving 0.15, 0.21, 0.29, 0.42, 0.59, 0.83, 1.17, 1.65, 2.34, 3.30 V. The quick test: on a logarithmic chip the gap from LED 1 to LED 2 is about 0.06 V and from LED 9 to LED 10 about 0.96 V; on a linear chip every gap is 0.33 V. Measured gaps: 0.29–0.37 V. Governs decision 5.

**9 · Driver dissipation.** All ten LEDs lit: (5 V − 1.95 V) × 5.7 mA × 10 ≈ 0.17 W per chip, using the yellow bar's typical forward voltage. At the original 10.4 mA it would have been about 0.32 W. Governs decision 7.

**10 · ADC scaling.** Measured 1,276 counts per volt from 0.24 to 2.65 V at 11 dB attenuation, on a second ESP32 of the same model; applied to the device's readings the conversions are approximate. Silent-room reading 1,940 counts → about 1.66 V, half of 3.3 V; loud-music extremes 600 and 3,290 counts → 0.60 and 2.69 V, about ±1.04 V around the bias. Above 2.65 V the ADC reads high. Governs decisions 3 and 4.

**11 · Rejected FFT sizes.** At the 32 kHz design rate: N = 2,048 gives 15.6 Hz bins but only 15.6 updates per second; N = 1,024 gives 31.25 Hz bins and 31.25 updates per second. N = 1,600 is not a power of two, which the radix-2 FFT requires. All bins collapse into three bands, so fine resolution buys nothing.
