/**
 * @file tx-one-to-many.ino
 * @author Michael Cheich (michael@programmingelectronics.com)
 * @brief espnow example where one tranmsitter send individual messages to multiple receivers
 * @version 0.1
 * @date 2022-09-12
 *
 * @copyright Copyright (c) 2022
 *

 NOTE: To add effect
  -Create const for "effect name" in Select Effect Menu section
  -Add effect name in SEL_EFFECT_OPTIONS array (adjust size accordingly)
  -Add "else if" in SELECT_EFFECT case to handle effect
  -Add effect code to RXs code as necessary
 */

#include <esp_now.h>
#include <WiFi.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Global copy of RXs
const byte NUMRECEIVERS = 20;

esp_now_peer_info_t receivers[NUMRECEIVERS] = {};

// Store the SSID of each connected network
char peerSSIDs[20][32];  // This Works with displayPeers()
byte RXCnt = 0;

const byte CHANNEL = 1;
const byte PRINTSCANRESULTS = 1;

// States -> The determine which cases are run
const byte MAIN_MENU = 0;
const byte LIST_PEERS = 1;
const byte RESCAN = 2;
const byte BROADCAST = 3;
const byte SELECT_EFFECT = 4;
const byte SOLID_COLOR = 5;

// Main Menu
const byte LIST_PEERS_SEL = 0;
const byte RESCAN_SEL = 1;
const byte BROADCAST_SEL = 2;

//Select Effect Menu Options
const byte CHANGE_COLOR = 0;
const byte CYLON = 1;
const byte PACIFICA = 2;
const byte RANDOM_REDS = 3;
const byte TURN_OFF = 4;

// List Peers menu options
const byte NUM_PEERS_TO_DISPLAY = 3;
const byte BACK_BUTTON_SPACER = 1;
const byte NUM_MENU_ITEMS_TO_DISPLAY = 3;


// Formatting for display
const byte LINE_SPACING = 5;  // space between each line

// Display Menus
const byte MAIN_MENU_LENGTH = 3;
const char *MAIN_MENU_OPTIONS[MAIN_MENU_LENGTH] = { "1. List Peers", "2. ReScan", "3. Broadcast" };

const byte SELECT_EFFECT_LENGTH = 6;
const char *SEL_EFFECT_OPTIONS[SELECT_EFFECT_LENGTH] = { "1. Change Color", "2. Cyclon", "3. Pacifica", "4. Random Reds", "5. Turn Off", "6. Back" };

const byte COLOR_OPTIONS_LENGTH = 9;

const char *COLOR_OPTIONS[COLOR_OPTIONS_LENGTH] = { "Red", "Orange", "Yellow", "Green", "Aqua", "Blue", "Purple", "Pink", "Turn Off" /*Turn Off must always be last option!*/ };
const byte COLOR_VALUES[COLOR_OPTIONS_LENGTH] = { 0, 32, 64, 96, 128, 160, 192, 224, 255 };

// Button pins and timing
const byte INCREMENT_BUTTON = 5;
const byte SELECT_BUTTON = 6;
const int DEBOUNCE = 250;

// Track user button presses
volatile byte currentSelection = 0;
volatile bool selectionMade = false;

// variables to keep track of the timing of recent interrupts
volatile unsigned long incr_button_time = 0;
volatile unsigned long sel_button_time = 0;
volatile unsigned long last_incr_button_time = 0;
volatile unsigned long last_sel_button_time = 0;

// This is data structure sent to each RX
struct neopixel_data {
  byte effect;
  bool display = true;
  byte hue = 100;
  byte saturation = 255;
  byte value = 255;
} data_out;

// Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE);  // High speed I2C

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

        SSID.toCharArray(peerSSIDs[RXCnt], SSID.length());  // This copies the appropriate chars strings into peerSSID[][] and work with displayPeers()

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

      Serial.print(" Status: ");
      // check if the peer exists
      bool exists = esp_now_is_peer_exist(receivers[i].peer_addr);

      if (exists) {
        // Receiver already paired.
        Serial.println("Already Paired");

      } else {
        // Receiver not paired, attempt pair
        esp_err_t addStatus = esp_now_add_peer(&receivers[i]);

        if (addStatus == ESP_OK) {
          // Pair success
          Serial.println("Pair success");

        } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
          // How did we get so far!!
          Serial.println("ESPNOW Not Init");

        } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
          Serial.println("Add Peer - Invalid Argument");

        } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
          Serial.println("Peer list full");

        } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
          Serial.println("Out of memory");

        } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
          Serial.println("Peer Exists");

        } else {
          Serial.println("Not sure what happened");
        }
        delay(100);
      }
    }
  } else {
    // No receiver found to process
    Serial.println("No receiver found to process");
  }
}


void displayError(esp_err_t result) {
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
    Serial.println("Success");

  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW not Init.");

  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");

  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");

  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");

  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");

  } else {
    Serial.println("Not sure what happened");
  }
}


void sendData(byte RX_sel, bool broadcastMode) {

  //Set address for specific peer (not used when broadcasting)
  const uint8_t *peer_addr = receivers[RX_sel].peer_addr;

  if (!broadcastMode) {
    Serial.print("Sending to SSID: ");
    Serial.println(peerSSIDs[RX_sel]);
  } else {
    Serial.print("Broadcatsing to all");
  }

  /*
    According to esp_now_send documentation, if first parameter is NULL, 
    it sends to all the addressess in the peer list.  I *assume* it is using the broadcast address {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}?  
    As opposed to a for loop that iterates through the all the peers in the peer list...
    In either case, I am not sure it matters. 
   */
  esp_err_t result = esp_now_send(broadcastMode ? NULL : peer_addr, (uint8_t *)&data_out, sizeof(data_out));
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

void rescan() {
  u8g2.clearBuffer();
  u8g2.drawButtonUTF8(1, 10, U8G2_BTN_INV, 0, 2, 2, "Scanning...");
  u8g2.sendBuffer();

  ScanForReceivers();
  manageReceiver();
}

void limitSelection(uint8_t max_selection) {

  if (currentSelection >= max_selection) {
    currentSelection = 0;
  }
}

void IRAM_ATTR incrementButton() {

  incr_button_time = millis();

  if (incr_button_time - last_incr_button_time > DEBOUNCE) {
    currentSelection++;
    Serial.println(currentSelection);
    last_incr_button_time = incr_button_time;
  }
}

void IRAM_ATTR selectButton() {

  sel_button_time = millis();

  if (sel_button_time - last_sel_button_time > DEBOUNCE) {
    selectionMade = true;
    Serial.println(selectionMade);
    last_sel_button_time = sel_button_time;
  }
}

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
  Serial.print("Setup Complete");
}

void loop() {

  static byte currentState = MAIN_MENU;
  static byte previousSelection = currentSelection + 1;
  static byte RX_selected = 0;
  static bool isBroadcasting = false;

  switch (currentState) {
    case (MAIN_MENU):

      limitSelection(MAIN_MENU_LENGTH);

      // Display Menu
      if (previousSelection != currentSelection) {
        displayMenu(MAIN_MENU_OPTIONS, MAIN_MENU_LENGTH);
        previousSelection = currentSelection;
      }

      // Handle selections
      if (selectionMade) {

        if (LIST_PEERS_SEL == currentSelection) {
          currentState = LIST_PEERS;
          isBroadcasting = false;

        } else if (RESCAN_SEL == currentSelection) {
          currentState = RESCAN;

        } else if (BROADCAST_SEL == currentSelection) {
          currentState = SELECT_EFFECT;
          isBroadcasting = true;
        }
      }

      break;

    case (LIST_PEERS):

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

    case (RESCAN):

      rescan();

      //Handle Selection
      currentState = MAIN_MENU;

      break;

    case (SELECT_EFFECT):

      // Limit Selection
      limitSelection(SELECT_EFFECT_LENGTH);

      if (previousSelection != currentSelection) {
        displayMenu(SEL_EFFECT_OPTIONS, SELECT_EFFECT_LENGTH);
        previousSelection = currentSelection;
      }

      // Handle selection
      if (selectionMade) {

        if ((SELECT_EFFECT_LENGTH - 1) == currentSelection /*Back Button Pressed*/) {
          currentState = isBroadcasting ? MAIN_MENU : LIST_PEERS;
          currentSelection = 0;  // Start at first menu item

        } else if (CHANGE_COLOR == currentSelection) {
          Serial.print("CHANGE_COLOR_SEL | currentSelection -> ");
          Serial.println(currentSelection);
          Serial.println(" ");

          currentState = SOLID_COLOR;
          previousSelection = currentSelection + 1;  // Make sure new menu is displayed

        } else if (CYLON == currentSelection) {
          Serial.print("CYLON_SEL | currentSelection -> ");
          Serial.println(currentSelection);
          Serial.println(" ");

          data_out.effect = CYLON;
          sendData(RX_selected, isBroadcasting);

        } else if (PACIFICA == currentSelection) {
          Serial.print("PACIFICA_SEL | currentSelection -> ");
          Serial.println(currentSelection);
          Serial.println(" ");

          data_out.effect = PACIFICA;
          sendData(RX_selected, isBroadcasting);

        } else if (RANDOM_REDS == currentSelection) {
          Serial.print("RANDOM_REDS_SEL | currentSelection -> ");
          Serial.println(currentSelection);
          Serial.println(" ");

          data_out.effect = RANDOM_REDS;
          sendData(RX_selected, isBroadcasting);

        } else if (TURN_OFF == currentSelection) {
          Serial.print("TURN_OFF | currentSelection -> ");
          Serial.println(currentSelection);
          Serial.println(" ");

          data_out.effect = CHANGE_COLOR;  //Turn Off use the Solid Color

          data_out.hue = 255;  // This value needs to be different then the previous hue sent to RX, or RX will not adjust
          //Adjust saturation and value to zero if "Turn Off" selected
          data_out.saturation = 0;
          data_out.value = 0;
          sendData(RX_selected, isBroadcasting);
        }

        selectionMade = false;
      }

      break;

    case (SOLID_COLOR):

      // Limit Selection
      limitSelection(COLOR_OPTIONS_LENGTH);

      // Display menu and send color data
      if (previousSelection != currentSelection) {

        displayMenu(COLOR_OPTIONS, COLOR_OPTIONS_LENGTH);
        previousSelection = currentSelection;

        data_out.effect = CHANGE_COLOR;
        data_out.hue = COLOR_VALUES[currentSelection];

        //Adjust saturation and value to zero if "Turn Off" selected
        data_out.saturation = currentSelection == COLOR_OPTIONS_LENGTH - 1 ? 0 : 255;
        data_out.value = currentSelection == COLOR_OPTIONS_LENGTH - 1 ? 0 : 255;
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