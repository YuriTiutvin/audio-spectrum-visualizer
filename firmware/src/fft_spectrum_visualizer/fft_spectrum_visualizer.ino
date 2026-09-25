// ============================================================================
//  3-band audio visualizer  -  ESP32 + MAX4466 + 3x LM3914
//  v4 (tuned) - cleaned up.
//
//  mic -> ADC -> FFT -> 3 band levels (dB) -> auto-scaled window -> 3 PWM bars
//
//  All tuning values are dB, dB/s or seconds (ratios), so they don't depend on
//  the gain pot, the room, or how loud the music is.
// ============================================================================

#include <arduinoFFT.h>
#include "driver/adc.h"

// ---------------------------------------------------------------- hardware
#define N 1024                               // FFT length
const int PWM_PIN[3] = { 27, 26, 25 };       // bass, mid, treble
const int PWM_FREQ = 10000, PWM_RES = 10;    // duty 0..1023

// The loop asks for 32 kHz, but adc1_get_raw() takes ~43 us, so it really runs
// at ~23.1 kHz (measured below). Don't touch the capture loop: its timing sets
// the sample rate, and the sample rate sets which treble folds into the band.
const float TARGET_FS = 32000.0f;

// ---------------------------------------------------------------- bands (Hz)
const float BAND_LO[3] = {  80.0f,  300.0f,  4000.0f };
const float BAND_HI[3] = { 300.0f, 4000.0f, 10000.0f };

// ---------------------------------------------------------------- tuning
//                              bass   mid   treble
const float GATE_DB[3]   = {   6.0f, 4.0f,  2.0f };  // dB over noise floor before a bar lights
const float SPAN_MIN[3]  = {   6.0f, 5.0f,  3.5f };  // narrowest window (dB)
const float SPAN_MAX[3]  = {  10.0f, 12.0f, 26.0f }; // widest window, down from the peak (dB)
const float FALL_DB_S[3] = {  55.0f, 55.0f, 30.0f }; // how fast a bar falls (dB/s)
const float GAMMA[3]     = {   1.0f, 1.0f,  0.75f }; // <1 lifts the middle of the bar

const float HEADROOM_DB = 2.0f;    // window top sits this far above the peak
const float SMOOTH_TAU  = 0.060f;  // s, level smoothing
const float PEAK_TAU    = 0.040f;  // s, how fast the window top chases a peak
const float PEAK_DOWN   = 2.5f;    // dB/s the window top relaxes
const float FLOOR_DOWN  = 3.0f;    // dB/s the noise floor may fall
const float FLOOR_UP_R  = 0.06f;   // floor rise rate, as a fraction of FLOOR_DOWN
const float STUCK_S = 12.0f, HOT_LO = 0.80f, HOT_LEAK = 4.0f;  // stale-floor escape

// ---------------------------------------------------------------- state
float vReal[N], vImag[N];
ArduinoFFT<float> FFT(vReal, vImag, N, TARGET_FS);
float gFs = TARGET_FS, gDt = 0.04f;
float sm[3], flr[3], pk[3], lvl[3], hot[3], bot[3], top[3];
bool  seeded = false;
uint32_t tPrev = 0, tPrint = 0;

// ---------------------------------------------------------------- capture
void captureSpectrum() {
  const float usPerSample = 1e6f / TARGET_FS;
  uint32_t t0 = micros();
  for (int i = 0; i < N; i++) {
    uint32_t tNext = t0 + (uint32_t)(i * usPerSample);
    while ((int32_t)(micros() - tNext) < 0) { }
    vReal[i] = (float)adc1_get_raw(ADC1_CHANNEL_6);   // GPIO34
    vImag[i] = 0.0f;
  }
  gFs = (float)(N - 1) * 1e6f / (float)(micros() - t0);   // the real rate

  float mean = 0;                                         // remove DC
  for (int i = 0; i < N; i++) mean += vReal[i];
  mean /= N;
  for (int i = 0; i < N; i++) vReal[i] -= mean;

  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  for (int k = 1; k < N / 2; k++)                         // power per bin
    vReal[k] = vReal[k] * vReal[k] + vImag[k] * vImag[k];
}

void setup() {
  Serial.begin(115200);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  for (int b = 0; b < 3; b++) ledcAttach(PWM_PIN[b], PWM_FREQ, PWM_RES);
  delay(300);
  tPrev = micros();
}

void loop() {
  captureSpectrum();
  uint32_t now = micros();
  gDt = (now - tPrev) * 1e-6f;
  tPrev = now;
  if (gDt < 0.005f || gDt > 0.5f) gDt = 0.04f;

  // ---- band level (dB): the mean of the bins above the band's own mean,
  // so a narrow sound isn't diluted across a wide band
  float db[3];
  for (int b = 0; b < 3; b++) {
    int k0 = (int)(BAND_LO[b] * N / gFs) + 1;
    int k1 = (int)(BAND_HI[b] * N / gFs);
    if (k1 > N / 2 - 1) k1 = N / 2 - 1;
    float sum = 0;
    for (int k = k0; k <= k1; k++) sum += vReal[k];
    float mu = sum / (float)(k1 - k0 + 1);
    float s2 = 0; int c2 = 0;
    for (int k = k0; k <= k1; k++) if (vReal[k] > mu) { s2 += vReal[k]; c2++; }
    float p = c2 ? s2 / (float)c2 : mu;
    db[b] = 10.0f * log10f(p > 1e-6f ? p : 1e-6f);
  }

  if (!seeded) {
    for (int b = 0; b < 3; b++) { sm[b] = flr[b] = pk[b] = lvl[b] = db[b]; hot[b] = 0; }
    seeded = true;
  }

  const float aSm = 1.0f - expf(-gDt / SMOOTH_TAU);
  const float aPk = 1.0f - expf(-gDt / PEAK_TAU);
  const float down = FLOOR_DOWN * gDt;
  int duty[3];

  for (int b = 0; b < 3; b++) {
    sm[b] += (db[b] - sm[b]) * aSm;

    // noise floor: steps down when below it, creeps up only on silent frames
    if (sm[b] < flr[b])                   flr[b] -= down;
    else if (sm[b] < flr[b] + GATE_DB[b]) flr[b] += down * FLOOR_UP_R;
    else if (hot[b] > STUCK_S / gDt)      flr[b] += down;

    // window: bottom = floor + gate, top = peak + headroom, width clamped
    if (sm[b] > pk[b]) pk[b] += (sm[b] - pk[b]) * aPk;
    else               pk[b] -= PEAK_DOWN * gDt;
    bot[b] = flr[b] + GATE_DB[b];
    top[b] = pk[b] + HEADROOM_DB;
    if (top[b] - bot[b] > SPAN_MAX[b]) bot[b] = top[b] - SPAN_MAX[b];
    if (top[b] - bot[b] < SPAN_MIN[b]) top[b] = bot[b] + SPAN_MIN[b];

    // bar: jumps up instantly, glides down
    if (sm[b] > lvl[b]) lvl[b] = sm[b];
    else { lvl[b] -= FALL_DB_S[b] * gDt; if (lvl[b] < sm[b]) lvl[b] = sm[b]; }

    float pos = (lvl[b] - bot[b]) / (top[b] - bot[b]);
    if (pos > 0.0f && pos < 1.0f) pos = powf(pos, GAMMA[b]);
    duty[b] = (int)(pos <= 0 ? 0 : (pos >= 1 ? 1023 : pos * 1023.0f));
    ledcWrite(PWM_PIN[b], duty[b]);

    if (pos >= 1.0f)         hot[b] += 1.0f;
    else if (pos < HOT_LO) { hot[b] -= HOT_LEAK; if (hot[b] < 0) hot[b] = 0; }
  }

  // bar is dark when lvl < bot, full when lvl > top
  if (millis() - tPrint >= 200) {
    tPrint = millis();
    Serial.printf("fs%5.0f %4.1ffps |", gFs, 1.0f / gDt);
    for (int b = 0; b < 3; b++)
      Serial.printf(" %c lvl%5.1f bot%5.1f top%5.1f d%4d |",
                    "BMT"[b], lvl[b], bot[b], top[b], duty[b]);
    Serial.println();
  }
}