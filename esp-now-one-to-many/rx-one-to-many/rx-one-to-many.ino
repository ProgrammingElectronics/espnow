/**
 * @file rx-one-to-many.ino
 * @author Michael Cheich (micheal@programmingelectronics.com)
 * @brief espnow example for a NEOpixel controller.  One transmitter controls multiple receivers. This is the RX code.
 * @version 0.1
 * @date 2022-09-12
 *
 * @copyright Copyright (c) 2022
 *
 */

/*
  I didn't need these, but maybe they could come in handy... 
  #define FASTLED_ESP8266_RAW_PIN_ORDER
  #define FASTLED_ESP8266_NODEMCU_PIN_ORDER
  #define FASTLED_ESP8266_D1_PIN_ORDER  
*/
#include <FastLED.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <neo_pixel_controller_shared.h>

/* Pins
 You may have to change this based on the RX dev board type.
 pin 12 worked for me across the nodeMCU, Adafruit huzzah esp8266, and Wemos M1 clone 
 */
#define DATA_PIN 12  // NEOpixel data pin

// LED Effects
enum LED_effects { CHANGE_COLOR,
                   CYLON,
                   PACIFICA,
                   RANDOM_REDS };

#define FRAMES_PER_SECOND 120

// LED array
#define NUM_LEDS 12  // Adjust for different LED stip lengths
CRGB leds[NUM_LEDS];

struct neopixel_data data;

/*************************************************************
  IMPORTANT!  Ideally each RX device gets its own unique SSID.
              This way, in the list of SSIDs on the TX, you can 
              determine which is which.  When uploading to a 
              RX, change the names saved in SIID_NAMES array
              to match your devices, and change thisDeviceSSID 
              to the index of the name you want from the 
              SSID_NAMES.  You'll have to change thisDeviceSSID
              every time you upload to a different widget.

              TODO: Set this up so it can be done over WiFi
              with a phone app.
*************************************************************/
#define thisDeviceSSID 3
const String SSID_NAMES[MAX_PEERS] = {
  /* SSID names over 17 chars will be removed (in TX code) to fit on screen */
  /* 0 */ "Ada_1",
  /* 1 */ "Ada_2",
  /* 2 */ "NodeMCU",
  /* 3 */ "D1MiniClone"
  /*      "|<-Max 17 char->|" */
};

/*************************************************************
  ESPNOW Functions
*************************************************************/

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
bool configDeviceAP() {

  String Prefix = "RX_" + SSID_NAMES[thisDeviceSSID];
  String Mac = WiFi.macAddress();
  String SSID = Prefix;
  String Password = "123456789";
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);

  if (!result) {
    Serial.println("AP Config failed.");

  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
  }

  return result;
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

/*************************************************************
  Helper Functions
*************************************************************/

//Fade all LEDs
void fadeall() {

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(250);
  }
}

/*************************************************************
  Cylon Effect from FastLED library example code
*************************************************************/
void cyclon() {

  static uint8_t hue = 0;

  // First slide the led in one direction
  for (int i = 0; i < NUM_LEDS; i++) {
    // Set the i'th led to red
    leds[i] = CHSV(hue++, 255, 255);
    // Show the leds
    FastLED.show();
    // now that we've shown the leds, reset the i'th led to black
    fadeall();
    // Wait a little bit before we loop around and do it again
    delay(10);
  }

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

/*************************************************************
  Pacifica Effect used from FastLED Library example code
*************************************************************/

CRGBPalette16 pacifica_palette_1 = { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
                                     0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50 };
CRGBPalette16 pacifica_palette_2 = { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
                                     0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F };
CRGBPalette16 pacifica_palette_3 = { 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
                                     0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF };

// Add one layer of waves into the led array
void pacifica_one_layer(CRGBPalette16 &p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff) {

  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale / 2) + 20;

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    waveangle += 250;
    uint16_t s16 = sin16(waveangle) + 32768;
    uint16_t cs = scale16(s16, wavescale_half) + wavescale_half;
    ci += cs;
    uint16_t sindex16 = sin16(ci) + 32768;
    uint8_t sindex8 = scale16(sindex16, 240);
    CRGB c = ColorFromPalette(p, sindex8, bri, LINEARBLEND);
    leds[i] += c;
  }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps() {

  uint8_t basethreshold = beatsin8(9, 55, 65);
  uint8_t wave = beat8(7);

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t threshold = scale8(sin8(wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = leds[i].getAverageLight();

    if (l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8(overage, overage);
      leds[i] += CRGB(overage, overage2, qadd8(overage2, overage2));
    }
  }
}

// Deepen the blues and greens
void pacifica_deepen_colors() {

  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].blue = scale8(leds[i].blue, 145);
    leds[i].green = scale8(leds[i].green, 200);
    leds[i] |= CRGB(2, 5, 7);
  }
}

void pacifica_loop() {

  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  static uint32_t sLastms = 0;
  uint32_t ms = GET_MILLIS();
  uint32_t deltams = ms - sLastms;
  sLastms = ms;
  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011, 10, 13));
  sCIStart2 -= (deltams21 * beatsin88(777, 8, 11));
  sCIStart3 -= (deltams1 * beatsin88(501, 5, 7));
  sCIStart4 -= (deltams2 * beatsin88(257, 4, 6));

  // Clear out the LED array to a dim background blue-green
  fill_solid(leds, NUM_LEDS, CRGB(2, 6, 10));

  // Render each of four layers, with different scales and speeds, that vary over time
  pacifica_one_layer(pacifica_palette_1, sCIStart1, beatsin16(3, 11 * 256, 14 * 256), beatsin8(10, 70, 130), 0 - beat16(301));
  pacifica_one_layer(pacifica_palette_2, sCIStart2, beatsin16(4, 6 * 256, 9 * 256), beatsin8(17, 40, 80), beat16(401));
  pacifica_one_layer(pacifica_palette_3, sCIStart3, 6 * 256, beatsin8(9, 10, 38), 0 - beat16(503));
  pacifica_one_layer(pacifica_palette_3, sCIStart4, 5 * 256, beatsin8(8, 10, 28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  pacifica_add_whitecaps();

  // Deepen the blues and greens a bit
  pacifica_deepen_colors();
}

/*************************************************************
  Random Reds Effect
*************************************************************/
void randomReds() {

  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = random(0, NUM_LEDS);
  leds[pos] += CHSV(255, 255, 255);
}

/*************************************************************
  Change All Color Effect
*************************************************************/
void changeAllColor() {

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(data.hue, data.saturation, data.value);
  }
  FastLED.show();
}

/*************************************************************
  Setup
*************************************************************/

void setup() {

  Serial.begin(115200);

  WiFi.mode(WIFI_AP);

  //Make 10 attempts at device config
  char buffer[40];
  bool configSuccess = false;

  for (int i = 0; i < 10 && !configSuccess; i++) {
    sprintf(buffer, "\nAP Config attempt %d of %d", i + 1, 10);
    Serial.println(buffer);
    configSuccess = false;//configDeviceAP();
    delay(100);
  }

  // If unsuccessful config, hold here forever, else continue
  if (!configSuccess) {
    while (1) {;}
  }

  // This is the mac address of the Receiver in AP Mode
  Serial.print("AP MAC: ");
  Serial.println(WiFi.softAPmacAddress());

  // Init ESPNow with a fallback logic
  InitESPNow();

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_register_recv_cb(OnDataRecv);

  // FastLED setup stuff
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(84);

  //Set the first LED to Red as a visual indicator the code is loaded
  leds[0] = CHSV(255, 255, 255);
  FastLED.show();
}

/*************************************************************
  Loop
*************************************************************/
void loop() {

  static byte previousHue = 0;

  switch (data.effect) {

    case CHANGE_COLOR:

      if (data.hue != previousHue) {
        previousHue = data.hue;
        changeAllColor();
      }
      break;

    case CYLON:
      cyclon();
      break;

    case PACIFICA:
      EVERY_N_MILLISECONDS(20) {
        pacifica_loop();
        FastLED.show();
      }
      break;

    case RANDOM_REDS:
      EVERY_N_MILLISECONDS(20) {
        randomReds();
        FastLED.show();
      }
      break;
  }

  //Ensure that CHANGE COLOR EFFECT only runs when a new color is selected
  if (data.effect != CHANGE_COLOR) {
    previousHue = 255;
  }
}