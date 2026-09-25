#include "driver/adc.h"

const adc1_channel_t MIC_CH = ADC1_CHANNEL_6;   // GPIO34, same as your main sketch
const uint32_t WINDOW_MS   = 100;

int latchMin = 4095;
int latchMax = 0;

void setup() {
  Serial.begin(115200);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(MIC_CH, ADC_ATTEN_DB_11);
  delay(200);
  Serial.println("MAX4466 gain tool. Play your LOUDEST source.");
  Serial.println("Target LATCH: ~750..3350, never 0 or 4095. Send a key to reset latch.");
}

void loop() {
  int winMin = 4095, winMax = 0;
  uint32_t n = 0, t0 = millis();

  while (millis() - t0 < WINDOW_MS) {
    int s = adc1_get_raw(MIC_CH);
    if (s < winMin) winMin = s;
    if (s > winMax) winMax = s;
    n++;
  }

  if (winMin < latchMin) latchMin = winMin;
  if (winMax > latchMax) latchMax = winMax;

  int mid = (winMin + winMax) / 2;
  const char* flag = (latchMin <= 5 || latchMax >= 4090) ? "  <<< CLIPPING!" : "";

  Serial.printf("win[%4d..%4d] mid=%4d | LATCH[%4d..%4d] pp=%4d (%lu smp)%s\n",
                winMin, winMax, mid, latchMin, latchMax, latchMax - latchMin, (unsigned long)n, flag);

  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    latchMin = 4095; latchMax = 0;
    Serial.println("--- latch reset ---");
  }
}
