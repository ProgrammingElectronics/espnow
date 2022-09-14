/**
 * @file tx-one-to-many.ino
 * @author Michael Cheich (michael@programmingelectronics.com)
 * @brief espnow example where one tranmsitter send individual messages to multiple receivers
 * @version 0.1
 * @date 2022-09-12
 *
 * @copyright Copyright (c) 2022
 *
 */

/**
 * todo
 * - Bring in example code and change to verbage
 * - Add lcd
 *    -pins
 * -
 *
 *
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
#define NUMRECEIVERS 20
esp_now_peer_info_t receivers[NUMRECEIVERS] = {};
char peerSSIDs[20][50]; // Store the SSID of each connected network

int RXCnt = 0;

#define CHANNEL 1
#define PRINTSCANRESULTS 0

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

const byte INCREMENT_BUTTON = 5;
const byte MAX_SELECTIONS = 2;
const byte LINE_SPACING = 5; // space between each line
const byte MAIN_MENU_LENGTH = 3;
const byte SELECT_EFFECT_LENGTH = 4;
const char *MAIN_MENU_OPTIONS[MAIN_MENU_LENGTH] = {"1. List Peers", "2. ReScan", "3. Broadcast"};
const char *SEL_EFFECT_OPTIONS[SELECT_EFFECT_LENGTH] = {"1. Change Color", "2. Cyclon", "3. Pacifica", "4. Random Reds"};

// Track user button presses
volatile byte currentSelection = 0;
volatile bool selectionMade = false;
// variables to keep track of the timing of recent interrupts
volatile unsigned long button_time = 0;
volatile unsigned long last_button_time = 0;

// Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE); // High speed I2C

// Init ESP Now with fallback
void InitESPNow()
{
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESPNow Init Success");
  }
  else
  {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// Scan for receivers in AP mode
void ScanForReceivers()
{
  int8_t scanResults = WiFi.scanNetworks();
  // reset receivers
  memset(receivers, 0, sizeof(receivers));
  RXCnt = 0;
  Serial.println("");
  if (scanResults == 0)
  {
    Serial.println("No WiFi devices in AP Mode found");
  }
  else
  {
    Serial.print("Found ");
    Serial.print(scanResults);
    Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i)
    {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS)
      {
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
      // Check if the current device starts with `RX`
      if (SSID.indexOf("RX") == 0)
      {
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

        if (6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]))
        {
          for (int ii = 0; ii < 6; ++ii)
          {
            receivers[RXCnt].peer_addr[ii] = (uint8_t)mac[ii];
          }
        }
        receivers[RXCnt].channel = CHANNEL; // pick a channel
        receivers[RXCnt].encrypt = 0;       // no encryption

        Serial.println(SSID);
        SSID.toCharArray(peerSSIDs[RXCnt], SSID.length());

        RXCnt++;
      }
    }
  }

  if (RXCnt > 0)
  {
    Serial.print(RXCnt);
    Serial.println(" Receivers(s) found, processing..");
  }
  else
  {
    Serial.println("No Receiver Found, trying again.");
  }

  // clean up ram
  WiFi.scanDelete();
}

// Check if the receiver is already paired with the transmitter.
// If not, pair the receiver with transmitter
void manageReceiver()
{
  if (RXCnt > 0)
  {
    for (int i = 0; i < RXCnt; i++)
    {
      Serial.print("Processing: ");
      for (int ii = 0; ii < 6; ++ii)
      {
        Serial.print((uint8_t)receivers[i].peer_addr[ii], HEX);
        if (ii != 5)
          Serial.print(":");
      }
      Serial.print(" Status: ");
      // check if the peer exists
      bool exists = esp_now_is_peer_exist(receivers[i].peer_addr);
      if (exists)
      {
        // Receiver already paired.
        Serial.println("Already Paired");
      }
      else
      {
        // Receiver not paired, attempt pair
        esp_err_t addStatus = esp_now_add_peer(&receivers[i]);
        if (addStatus == ESP_OK)
        {
          // Pair success
          Serial.println("Pair success");
        }
        else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT)
        {
          // How did we get so far!!
          Serial.println("ESPNOW Not Init");
        }
        else if (addStatus == ESP_ERR_ESPNOW_ARG)
        {
          Serial.println("Add Peer - Invalid Argument");
        }
        else if (addStatus == ESP_ERR_ESPNOW_FULL)
        {
          Serial.println("Peer list full");
        }
        else if (addStatus == ESP_ERR_ESPNOW_NO_MEM)
        {
          Serial.println("Out of memory");
        }
        else if (addStatus == ESP_ERR_ESPNOW_EXIST)
        {
          Serial.println("Peer Exists");
        }
        else
        {
          Serial.println("Not sure what happened");
        }
        delay(100);
      }
    }
  }
  else
  {
    // No receiver found to process
    Serial.println("No receiver found to process");
  }
}

uint8_t data = 0;
// send data
void sendData()
{
  data++;
  for (int i = 0; i < RXCnt; i++)
  {
    const uint8_t *peer_addr = receivers[i].peer_addr;
    if (i == 0)
    { // print only for first receiver
      Serial.print("Sending: ");
      Serial.println(data);
    }
    esp_err_t result = esp_now_send(peer_addr, &data, sizeof(data));
    Serial.print("Send Status: ");
    if (result == ESP_OK)
    {
      Serial.println("Success");
    }
    else if (result == ESP_ERR_ESPNOW_NOT_INIT)
    {
      // How did we get so far!!
      Serial.println("ESPNOW not Init.");
    }
    else if (result == ESP_ERR_ESPNOW_ARG)
    {
      Serial.println("Invalid Argument");
    }
    else if (result == ESP_ERR_ESPNOW_INTERNAL)
    {
      Serial.println("Internal Error");
    }
    else if (result == ESP_ERR_ESPNOW_NO_MEM)
    {
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
    }
    else if (result == ESP_ERR_ESPNOW_NOT_FOUND)
    {
      Serial.println("Peer not found.");
    }
    else
    {
      Serial.println("Not sure what happened");
    }
    delay(100);
  }
}

// callback when data is sent from Transmitter to Receiver
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void displayMenu(const char *menuArray[], byte len)
{
  u8g2.clearBuffer(); // clear the internal memory

  int spacing = LINE_SPACING + u8g2.getAscent() + abs(u8g2.getDescent());
  for (int i = 0; i < len; i++)
  {
    u8g2.drawButtonUTF8(0, spacing, currentSelection == i ? U8G2_BTN_INV : U8G2_BTN_BW0, 0, 2, 2, menuArray[i]);
    spacing += LINE_SPACING + u8g2.getAscent() + abs(u8g2.getDescent());
  }

  u8g2.sendBuffer(); // transfer internal memory to the display
}

void displayPeers()
{
  u8g2.clearBuffer();
  int spacing = LINE_SPACING + u8g2.getAscent() + abs(u8g2.getDescent());
  for (int i = 0; i < RXCnt; i++)
  {
    u8g2.drawButtonUTF8(0, spacing, currentSelection == i ? U8G2_BTN_INV : U8G2_BTN_BW0, 0, 2, 2, peerSSIDs[i]);
    spacing += LINE_SPACING + u8g2.getAscent() + abs(u8g2.getDescent());
  }

  u8g2.sendBuffer(); // transfer internal memory to the display
}

void IRAM_ATTR incrementSelection()
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

void setup()
{
  Serial.begin(230400);

  pinMode(INCREMENT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INCREMENT_BUTTON), incrementSelection, FALLING);

  // Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  ScanForReceivers();

  u8g2.begin();
  u8g2.setFont(u8g2_font_7x13B_tf); // choose a suitable font
}

void loop()
{
  static byte previousSelection = 1;
  // static byte currentState = MAIN_MENU;
  static byte currentState = LIST_PEERS;

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
      displayPeers();
      break;
    }
  }
}
