/**
 * @file rx-one-to-many.ino
 * @author Michael Cheich (micheal@programmingelectronics.com)
 * @brief
 * @version 0.1
 * @date 2022-09-12
 *
 * @copyright Copyright (c) 2022
 *
 */
//#define FASTLED_ESP8266_RAW_PIN_ORDER
//#define FASTLED_ESP8266_NODEMCU_PIN_ORDER

//#define FASTLED_ESP8266_D1_PIN_ORDER
#include <FastLED.h>
#include <espnow.h>
#include <ESP8266WiFi.h>

#define CHANNEL 1

const byte SOLID_COLOR = 0;
const byte CYCLON = 1;
const byte PACIFICA = 2;
const byte RANDOM_REDS = 3;

// pins
// Adafruit Feather Huzzah - pin 12
const byte DATA_PIN = 12;

// LED array
const byte NUM_LEDS = 12;
CRGB leds[NUM_LEDS];

typedef struct neopixel_data {
  byte effect;
  bool display = true;
  byte hue = 100;
  byte saturation = 255;
  byte value = 255;
} neopixel_data;

// Where incoming data is stored
neopixel_data data;

// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ERR_OK) {
    Serial.println("ESPNow Init Success");
  } else {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}

// config AP SSID
void configDeviceAP() {
  //String Prefix = "RX_Ada_1:";
  //String Prefix = "RX_Ada_2:";
  //String Prefix = "RX_NodeMCU:";
  String Prefix = "RX_D1MiniClone:";
  String Mac = WiFi.macAddress();
  String SSID = Prefix;
  String Password = "123456789";
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
  }
}

// callback when data is recv from Transmitter
void OnDataRecv(uint8_t *mac_addr, uint8_t *dataIn, uint8_t data_len) {

  memcpy(&data, dataIn, sizeof data);

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Recv from: ");
  Serial.println(macStr);
  Serial.print("Last Packet Recv Data: ");
  Serial.println(data.hue);
  Serial.println("");
}

void fadeall() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(250);
  }
}

void cyclon() {
  static uint8_t hue = 0;
  Serial.print("x");
  // First slide the led in one direction
  for (int i = 0; i < NUM_LEDS; i++) {
    // Set the i'th led to red
    leds[i] = CHSV(hue++, 255, 255);
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }
  Serial.print("x");

  // Now go in the other direction.
  for (int i = (NUM_LEDS)-1; i >= 0; i--) {
    // Set the i'th led to red
    leds[i] = CHSV(hue++, 255, 255);
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);

  // Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  // configure device AP mode
  configDeviceAP();
  // This is the mac address of the Receiver in AP Mode
  Serial.print("AP MAC: ");
  Serial.println(WiFi.softAPmacAddress());
  Serial.print("SSID: ");
  Serial.println();
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_recv_cb(OnDataRecv);

  //FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  //FastLED.addLeds<WS2812,DATA_PIN,RGB>(leds,NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(84);

// This does not work... I don't know why?
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(255, 255, 255);
  }
  FastLED.show();

}

void loop() {
  static byte previousEffect = data.effect;
  static byte previousHue = data.hue;

  if (data.effect == SOLID_COLOR && data.hue != previousHue) {

    previousHue = data.hue;

    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(data.hue, data.saturation, data.value);
    }
    FastLED.show();
  }
}