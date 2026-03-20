#include <Arduino.h>
#include "driver/rmt.h"

#define CH6_PIN 33
#define CH7_PIN 34
#define CH8_PIN 32

#define RELAY1 26
#define RELAY2 14
#define RELAY3 25
#define RELAY4 27

#define RMT_CLK_DIV 80   // 1 tick = 1us

rmt_channel_t channels[3] = {
  RMT_CHANNEL_0,
  RMT_CHANNEL_1,
  RMT_CHANNEL_2
};

RingbufHandle_t rb[3];

uint32_t pulseWidth[3] = {0,0,0};

bool relayState[4] = {0};
bool signalActive = false;

uint32_t lastSwitchTime = 0;
uint8_t lastMode = 1;
uint32_t lastSignalTime = 0;

// ===== RMT INIT =====
void setupRMT(gpio_num_t pin, rmt_channel_t ch, int idx) {
  rmt_config_t config = {};
  config.rmt_mode = RMT_MODE_RX;
  config.channel = ch;
  config.gpio_num = pin;
  config.clk_div = RMT_CLK_DIV;
  config.mem_block_num = 1;

  config.rx_config.filter_en = true;
  config.rx_config.filter_ticks_thresh = 50;
  config.rx_config.idle_threshold = 3000;

  rmt_config(&config);
  rmt_driver_install(ch, 1024, 0);

  rmt_get_ringbuf_handle(ch, &rb[idx]);
  rmt_rx_start(ch, true);  // 🔥 สำคัญ
}

// ===== อ่าน PWM =====
uint32_t readPWM(int idx) {
  size_t rx_size = 0;

  rmt_item32_t* item = (rmt_item32_t*)
      xRingbufferReceive(rb[idx], &rx_size, 10 / portTICK_PERIOD_MS);

  if (item) {
    uint32_t d0 = item[0].duration0;
    uint32_t d1 = item[0].duration1;

    vRingbufferReturnItem(rb[idx], (void*) item);

    // 🔥 เลือกค่าที่อยู่ในช่วง servo
    if (d0 > 900 && d0 < 2100) return d0;
    if (d1 > 900 && d1 < 2100) return d1;
  }

  return 0; // ❗ ไม่มีสัญญาณ
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);

  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  digitalWrite(RELAY4, LOW);

  setupRMT((gpio_num_t)CH6_PIN, channels[0], 0);
  setupRMT((gpio_num_t)CH7_PIN, channels[1], 1);
  setupRMT((gpio_num_t)CH8_PIN, channels[2], 2);
}

void loop() {

  pulseWidth[0] = readPWM(0);
  pulseWidth[1] = readPWM(1);
  pulseWidth[2] = readPWM(2);

  uint32_t ch6 = pulseWidth[0];
  uint32_t ch7 = pulseWidth[1];
  uint32_t ch8 = pulseWidth[2];

  // ===== signal valid =====
  bool signalValid =
      (ch6 > 900 && ch6 < 2100) &&
      (ch7 > 900 && ch7 < 2100) &&
      (ch8 > 900 && ch8 < 2100);

  if (signalValid) {
    lastSignalTime = millis();
  }

  // ===== failsafe =====
  if (millis() - lastSignalTime > 500) {
    signalActive = false;
  }

  // ===== ต้องขยับก่อน =====
  if (!signalActive && signalValid) {
    if (abs((int)ch6 - 1500) > 100 ||
        abs((int)ch7 - 1500) > 100 ||
        abs((int)ch8 - 1500) > 100) {
      signalActive = true;
    }
  }

  // ===== ยังไม่พร้อม =====
  if (!signalValid || !signalActive) {
    relayState[0] = 0;
    relayState[1] = 0;
    relayState[2] = 0;
    relayState[3] = 0;
  } else {

    // CH6
    if (ch6 > 1700) relayState[0] = 1;
    else if (ch6 < 1300) relayState[0] = 0;

    // CH7
    if (ch7 > 1700) relayState[1] = 1;
    else if (ch7 < 1300) relayState[1] = 0;

    // CH8
    uint8_t mode;
    if (ch8 < 1300) mode = 0;
    else if (ch8 > 1700) mode = 2;
    else mode = 1;

    if (mode != lastMode && millis() - lastSwitchTime > 200) {
      lastSwitchTime = millis();

      relayState[2] = 0;
      relayState[3] = 0;

      if (mode == 0) relayState[2] = 1;
      else if (mode == 2) relayState[3] = 1;

      lastMode = mode;
    }
  }

  // ===== write =====
  digitalWrite(RELAY1, relayState[0]);
  digitalWrite(RELAY2, relayState[1]);
  digitalWrite(RELAY3, relayState[2]);
  digitalWrite(RELAY4, relayState[3]);

  // debug
  Serial.printf("CH6:%d CH7:%d CH8:%d | R:%d,%d,%d,%d\n",
                ch6, ch7, ch8,
                relayState[0],
                relayState[1],
                relayState[2],
                relayState[3]);

  delay(20);
}