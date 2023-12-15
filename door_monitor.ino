#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include "./wifi.h"
#include "./open_door.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
// In the above wifi.h file, place
//  const char* ssid = "SSID";
//  const char* password = "PASS";
// Also for the OTA update!
//  const char* ota_user = "USER";
//  const char* ota_pass = "PASS";
// Leds are active LOW
constexpr int ledR = 16;
constexpr int ledG = 17;
constexpr int ledB = 18;

constexpr int pirIn = 21;
// OTA update inclues
#include <ESPAsyncWebSrv.h>
#include <Update.h>
// Code to handle updating OTA.
#include "allow_update.h"

// Pinout 
//  PIR:  GND to gnd, 3.3 to 3.3, signal to 21.
//  Switch:  one wire to 15, one wire to gnd. Wire to common and normally open.


// Use ESP Dev Module.


const gpio_num_t switch_pin_a = GPIO_NUM_15;
// const int switch_pin_b = 39; // Only use one pin for now.
const int enable_OTA_pin = 4;

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int last_sleep_sought_state = -1;

bool enter_ota_mode() {
  return digitalRead(enable_OTA_pin) == HIGH;
}



void go_to_sleep(int look_for) {
  Serial.println("Sleeping!");
  last_sleep_sought_state = look_for;
  // Call this before sleep to lower power consumption.
  // See https://electronics.stackexchange.com/questions/530151/esp32-wroom32-consuming-77-%C2%B5a-much-too-high-in-deep-sleep
  gpio_reset_pin(GPIO_NUM_2);

  // Set wakeup conditions
  rtc_gpio_pullup_en(switch_pin_a);
  rtc_gpio_pulldown_dis(switch_pin_a);
  esp_sleep_enable_ext0_wakeup(switch_pin_a, look_for);
  // Sleep!
  esp_deep_sleep_start();
}

void setup() {
  
  bool first_boot = (bootCount == 0);
  bootCount += 1;
  // put your setup code here, to run once:
  rtc_gpio_deinit(GPIO_NUM_25);  // Enable using RTC IO as GPIO again, see https://docs.espressif.com/projects/esp-idf/en/v4.4.4/esp32s3/api-reference/system/sleep_modes.html#external-wakeup-ext0
  Serial.begin(115200);
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledB, OUTPUT);
  pinMode(switch_pin_a, INPUT_PULLUP);
  pinMode(enable_OTA_pin, INPUT_PULLDOWN);  // This is only checked while awake so this is fine.
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);  // Just use this pin as a gnd for the ota jumper lol.
  pinMode(pirIn, INPUT);
  digitalWrite(ledR, HIGH);
  digitalWrite(ledG, HIGH);
  digitalWrite(ledB, HIGH);

  Serial.println("Boot up!");
  if (first_boot) {
    // delay for PIR sensor boot.
    Serial.println("Booting,delaying for pir.");
    digitalWrite(ledG, LOW);
    delay(60000);
    Serial.println("Done delaying.");
    digitalWrite(ledG, HIGH);
  }

  Serial.println("Switch:");
  Serial.println(digitalRead(switch_pin_a));

  // Nothing to do on first boot, so normally go right to sleep.
  if (first_boot && !enter_ota_mode()) { go_to_sleep(LOW); }

  // We went to sleep while the switch was low and awaiting the door to be relocked/switch to open.
  if (last_sleep_sought_state == HIGH) {
    // debounce
    delay(20);
    // if the switch is open, enter normal sleep.
    if (digitalRead(switch_pin_a) == HIGH) {
      Serial.println("Last sleep was with switch closed, switch is now open so resuming sleep until switch closed once more.");
      go_to_sleep(LOW);
    } else {
      // Otherwise switch is still closed for some reason, return to high sleep.
      go_to_sleep(HIGH);
    }
  }

  Serial.println("Switch:");
  Serial.println(digitalRead(switch_pin_a));

  Serial.println("Woke up!");

  // WiFi can cause false readings, so take this reading ahead of time, now.
  int pirReading = digitalRead(pirIn);
  digitalWrite(ledR,LOW);
  for (int counter = 0; counter <= 500; counter += 5) {
    if (digitalRead(pirIn) == HIGH) { pirReading = HIGH; }
    delay(5);
  }
  digitalWrite(ledR,HIGH);

  const char *hostname = "door_monitor";
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Set the hostname: https://github.com/espressif/arduino-esp32/issues/3438
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);  // required to set hostname properly
  WiFi.setHostname(hostname);
  Serial.println("Connecting to wifi!");
  long wifi_time = millis();
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    // Upon entering Loop(), the ESP32 will be rebooted.
    return;
  } else {
    Serial.printf("WiFi tool this long to connect: %lu \n", millis() - wifi_time);
  }
  // We are on wifi now.
  // Enter OTA mode  pins are connected.
  // Do this before sleep!
  if (first_boot && enter_ota_mode()) {
    block_until_update();
  }

  // Debounce just to be sure...
  delay(20);
  if (digitalRead(switch_pin_a)) {
    // Switch is high, thus open.
    Serial.println("Debounce failed, switch is open, going return to sleep.");
    // Will wake again if switch is repressed.
    go_to_sleep(LOW);
  }
  // Otherwise proceed to normal door monitoring operation.
  if (pirReading == LOW) {
    Serial.println("No motion, so activating door!"); 
    digitalWrite(ledR,LOW);
    open_door();
    digitalWrite(ledR, HIGH);
  } else {
    digitalWrite(ledG, LOW);
    delay(100);
    Serial.println("Motion, so not activating!");
    digitalWrite(ledG, HIGH);
  }

  go_to_sleep(HIGH);
}


// Loop shouldn't run with all the sleeping will will be doing!
void loop() {}
