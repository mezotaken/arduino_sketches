#include "FastLED.h"
#define HOLD_TIME 1500

// Параметры для главного и вспомогательного мониторов
const byte num_leds_main = 136;
const byte pin_main = 12;
CRGB leds_main[num_leds_main];

const byte num_leds_aux = 114;
const byte pin_aux = 9;
CRGB leds_aux[num_leds_aux];

const byte led_states[][3] = {{0, 0, 0}, {255, 214, 170}};

// Переменные для протокола передачи
byte mc = 0;
bool upd = false;
byte cur_color[3];

// Параметры для обработки кнопок и реле

const byte pin_util_button = 3;
const byte pin_led_button = 5;
const byte lamp_pin = A5;
const byte charge_pin = A3;

// Переменные для обработки кнопок
bool util_button_ps = false;
bool led_button_ps = false;

bool util_button_hold_triggered = false;
bool led_button_hold_triggered = false;

unsigned long util_button_start = 0;
unsigned long led_button_start = 0;

bool lamp_sw = false;
bool charge_sw = false;
bool main_led_sw = false;
bool aux_led_sw = false;


// Для быстрой установки одного цвета на всю ленту
void set_strip_color(byte n, CRGB *strip, byte *color) {
  for (int i=0;i<n;i++) {
    strip[i].setRGB(color[0], color[1], color[2]);
  }
  FastLED.show();
}


// Обработка одной сенсорной кнопки, возвращает 0, если ничего не произошло;
// 1 - сработало короткое нажатие; 2 - сработало удержание. Должно вызываться в loop()
byte process_button(byte pin, bool *prev_st, unsigned long *start, bool *hold_triggered) {
  byte cur_st = digitalRead(pin);
  if (cur_st == true and *prev_st == false) {
    *start = millis();
    *prev_st = true;
  }
  if (cur_st == true and *prev_st == true and millis() - *start >= HOLD_TIME) {
    *prev_st = false;
    *hold_triggered = true;
    return 2;
  }
  if (cur_st == false and *prev_st == true) {
    *prev_st = false;
    if (*hold_triggered == true) {
      *hold_triggered = false;
      return 0;
    } else {
      return 1;
    }
  }
  return 0;
}


void setup() {
  Serial.begin(115200); 
  Serial.setTimeout(1); 
  pinMode(pin_main, OUTPUT);
  FastLED.addLeds<WS2812, pin_main, GRB>(leds_main, num_leds_main).setCorrection(TypicalLEDStrip);
  pinMode(pin_aux, OUTPUT);
  FastLED.addLeds<WS2812, pin_aux, GRB>(leds_aux, num_leds_aux).setCorrection(TypicalLEDStrip);
  pinMode(pin_util_button, INPUT);
  pinMode(pin_led_button, INPUT);
  pinMode(lamp_pin, OUTPUT);
  pinMode(charge_pin, OUTPUT);

  FastLED.setBrightness(255);

  set_strip_color(num_leds_main, leds_main, led_states[false]);
  set_strip_color(num_leds_aux, leds_aux, led_states[false]);
}


void loop() {
  if (Serial.available() >= 3) {
    Serial.readBytes(cur_color, 3);
    mc++;
    upd = true;
  }
  if (upd && mc < num_leds_main/2) {
    upd = false;
    leds_main[mc*2].setRGB(cur_color[0], cur_color[1], cur_color[2]);
    if (mc != 0) {
      leds_main[mc*2 - 1].setRGB((cur_color[0]+leds_main[(mc-1)*2].raw[0])/2, (cur_color[1]+leds_main[(mc-1)*2].raw[1])/2, (cur_color[2]+leds_main[(mc-1)*2].raw[2])/2);
    }
    if (mc == num_leds_main/2 - 1) {
      leds_main[mc*2 + 1].setRGB((cur_color[0]+leds_main[0].raw[0])/2, (cur_color[1]+leds_main[0].raw[1])/2, (cur_color[2]+leds_main[0].raw[2])/2);
    }
  }
  if (mc == num_leds_main/2 - 1) {
    mc = -1;
    FastLED.show();
  }


  byte util_res = process_button(pin_util_button, &util_button_ps, &util_button_start, &util_button_hold_triggered);
  byte led_res = process_button(pin_led_button, &led_button_ps, &led_button_start, &led_button_hold_triggered);
  if (led_res == 1) {
    main_led_sw = !main_led_sw;
    set_strip_color(num_leds_main, leds_main, led_states[main_led_sw]);
  } else if (led_res == 2) {
    aux_led_sw = !aux_led_sw;
    set_strip_color(num_leds_aux, leds_aux, led_states[aux_led_sw]);
  }


  if (util_res == 1) {
    lamp_sw = !lamp_sw;
    digitalWrite(lamp_pin, lamp_sw);
  } else if (util_res == 2) {
    charge_sw = !charge_sw;
    digitalWrite(charge_pin, charge_sw);
  }
}
