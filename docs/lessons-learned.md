# Lessons learned

My second hardware project, and the first that combined this many unfamiliar parts and ideas: my first ESP32, my first C++ firmware, my first FFT, and a mixed analog and digital signal path. Organised by concept, for re-reading before an interview.

## Sampling

**The real rate is what the hardware allows, not what the code asks for.** The loop asks for a sample every 31.25 µs (32 kHz), but one `adc1_get_raw` read takes about 43 µs, so the wait between samples never engages and the loop runs at 23.1 kHz. The fix is to measure the rate and derive everything from the measurement: the bin width, which bins belong to which band, and what aliases where. The cost of a blocking loop is twofold. Nothing is sampled while the FFT runs, about 14 % of the time, and sample spacing jitters whenever something else takes the CPU.

**Aliasing moves content; it does not delete it.** With no filter in front of the ADC, a tone between Nyquist (half the sample rate) and the sample rate comes back mirrored at Fs − f. At 23.1 kHz, 13.1–19.1 kHz lands exactly on 4–10 kHz, the treble band. Normally that is an error. Here it carried real hi-hat and cymbal energy into the weakest bar, so it was kept. The lesson that came with it: changing the sample rate changes several things at once. Going to 32 kHz removed the fold and also cut the bass band from 10 bins to 7, and the display got worse in both places.

**A faster, steadier sampler is not automatically better.** Steady DMA sampling matters for measuring a spectrum; for three bars it removed the treble the display depended on.

## The FFT and the bands

**DC has to go before the window.** The microphone's output sits at about 1.66 V, a huge constant. The Hann window spreads that DC into the lowest bins, so ignoring bin 0 is not enough; subtracting each block's mean removes it at the source.

**Windowing trades leakage for width.** A tone that does not land exactly on a bin leaks into its neighbours; the Hann window reduces far leakage at the cost of a wider main peak. With only three bands, that trade costs nothing.

**How a band becomes one number decides what the bar means.** Summing all bins in a band lets a wide band of noise outweigh a single tone; averaging all bins dilutes a narrow sound across hundreds of bins. The final firmware averages only the bins above the band's own mean.

**Band edges decide which bar dominates.** The 2–5 kHz presence region is loud in most music; whichever band owns it looks busy. Low edges import room rumble: HVAC and building noise pile into the bass band, and 50/60 Hz mains hum sits below the 80 Hz edge on purpose. A hummed low note lights the mid bar because its harmonics genuinely fall in the mid band; that is not leakage.

## Noise, windows and auto-scaling

**A noise floor may only learn from silence.** If the floor is allowed to rise whenever the level is above it, it cannot tell a louder room from music playing, and sustained music slowly fades itself out. The floor here rises only on frames already within the gate above it, frames the display already treats as silence.

**Only one end of a window may follow the signal.** The window's top follows the music's recent peaks; its bottom follows the room. If both followed the music, the level would always sit between them and the bar would park at mid-scale.

**Treble is weak on a room microphone for physical reasons.** Music puts under 5 % of its energy above 4 kHz, while a microphone's hiss is spread evenly across frequency, so the treble band always has the worst signal-to-noise ratio. Firmware can flatter treble; it cannot recover the signal.

**A constant decibel boost does nothing in a self-scaling display.** The floor and the peak both move with it, so it cancels out of the bar position. Only a curve applied to the position changes what the bar shows, and a curve that lifts small values also magnifies their noise.

**Looking good and being accurate pull apart.** Each bar scaled to its own floor and peak makes the display lively at any volume, and makes the bars incomparable with each other. An accurate display needs one shared, calibrated reference.

## Analog display chain

**A PWM pin plus an RC filter is a DAC.** The average of a PWM signal is duty × 3.3 V; a low-pass filter extracts it. The corner frequency sets how fast the output can change: 15.9 Hz here, against a display that updates about 20 times a second, with about 5 mV of calculated ripple left from the 10 kHz carrier. The oscilloscope showed the 3.3 V square wave turned into a steady level.

**A linear ladder can still show a logarithmic scale.** The firmware converts each band to decibels (10·log10 of power) and sends a voltage proportional to where that level sits in its decibel window. The chip's linear ladder then gives every LED the same number of decibels. The logarithm happens in code; the chip only has to be linear.

**A bar-graph driver sinks current.** The LED anodes go to 5 V and the driver pulls each cathode low, regulating the current itself, so the LEDs need no series resistors. That tripped me up at first because I expected the chip to source the current.

**One analog input carries one number.** A bar and a separate floating peak LED would need two numbers; the driver shows a bar or a dot, never both from one voltage.

## Parts and verification

**A part is what it measures, not what it is marked.** The drivers are marked LM3915, a logarithmic part. Their thresholds were evenly spaced, about 0.33 V apart, which only a linear part produces. Obsolete analog parts from cheap multipacks are a known counterfeit risk.

**The ESP32's ADC is not linear across its range.** Measured at 11 dB attenuation, it reads 0 below about 0.13 V, is close to a straight line from 0.24 to 2.65 V, and then reads high, reaching 4,095 by 3.25 V.

**USB-C gives nothing until the sink identifies itself.** A 5.1 kΩ resistor from each CC pin to ground tells a charger that a device wants default 5 V. Both pins need one because the plug is reversible. A PD trigger board asks for more, up to 20 V, which would destroy a 5 V circuit.

**Bypass capacitors belong at the chip.** A capacitor helps only the chip it sits next to; the trace between a shared capacitor and a distant chip has enough inductance to defeat it.

## Process

- **Isolate before integrating.** Proving the FFT on generated data and the driver from a potentiometer meant each fault showed which chain it was in.
- **Integrate before final assembly.** I proved each block alone, but built the final stack before the whole system had run together, so the microphone's limits were found when they could only be worked around.
- **After rewiring, check every pin carries only what it should.** Two separate faults came from leftover wires of an earlier setup.
- **Test with the real source at the real distance.** A point-blank cough clipped the ADC; music at the listening position never did.
- **Measure what the device does, then tune by eye.** The goal was how the bars look, so the final constants were set by watching the bars on one song at several distances.
