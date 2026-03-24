#include <Arduino.h>   // 🔥 ต้องมี
#include "motor_bts7960.h"
#include "driver/rmt.h"

// ===== INPUT PWM (RMT) =====
#define CH1_PIN 4
#define CH2_PIN 16

#define RMT_CLK_DIV 80

rmt_channel_t motor_channels[2] = {
  RMT_CHANNEL_3,
  RMT_CHANNEL_4
};

RingbufHandle_t motor_rb[2];
uint32_t motor_pwm[2] = {0,0};

uint32_t lastSignalTime_motor = 0;

// ===== BTS7960 =====
#define L_RPWM 18
#define L_LPWM 19
#define R_RPWM 21
#define R_LPWM 22

#define PWM_FREQ 20000
#define PWM_RES 8

#define CH_L_RPWM 0
#define CH_L_LPWM 1
#define CH_R_RPWM 2
#define CH_R_LPWM 3

// =========================
void setupRMT_motor(gpio_num_t pin, rmt_channel_t ch, int idx)
{
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

  rmt_get_ringbuf_handle(ch, &motor_rb[idx]);
  rmt_rx_start(ch, true);
}

// =========================
uint32_t readPWM_RMT_motor(int idx)
{
  size_t rx_size = 0;

  rmt_item32_t* item = (rmt_item32_t*)
    xRingbufferReceive(motor_rb[idx], &rx_size, 5 / portTICK_PERIOD_MS);

  if (item)
  {
    uint32_t d0 = item[0].duration0;
    uint32_t d1 = item[0].duration1;

    vRingbufferReturnItem(motor_rb[idx], (void*) item);

    if (d0 > 900 && d0 < 2100) return d0;
    if (d1 > 900 && d1 < 2100) return d1;
  }

  return 0;
}

// =========================
void drive(int left, int right)
{
  left  = constrain(left,  -255, 255);
  right = constrain(right, -255, 255);

  ledcWrite(CH_L_RPWM, left > 0 ? left : 0);
  ledcWrite(CH_L_LPWM, left < 0 ? -left : 0);

  ledcWrite(CH_R_RPWM, right > 0 ? right : 0);
  ledcWrite(CH_R_LPWM, right < 0 ? -right : 0);
}

// =========================
void motorSetup()
{
  // ===== INPUT =====
  pinMode(CH1_PIN, INPUT_PULLDOWN);
  pinMode(CH2_PIN, INPUT_PULLDOWN);

  // ===== PWM =====
  ledcSetup(CH_L_RPWM, PWM_FREQ, PWM_RES);
  ledcSetup(CH_L_LPWM, PWM_FREQ, PWM_RES);
  ledcSetup(CH_R_RPWM, PWM_FREQ, PWM_RES);
  ledcSetup(CH_R_LPWM, PWM_FREQ, PWM_RES);

  ledcAttachPin(L_RPWM, CH_L_RPWM);
  ledcAttachPin(L_LPWM, CH_L_LPWM);
  ledcAttachPin(R_RPWM, CH_R_RPWM);
  ledcAttachPin(R_LPWM, CH_R_LPWM);

  // ===== RMT =====
  setupRMT_motor((gpio_num_t)CH1_PIN, motor_channels[0], 0);
  setupRMT_motor((gpio_num_t)CH2_PIN, motor_channels[1], 1);
}

// =========================
void motorLoop()
{
  motor_pwm[0] = readPWM_RMT_motor(0);
  motor_pwm[1] = readPWM_RMT_motor(1);

  uint32_t ch1 = motor_pwm[0];
  uint32_t ch2 = motor_pwm[1];

  // ===== ตรวจสัญญาณ =====
  bool signalValid =
    (ch1 > 900 && ch1 < 2100) &&
    (ch2 > 900 && ch2 < 2100);

  if (signalValid)
    lastSignalTime_motor = millis();

  // ===== FAILSAFE =====
  if (millis() - lastSignalTime_motor > 300)
  {
    drive(0, 0); // 🔥 ตัดมอเตอร์ทันที
    return;
  }

  int throttle = 0;
  int steering = 0;

  // ===== logic =====
  if (ch1 > 1500) throttle = map(ch1, 1500, 2000, 0, 255);
  else if (ch1 < 1500) throttle = map(ch1, 1000, 1500, -255, 0);

  if (ch2 > 1500) steering = map(ch2, 1500, 2000, 0, 255);
  else if (ch2 < 1500) steering = map(ch2, 1000, 1500, -255, 0);

  // deadzone
  if (abs(throttle) < 20) throttle = 0;
  if (abs(steering) < 20) steering = 0;

  int left  = throttle + steering;
  int right = throttle - steering;

  drive(left, right);
}