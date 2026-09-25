# Interconnect

Every connection in the built device, confirmed against the perfboard. Reference designators match [bom.csv](bom.csv). ESP32 pins are written as GPIO numbers as printed on the DevKit; driver pins are the 18-pin DIP pin numbers.

## Reference designators

| Ref | Part | Band |
|---|---|---|
| U1 | ESP32-DevKitC-32UE | — |
| MK1 | Adafruit MAX4466 microphone board | — |
| J1 | Adafruit USB-C breakout (4090) | — |
| U2, U3, U4 | bar-graph driver marked "LM3915N-1" (measured linear) | bass, mid, treble |
| DS1, DS2, DS3 | 10-segment LED bar | bass, mid, treble |
| R1, R2, R3 | 2.2 kΩ LED-current resistor | bass, mid, treble |
| R4, R5, R6 | 10 kΩ PWM filter resistor | bass, mid, treble |
| C1, C2, C3 | 1 µF PWM filter capacitor | bass, mid, treble |
| C4, C5, C6 | 10 µF driver bypass capacitor | bass, mid, treble |

## Band mapping

| Band | ESP32 pin | Filter | Driver | LED bar | Colour |
|---|---|---|---|---|---|
| Bass (80–300 Hz) | GPIO27 | R4, C1 | U2 | DS1 | red (XGURX10D) |
| Mid (300 Hz–4 kHz) | GPIO26 | R5, C2 | U3 | DS2 | yellow (XGUYX10D) |
| Treble (4–10 kHz) | GPIO25 | R6, C3 | U4 | DS3 | green (XGUGX10D) |

From the front, left to right: red (bass), yellow (mid), green (treble).

## Microphone and ADC

| Signal | From | To | Net | Notes |
|---|---|---|---|---|
| Audio in | MK1 OUT | U1 GPIO34 (ADC1 channel 6) | AUDIO | direct, no coupling capacitor; input-only pin |
| Mic supply | U1 3V3 | MK1 VCC | 3V3 | 3.3 V keeps the output inside the ADC range |
| Mic ground | U1 GND | MK1 GND | GND_SIG | |

## One display channel (bass shown; mid and treble are identical with the parts in the band table)

| Signal | From | To | Net | Notes |
|---|---|---|---|---|
| PWM | U1 GPIO27 | R4 | PWM_B | 10 kHz, 10-bit |
| Filtered level | R4 / C1 node | U2 pin 5 (SIG) | SIG_B | C1 from the node to GND; fc = 15.9 Hz |
| Driver supply | 5 V | U2 pin 3 (V+) | 5V | C4, 10 µF, from pin 3 to GND at the chip |
| Driver ground | U2 pin 2 (V−) | ground point | GND_LED | |
| Full scale | U1 3V3 | U2 pin 6 (RHI) | 3V3 | 3.3 V only; not connected to pin 7 |
| Range bottom | U2 pin 4 (RLO) | ground point | GND_LED | |
| LED current set | U2 pin 7 (REF OUT) | R1 → U2 pin 8 (REF ADJ) → ground point | — | R1 spans REF OUT to ground; 2.2 kΩ, about 5.7 mA per LED |
| Mode | U2 pin 9 (MODE) | U2 pin 3 (V+) | 5V | bar mode |
| LED supply | 5 V | DS1 anodes, all ten | 5V | |
| LED sinks | DS1 cathodes | U2 LED outputs | — | LED 1 = pin 1, LED 2 = pin 18, LED 3 = pin 17 … LED 10 = pin 10; no series resistors |

## Power input

| Signal | From | To | Net | Notes |
|---|---|---|---|---|
| 5 V in | J1 VBUS | U1 5V pin | 5V | not used at the same time as the DevKit's own USB port |
| Ground | J1 GND | ground point | GND | |
| CC1 | J1 CC1 | 5.1 kΩ to GND | — | fitted on the breakout |
| CC2 | J1 CC2 | 5.1 kΩ to GND | — | fitted on the breakout |
| D+ / D− | J1 | not connected | — | |

## Grounding

| Return | Route |
|---|---|
| Driver and LED current (GND_LED) | its own wire to the single ground point |
| Microphone and ESP32 (GND_SIG) | a separate wire to the same point |
| Microphone signal | short, and routed away from the three PWM wires |

The single ground point is on the back of the back board. The two returns meet only there, so LED current, which switches with the music, never flows through the microphone's ground wire.
