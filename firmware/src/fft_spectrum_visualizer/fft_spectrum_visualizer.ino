#include <arduinoFFT.h>
#include "driver/adc.h"

#define N 1024
const double TARGET_FS   = 32000.0;
const double usPerSample = 1e6 / TARGET_FS;
#define MIC_CH ADC1_CHANNEL_6
const double BASS_EDGE = 260.0;
const double MID_EDGE  = 4000.0;

// signal-processing knobs (from Stage 3)
const double ATTACK  = 0.20;
const double FLOOR_A = 0.01;
const double GATE    = 1.80;

// --- display output ---
const int PWM_BASS = 25, PWM_MID = 26, PWM_TREB = 27;   // one PWM pin per band
const int PWM_FREQ = 10000;   // 10 kHz
const int PWM_RES  = 10;      // 10-bit -> duty 0..1023
// per-band full-scale: the band level that should fill the bar (TUNE THESE)
double BASS_MAX = 3000, MID_MAX = 3000, TREB_MAX = 3000;

double vReal[N], vImag[N];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, N, TARGET_FS);

double binFloor[N / 2];
double sBass = 0, sMid = 0, sTreble = 0;
double gFs;
bool seeded = false;
uint32_t lastPrint = 0;

void captureSpectrum() {
  uint32_t tStart = micros();
  for (int i = 0; i < N; i++) {
    uint32_t tNext = tStart + (uint32_t)(i * usPerSample);
    while ((int32_t)(micros() - tNext) < 0) { }
    vReal[i] = adc1_get_raw(MIC_CH);
    vImag[i] = 0.0;
  }
  uint32_t tEnd = micros();
  gFs = (double)N * 1e6 / (double)(tEnd - tStart);

  double mean = 0;
  for (int i = 0; i < N; i++) mean += vReal[i];
  mean /= N;
  for (int i = 0; i < N; i++) vReal[i] -= mean;

  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
}

void setup() {
  Serial.begin(115200);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(MIC_CH, ADC_ATTEN_DB_11);
  ledcAttach(PWM_BASS, PWM_FREQ, PWM_RES);
  ledcAttach(PWM_MID,  PWM_FREQ, PWM_RES);
  ledcAttach(PWM_TREB, PWM_FREQ, PWM_RES);
  delay(500);
}

void loop() {
  captureSpectrum();

  int bassHi = (int)(BASS_EDGE * N / gFs);
  int midHi  = (int)(MID_EDGE  * N / gFs);
  int nyq    = N / 2 - 1;

  double bass = 0, mid = 0, treble = 0;
  for (int k = 1; k <= nyq; k++) {
    double m = vReal[k];
    if (!seeded) binFloor[k] = m;
    binFloor[k] += (m - binFloor[k]) * FLOOR_A;
    double clean = m - binFloor[k] * GATE;
    if (clean < 0) clean = 0;
    if      (k <= bassHi) bass   += clean;
    else if (k <= midHi)  mid    += clean;
    else                  treble += clean;
  }
  seeded = true;

  sBass   += (bass   - sBass)   * ATTACK;
  sMid    += (mid    - sMid)    * ATTACK;
  sTreble += (treble - sTreble) * ATTACK;

  // map each band level -> PWM duty (0..1023), clamped
  int dBass = (int)constrain(sBass   / BASS_MAX * 1023.0, 0, 1023);
  int dMid  = (int)constrain(sMid    / MID_MAX  * 1023.0, 0, 1023);
  int dTreb = (int)constrain(sTreble / TREB_MAX * 1023.0, 0, 1023);
  ledcWrite(PWM_BASS, dBass);
  ledcWrite(PWM_MID,  dMid);
  ledcWrite(PWM_TREB, dTreb);

  uint32_t now = millis();
  if (now - lastPrint >= 100) {
    lastPrint = now;
    Serial.print("bass="); Serial.print(sBass, 0); Serial.print("(d"); Serial.print(dBass); Serial.print(")");
    Serial.print(" mid="); Serial.print(sMid, 0); Serial.print("(d"); Serial.print(dMid); Serial.print(")");
    Serial.print(" treble="); Serial.print(sTreble, 0); Serial.print("(d"); Serial.print(dTreb); Serial.println(")");
  }
}