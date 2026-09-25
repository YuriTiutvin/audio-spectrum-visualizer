# Firmware — build and flash

Two Arduino sketches, each in its own folder as the Arduino IDE requires.

| Sketch | What it is |
|---|---|
| [src/fft_spectrum_visualizer](src/fft_spectrum_visualizer/fft_spectrum_visualizer.ino) | the firmware running on the device |
| [src/adc_readout](src/adc_readout/adc_readout.ino) | a bench tool that prints raw ADC minimum, maximum and midpoint; used to set the microphone gain and to measure the ADC curve |

How the main sketch works is in [notes.md](notes.md).

## Requirements

- Arduino IDE 2.3.10
- Board support: esp32 by Espressif Systems, version 3.3.11 (Boards Manager)
- Library: arduinoFFT, version 2.0.4 (Library Manager)
- An ESP32-DevKitC-32UE, or any classic ESP32 (WROOM-32 family) board. Not an ESP32-C3, C6 or S2: those have no hardware floating-point unit, and the pin numbers differ.

The sketch uses the core-3.x LEDC calls (`ledcAttach`, `ledcWrite`), so an esp32 package older than 3.0 will not compile it. It also uses the legacy `driver/adc.h` ADC interface, which compiles on 3.x with a deprecation warning.

## Build and flash

1. Install the board package and arduinoFFT.
2. Open `src/fft_spectrum_visualizer/fft_spectrum_visualizer.ino`.
3. Tools → Board → esp32 → **ESP32 Dev Module**. Leave the other settings at their defaults.
4. Connect the board with the DevKit's own USB port and choose its serial port. **Disconnect the USB-C power input first**; the two supplies must not be live at the same time.
5. Upload.

## Run

Power the device from its USB-C input and play music. Nothing needs calibrating. The bars settle on the room within a few seconds; if they stay stuck after power-on, press **EN** on the ESP32 to restart.

To watch what it is doing, open Serial Monitor at **115200 baud** while it runs from the DevKit's USB port. Every 200 ms it prints the measured sample rate, the frame rate, and for each band the level, the window bottom and top, and the PWM duty:

```
fs23118 19.6fps | B lvl 78.3 bot 71.0 top 81.0 d 746 | M lvl 60.3 bot 59.6 top 71.6 d  54 | T lvl 48.6 bot 49.1 top 55.9 d   0 |
```

A bar is dark while `lvl` is below `bot` and full when `lvl` is above `top`.

## Setting the microphone gain

Flash `src/adc_readout`, open Serial Monitor at 115200 baud, and play the loudest material you expect at the real listening position. Adjust the gain trimmer until the `LATCH` range stays clear of 0 and 4095; any key resets it. Then reflash the main sketch.
