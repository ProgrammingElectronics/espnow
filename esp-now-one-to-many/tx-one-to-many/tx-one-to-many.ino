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

// Global copy of RXs
#define NUMRECEIVERS 20
esp_now_peer_info_t receivers[NUMRECEIVERS] = {};
int RXCnt = 0;

#define CHANNEL 1
#define PRINTSCANRESULTS 0

// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// Scan for receivers in AP mode
void ScanForReceivers() {
  int8_t scanResults = WiFi.scanNetworks();
  //reset receivers
  memset(receivers, 0, sizeof(receivers));
  RXCnt = 0;
  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS) {
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
      }
      delay(10);
      // Check if the current device starts with `RX`
      if (SSID.indexOf("RX") == 0) {
        // SSID of interest
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        // Get BSSID => Mac Address of the RX
        int mac[6];

        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            receivers[RXCnt].peer_addr[ii] = (uint8_t) mac[ii];
          }
        }
        receivers[RXCnt].channel = CHANNEL; // pick a channel
        receivers[RXCnt].encrypt = 0; // no encryption
        RXCnt++;
      }
    }
  }

  if (RXCnt > 0) {
    Serial.print(RXCnt); Serial.println(" Receivers(s) found, processing..");
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
      for (int ii = 0; ii < 6; ++ii ) {
        Serial.print((uint8_t) receivers[i].peer_addr[ii], HEX);
        if (ii != 5) Serial.print(":");
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


uint8_t data = 0;
// send data
void sendData() {
  data++;
  for (int i = 0; i < RXCnt; i++) {
    const uint8_t *peer_addr = receivers[i].peer_addr;
    if (i == 0) { // print only for first receiver
      Serial.print("Sending: ");
      Serial.println(data);
    }
    esp_err_t result = esp_now_send(peer_addr, &data, sizeof(data));
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
    delay(100);
  }
}

// callback when data is sent from Transmitter to Receiver
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(115200);
  //Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
}

void loop() {
  // In the loop we scan for receivers
  ScanForReceivers();
  // If receiver is found, it would be populate in `receiver` variable
  // We will check if `receiver` is defined and then we proceed further
  if (RXCnt > 0) { // check if receiver channel is defined
    // `receiver` is defined
    // Add receiver as peer if it has not been added already
    manageReceiver();
    // pair success or already paired
    // Send data to device
    sendData();
  } else {
    // No receiver found to process
  }

  // wait for 3seconds to run the logic again
  delay(1000);
}
