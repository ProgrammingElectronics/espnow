/**
 * @file tx-one-to-many.ino
 * @author Michael Cheich (michael@programmingelectronics.com)
 * @brief espnow example for a NEOpixel controller.  One transmitter controls multiple receivers. This is the TX code.
 * @version 0.1
 * @date 2022-09-12
 *
 * @copyright Copyright (c) 2022
 *

Designed for screen and size -> SSD1306 128X64

 NOTE: To add effect
  -Create const for "effect name" in Select Effect Menu section
  -Add effect name in SEL_EFFECT_OPTIONS array (adjust size accordingly)
  -Add "else if" in SELECT_EFFECT case to handle effect
  -Add effect code to RXs code as necessary
 */

#include <esp_now.h>
#include <WiFi.h>
#include <U8g2lib.h>
#include <neo_pixel_controller_shared.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define PRINTSCANRESULTS 1

// RX information and storage
#define NUMRECEIVERS 20
esp_now_peer_info_t receivers[NUMRECEIVERS] = {};

// Store the SSID of each connected network
#define MAX_SSID_DISPLAY_LEN 20
char peerSSIDs[MAX_PEERS][MAX_SSID_DISPLAY_LEN];
byte RXCnt = 0;  // Track the # of connected RXs

// States -> These determine which cases are run
enum STATES { MAIN_MENU,
              LIST_PEERS,
              RESCAN,
              BROADCAST,
              SELECT_EFFECT,
              SOLID_COLOR };

// Main Menu
enum MENU_MAIN { LIST_PEERS_SEL,
                 RESCAN_SEL,
                 BROADCAST_SEL };

//Select Effect Menu Options
enum MENU_SELECT_EFFECT { CHANGE_COLOR,
                          CYLON,
                          PACIFICA,
                          RANDOM_REDS,
                          TURN_OFF };

// List Peers menu options
#define NUM_PEERS_TO_DISPLAY 3
#define BACK_BUTTON_SPACER 1
#define NUM_MENU_ITEMS_TO_DISPLAY 3

// Formatting for display
#define LINE_SPACING 5  // space between each line

// Display Menus
#define MAIN_MENU_LENGTH 3
const char *MAIN_MENU_OPTIONS[MAIN_MENU_LENGTH] = { "1. List Peers", "2. ReScan", "3. Broadcast" };

#define SELECT_EFFECT_LENGTH 6
const char *SEL_EFFECT_OPTIONS[SELECT_EFFECT_LENGTH] = { "1. Change Color", "2. Cyclon", "3. Pacifica", "4. Random Reds", "5. Turn Off", "6. Back" };

#define COLOR_OPTIONS_LENGTH 8
const char *COLOR_OPTIONS[COLOR_OPTIONS_LENGTH];  //Initialized in setup, this array holds the color_name's specificed below

//Solid color options
#define TURN_OFF_COLOR 42 /* Reserved value used to trigger turning off all LEDs, cannot be used as color_value below*/

struct COLOR_VAL_PAIR {
  const char *color_name;
  const byte color_value;
} const COLOR_MENU[COLOR_OPTIONS_LENGTH] = {
  { "Red", 0 },
  { "Orange", 32 },
  { "Yellow", 64 },
  { "Green", 96 },
  { "Aqua", 128 },
  { "Blue", 160 },
  { "Purple", 192 },
  { "Pink", 224 }
};

// Button pins
#define INCREMENT_BUTTON 5
#define SELECT_BUTTON 6

// Track user button presses
volatile byte currentSelection = 0;
volatile bool selectionMade = false;

// Keep track of the timing of for button debounce done in interrupts
#define DEBOUNCE 250
volatile unsigned long incr_button_time = 0;
volatile unsigned long sel_button_time = 0;
volatile unsigned long last_incr_button_time = 0;
volatile unsigned long last_sel_button_time = 0;

struct neopixel_data data_out;

// Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE);  // High speed I2C

/****************************************************************
  ESPNOW Functions
*****************************************************************/

// Init ESP Now with fallback
void InitESPNow() {

  WiFi.disconnect();

  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");

  } else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow
    ESP.restart();
  }
}

// Scan for receivers in AP mode
void ScanForReceivers() {

  int8_t scanResults = WiFi.scanNetworks();
  // reset receivers
  memset(receivers, 0, sizeof(receivers));
  RXCnt = 0;
  Serial.println("");

  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");

  } else {
    Serial.print("Found ");
    Serial.print(scanResults);
    Serial.println(" devices ");

    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" [");
        Serial.print(BSSIDstr);
        Serial.print("]");
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
      }
      delay(10);

      //TODO -> Limit RX count to 20 per ESPnow specs
      // Check if the current device starts with `RX`
      if (SSID.indexOf("RX") == 0) {
        // SSID of interest
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" [");
        Serial.print(BSSIDstr);
        Serial.print("]");
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
        // Get BSSID => Mac Address of the RX
        int mac[6];

        if (6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5])) {
          for (int ii = 0; ii < 6; ++ii) {
            receivers[RXCnt].peer_addr[ii] = (uint8_t)mac[ii];
          }
        }
        receivers[RXCnt].channel = CHANNEL;  // pick a channel
        receivers[RXCnt].encrypt = 0;        // no encryption

        //Remove the "RX_" from the beginning of the SSID and limit length for display
        SSID = SSID.substring(3, MAX_SSID_DISPLAY_LEN);
        SSID.toCharArray(peerSSIDs[RXCnt], SSID.length() + 1);  // This copies the appropriate chars strings into peerSSID[][]

        RXCnt++;
      }
    }
  }

  if (RXCnt > 0) {
    Serial.print(RXCnt);
    Serial.println(" Receivers(s) found, processing..");

  } else {
    Serial.println("No Receiver Found, trying again.");
  }

  // clean up ram
  WiFi.scanDelete();
}

// Check if the receiver is already paired with the transmitter.
// If not, pair the receiver with transmitter
void manageReceiver() {

  if (RXCnt > 0) {

    for (int i = 0; i < RXCnt; i++) {
      Serial.print("Processing: ");

      for (int ii = 0; ii < 6; ++ii) {
        Serial.print((uint8_t)receivers[i].peer_addr[ii], HEX);

        if (ii != 5)
          Serial.print(":");
      }

      Serial.print(" Pair Status: ");
      // check if the peer exists
      bool exists = esp_now_is_peer_exist(receivers[i].peer_addr);

      if (exists) {
        // Receiver already paired.
        Serial.println("Already Paired");

      } else {
        // Receiver not paired, attempt pair
        esp_err_t addStatus = esp_now_add_peer(&receivers[i]);
        displayError(addStatus);
        delay(100);
      }
    }
  } else {
    // No receiver found to process
    Serial.println("No receiver found to process");
  }
}

void displayError(esp_err_t result) {

  switch (result) {

    case ESP_OK:
      Serial.println("Success");
      break;

    case ESP_ERR_ESPNOW_NOT_INIT:
      // How did we get so far!!
      Serial.println("ESPNOW not Init.");
      break;

    case ESP_ERR_ESPNOW_ARG:
      Serial.println("Invalid Argument-> ESP_ERR_ESPNOW_ARG");
      break;

    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.println("Internal Error-> ESP_ERR_ESPNOW_INTERNAL");
      break;

    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("Out of memory-> ESP_ERR_ESPNOW_NO_MEM");
      break;

    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println("Peer not found-> ESP_ERR_ESPNOW_NOT_FOUND");
      break;

    case ESP_ERR_ESPNOW_FULL:
      Serial.println("Peer list full-> ESP_ERR_ESPNOW_FULL");
      break;

    case ESP_ERR_ESPNOW_EXIST:
      Serial.println("Peer Exists-> ESP_ERR_ESPNOW_EXIST");
      break;

    default:
      Serial.println("Not sure what happened");
      break;
  }
}

void sendData(byte RX_sel, bool broadcastMode) {

  //Set address for specific peer (not used when broadcasting)
  const uint8_t *peer_addr = receivers[RX_sel].peer_addr;

  if (!broadcastMode) {
    Serial.print("Sending to SSID: ");
    Serial.println(peerSSIDs[RX_sel]);
  } else {
    Serial.print("Broadcasting to all");
  }

  /*
    According to esp_now_send documentation, if first parameter is NULL, 
    it sends to all the addressess in the peer list.  I *assume* it is using the broadcast address {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}?  
    As opposed to a for loop that iterates through the all the peers in the peer list...
    In either case, I am not sure it matters. 
   */
  esp_err_t result = esp_now_send(broadcastMode ? NULL : peer_addr, (uint8_t *)&data_out, sizeof(data_out));

  Serial.print("Send Status: ");
  displayError(result);
}

// callback when data is sent from Transmitter to Receiver
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

/****************************************************************
  OLED Display Functions
*****************************************************************/

void displayMenu(const char *menuArray[], byte len) {

  u8g2.clearBuffer();  // clear the internal memory

  int spacing = LINE_SPACING + u8g2.getAscent() + abs(u8g2.getDescent());

  // Only show 3 menu items at once
  int start = currentSelection / NUM_MENU_ITEMS_TO_DISPLAY * NUM_MENU_ITEMS_TO_DISPLAY;
  int end = start + NUM_MENU_ITEMS_TO_DISPLAY > len ? len : start + NUM_MENU_ITEMS_TO_DISPLAY;

  for (int i = start; i < end; i++) {
    u8g2.drawButtonUTF8(0, spacing, currentSelection == i ? U8G2_BTN_INV : U8G2_BTN_BW0, 0, 2, 2, menuArray[i]);
    spacing += LINE_SPACING + u8g2.getAscent() + abs(u8g2.getDescent());
  }

  u8g2.sendBuffer();  // transfer internal memory to the display
}

/****************************************************************
  ISR's (Interrupt Service Routines) for button presses
*****************************************************************/
void IRAM_ATTR incrementButton() {

  incr_button_time = millis();

  if (incr_button_time - last_incr_button_time > DEBOUNCE) {
    currentSelection++;
    last_incr_button_time = incr_button_time;
  }
}

void IRAM_ATTR selectButton() {

  sel_button_time = millis();

  if (sel_button_time - last_sel_button_time > DEBOUNCE) {
    selectionMade = true;
    last_sel_button_time = sel_button_time;
  }
}

/****************************************************************
  Swicth-case functions
*****************************************************************/

void rescan() {

  u8g2.clearBuffer();
  u8g2.drawButtonUTF8(1, 10, U8G2_BTN_INV, 0, 2, 2, "Scanning...");
  u8g2.sendBuffer();

  ScanForReceivers();
  manageReceiver();
}

/****************************************************************
  Helper functions
*****************************************************************/

void limitSelection(uint8_t max_selection) {

  if (currentSelection >= max_selection) {
    currentSelection = 0;
  }
}

/****************************************************************
  Setup
*****************************************************************/

void setup() {

  Serial.begin(115200);

  pinMode(INCREMENT_BUTTON, INPUT_PULLUP);
  pinMode(SELECT_BUTTON, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(INCREMENT_BUTTON), incrementButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(SELECT_BUTTON), selectButton, FALLING);

  // Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  Serial.println("Wifi Mode Set.");

  // Init ESPNow with a fallback logic
  InitESPNow();
  Serial.println("ESPNow Init");

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  ScanForReceivers();
  manageReceiver();

  Serial.print("RXs scanned, found: ");
  Serial.print(RXCnt);

  u8g2.begin();
  u8g2.setFont(u8g2_font_7x13B_tf);  // choose a suitable font

  //Create menu for display on OLED for CHANGE COLOR sub menu
  for (int i = 0; i < COLOR_OPTIONS_LENGTH; i++) {
    COLOR_OPTIONS[i] = COLOR_MENU[i].color_name;
  }

  Serial.print("Setup Complete");
}

/****************************************************************
  Loop
*****************************************************************/

void loop() {

  static byte currentState = MAIN_MENU;
  static byte previousSelection = currentSelection + 1;
  static byte RX_selected = 0;
  static bool isBroadcasting = false;

  switch (currentState) {
    case MAIN_MENU:

      limitSelection(MAIN_MENU_LENGTH);

      // Display Menu
      if (previousSelection != currentSelection) {

        displayMenu(MAIN_MENU_OPTIONS, MAIN_MENU_LENGTH);
        previousSelection = currentSelection;
      }

      // Handle selections
      if (selectionMade) {

        switch (currentSelection) {

          case LIST_PEERS_SEL:
            currentState = LIST_PEERS;
            isBroadcasting = false;
            break;

          case RESCAN_SEL:
            currentState = RESCAN;
            break;

          case BROADCAST_SEL:
            currentState = SELECT_EFFECT;
            isBroadcasting = true;
            break;
        }
      }

      break;

    case LIST_PEERS:

      // Limit Selection
      limitSelection(RXCnt + BACK_BUTTON_SPACER);

      if (previousSelection != currentSelection) {

        //Build array of char pointers to RX SSIDs for menu and add back button
        const char *SSID_Menu[RXCnt + BACK_BUTTON_SPACER];
        for (int i = 0; i < RXCnt + BACK_BUTTON_SPACER; i++) {

          if (i == RXCnt) {
            SSID_Menu[i] = "Back";

          } else {
            SSID_Menu[i] = peerSSIDs[i];
          }
        }

        //displayPeers();
        displayMenu(SSID_Menu, RXCnt + BACK_BUTTON_SPACER);
        previousSelection = currentSelection;
      }

      // Handle selection
      if (selectionMade) {

        if (RXCnt == currentSelection /*Back Button Pressed*/) {
          currentState = MAIN_MENU;

        } else if (selectionMade) { /* Specific peer selected*/
          currentState = SELECT_EFFECT;
          RX_selected = currentSelection;
        }
      }

      break;

    case RESCAN:

      rescan();

      //Handle Selection
      currentState = MAIN_MENU;

      break;

    case SELECT_EFFECT:

      // Limit Selection
      limitSelection(SELECT_EFFECT_LENGTH);

      if (previousSelection != currentSelection) {
        displayMenu(SEL_EFFECT_OPTIONS, SELECT_EFFECT_LENGTH);
        previousSelection = currentSelection;
      }

      // Handle selection
      if (selectionMade) {

        switch (currentSelection) {

          case (SELECT_EFFECT_LENGTH - 1): /*Back Button Pressed*/
            currentState = isBroadcasting ? MAIN_MENU : LIST_PEERS;
            currentSelection = 0;  // Start at first menu item
            break;

          case CHANGE_COLOR:
            currentState = SOLID_COLOR;
            previousSelection = currentSelection + 1;  // Make sure new menu is displayed
            break;

          case CYLON:
            data_out.effect = CYLON;
            sendData(RX_selected, isBroadcasting);
            break;

          case PACIFICA:
            data_out.effect = PACIFICA;
            sendData(RX_selected, isBroadcasting);
            break;

          case RANDOM_REDS:
            data_out.effect = RANDOM_REDS;
            sendData(RX_selected, isBroadcasting);
            break;

          case TURN_OFF:
            data_out.effect = CHANGE_COLOR;
            data_out.hue = TURN_OFF_COLOR;
            data_out.saturation = 0;
            data_out.value = 0;
            sendData(RX_selected, isBroadcasting);
            break;
        }

        selectionMade = false;
      }

      break;

    case SOLID_COLOR:

      // Limit Selection
      limitSelection(COLOR_OPTIONS_LENGTH);

      // Display menu and send color data
      if (previousSelection != currentSelection) {

        displayMenu(COLOR_OPTIONS, COLOR_OPTIONS_LENGTH);
        previousSelection = currentSelection;

        data_out.effect = CHANGE_COLOR;
        data_out.hue = COLOR_MENU[currentSelection].color_value;

        //Adjust saturation and value to zero if "Turn Off" selected
        data_out.saturation = 255;
        data_out.value = 255;
        sendData(RX_selected, isBroadcasting);
      }

      // Handle selection
      if (selectionMade) currentState = SELECT_EFFECT;

      break;
  }

  // Reset Seletion
  if (selectionMade) {
    currentSelection = 0;                      // Start at first menu item in next menu
    previousSelection = currentSelection + 1;  // Make sure new menu is displayed
    selectionMade = false;
  }
}