#include <Servo.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

typedef struct {
  byte hour;
  byte minute;
} TimePoint;

bool teq(TimePoint t1, TimePoint t2) {
  return t1.hour == t2.hour && t1.minute == t2.minute;
}
bool tge(TimePoint t1, TimePoint t2) {
  return t1.hour*60+t1.minute >= t2.hour*60+t2.minute;
}

//---------------------Global Vars--------------------------//
const byte n = 4;
const TimePoint schedule[n] = {{8, 0}, {12, 0}, {16, 0}, {20, 0}};
byte ai;

// WiFi
const char *ssid = "LOGIN";
const char *password = "PASSWORD";
const long utc_offset = 10800;

const unsigned long ntp_timeout = 30000;
unsigned long last_request = 0;
TimePoint cur_time;

WiFiUDP ntp_udp;
NTPClient time_client(ntp_udp, "192.168.1.1", utc_offset);

// LED
const byte strip_pin = D2;
const byte led_states[][3] = {{0, 0, 0}, {255, 255, 255}};
CRGB strip[n];

// Button
const byte button_pin = A0;
const unsigned long button_timeout = 1000;
unsigned long last_press = 0;
const int button_threshold = 800;


// Servos
const byte servo_pins[n] = {D6, D7, D4, D5};
Servo servos[n];

const byte closed[n] = {5, 0, 8, 5};
const byte opened[n] = {110, 105, 100, 100};

// Buzzer
const byte buzzer_pin = D3;

//----------------------WiFi functions----------------------//

void wifi_connect(const char *ssid, const char *password) {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    tone(buzzer_pin, 200, 250);
    delay(500);
  }
}

void get_time(NTPClient time_client) {
  if (millis() - last_request > ntp_timeout || last_request == 0) {
    time_client.update();
    cur_time.hour = time_client.getHours();
    cur_time.minute = time_client.getMinutes();
    last_request = millis();
  }
}

//------------------LED strip functions---------------------//
void set_strip_color(const byte *color) {
  for (byte i = 0; i < n; i++) {
    strip[i].setRGB(color[0], color[1], color[2]);
    FastLED.show();
  }
}

//-------------------Button functions----------------------//
bool is_button_pressed() {
  bool result = false;
  if (analogRead(button_pin) > button_threshold && millis() - last_press > button_timeout ) {
    result = true;
    last_press = millis();
  }
  return result;
}

//-------------------Servos functions----------------------//
void servo_slow_set(Servo s, byte cur_angle, byte new_angle, byte delay_ms = 10) {
  if (new_angle > cur_angle) {
    for (int i = cur_angle; i <= new_angle; i++) {
      s.write(i);
      delay(delay_ms);
    }
  } else {
    for (int i = cur_angle; i >= new_angle; i--) {
      s.write(i);
      delay(delay_ms);
    }
  }
}

//------------------------Logic functions------------------//

byte find_closest_index(TimePoint cur_time) {
  byte i = 0;
  while (i < n && tge(cur_time, schedule[i]))
    i++;
  return i%n;
}


void alarm() {
  unsigned long alarm_start = millis();
  const unsigned long alarm_length = 1000*60*60;
  const unsigned long active_time = 3000;
  const unsigned long pause_time = 1000;
  unsigned long last_alarm_event = alarm_start;
  bool alarm_active = false;
  while (!is_button_pressed() && millis() - alarm_start < alarm_length) {
    delay(100);
    yield();
    if (alarm_active && millis() - last_alarm_event > active_time) {
      alarm_active = false;
      last_alarm_event = millis();
      set_strip_color(led_states[false]);
      noTone(buzzer_pin);
    } 
    if (!alarm_active && millis() - last_alarm_event > pause_time ) {
      alarm_active = true;
      last_alarm_event = millis();
      set_strip_color(led_states[true]);
      tone(buzzer_pin, 400, active_time);
    }
  }
  set_strip_color(led_states[false]);
  noTone(buzzer_pin);
}


//---------------------------------------------------------//


void setup() {
  Serial.begin(9600);
  wifi_connect(ssid, password);
  // При закрытии делаем паузу, потому что при одновременном закрытии можно перегрузить ардуино по питанию
  for (byte i = 0; i < n; i++) {
    servos[i].attach(servo_pins[i]);
    servos[i].write(closed[i]);
    delay(500);
  }

  pinMode(buzzer_pin, OUTPUT);
  pinMode(strip_pin, OUTPUT);
  pinMode(button_pin, INPUT);

  FastLED.addLeds<WS2812, strip_pin, GRB>(strip, 4).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(255);
  set_strip_color(led_states[false]);

  get_time(time_client);
  ai = find_closest_index(cur_time);
}

void loop() {
  get_time(time_client);
  delay(100);
  yield();
  if (teq(cur_time, schedule[ai])) {
    servo_slow_set(servos[ai], closed[ai], opened[ai]);

    alarm();

    servo_slow_set(servos[ai], opened[ai], closed[ai]);
    ai = (ai+1)%n;
  }
  if (is_button_pressed()){
    servo_slow_set(servos[ai], closed[ai], opened[ai]);
    delay(500);
    servo_slow_set(servos[ai], opened[ai], closed[ai]);
    ai = (ai+1)%n;
  }
}