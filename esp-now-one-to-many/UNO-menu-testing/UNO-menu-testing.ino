#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

const byte SELECTION_BUTTON = 2;
const byte MAX_SELECTIONS = 2;
const byte LINE_SPACING = 5; // space between each line
const byte MAIN_MENU_LENGTH = 3;
const byte SELECT_EFFECT_LENGTH = 4;
const char *MAIN_MENU_OPTIONS[MAIN_MENU_LENGTH] = {"1. List Peers", "2. ReScan", "3. Broadcast"};
const char *SEL_EFFECT_OPTIONS[SELECT_EFFECT_LENGTH] = {"1. Change Color", "2. Cyclon", "3. Pacifica", "4. Random Reds"};

// States
#define MAIN_MENU 0
#define LIST_PEERS 1
#define RESCAN 2
#define BROADCAST 3
#define SELECT_EFFECT 4
#define CHANGE_COLOR 5
// Main Menu
#define LIST_PEERS_SEL 0
#define RESCAN_SEL 1
#define BROADCAST_SEL 2
// Select Effect
#define CHANGE_COLOR_SEL 0
#define CYLON_SEL 1

// Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE); // High speed I2C

// Track user button presses
volatile byte currentSelection = 0;
volatile bool selectionMade = false;
// variables to keep track of the timing of recent interrupts
volatile unsigned long button_time = 0;
volatile unsigned long last_button_time = 0;

void incrementSelection()
{
  button_time = millis();

  if (button_time - last_button_time > 250)
  {
    currentSelection++;
    if (currentSelection > MAX_SELECTIONS)
    {
      currentSelection = 0;
    }
    Serial.println(currentSelection);
    last_button_time = button_time;
  }
}

/**
 * @brief Display main menue options OLED
 *
 */
void displayMenu(const char *menuArray[], byte len)
{
  u8g2.clearBuffer(); // clear the internal memory

  int spacing = LINE_SPACING + u8g2.getAscent() + abs(u8g2.getDescent());
  for (int i = 0; i < len; i++)
  {
    u8g2.drawButtonUTF8(0, spacing, currentSelection == i ? U8G2_BTN_INV : U8G2_BTN_BW0, 0, 2, 2, menuArray[i]);
    Serial.println(spacing);
    spacing += LINE_SPACING + u8g2.getAscent() + abs(u8g2.getDescent());
  }

  u8g2.sendBuffer(); // transfer internal memory to the display
}

void setup()
{
  Serial.begin(115200);

  pinMode(SELECTION_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SELECTION_BUTTON), incrementSelection, RISING);

  u8g2.begin();
  u8g2.setFont(u8g2_font_7x13B_tf); // choose a suitable font
}

void loop()
{

  static byte previousSelection = 1;
  static byte currentState = MAIN_MENU;

  // Only update display if selection changes
  if (previousSelection != currentSelection)
  {
    previousSelection = currentSelection;

    switch (currentState)
    {
    case (MAIN_MENU):

      // Display menu based on state
      displayMenu(MAIN_MENU_OPTIONS, MAIN_MENU_LENGTH);

      // Change state if selection made
      if (selectionMade && currentSelection == LIST_PEERS_SEL)
      {
        currentState = LIST_PEERS;
      }
      break;
    case (LIST_PEERS):
      break;
    }
  }
}