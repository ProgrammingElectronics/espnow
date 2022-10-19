/* IMPORTANT-> This header file and enclosing folder must be placed in the ../Arduino/libraries folder */

#ifndef neo_pixel_controller_shared_h
#define neo_pixel_controller_shared_h

#define CHANNEL 1
#define MAX_PEERS 20

// This is data structure sent to each RX
struct neopixel_data {
  byte effect;
  bool display = true;
  byte hue = 42;
  byte saturation = 255;
  byte value = 255;
};

#endif