#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>


PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

const char* ssid = "OPPO A1K";
const char* password = "66769712";


const int maxAttempts = 30;
const int delayBetweenAttempts = 1000;
const int ledwifi = 15;

const int ledEmergency = 2;

const int redLane1 = 27;
const int yellowLane1 = 14;
const int greenLane1 = 12;

const int redLane2 = 19;
const int yellowLane2 = 18;
const int greenLane2 = 5;

const int redLane3 = 25;
const int yellowLane3 = 33;
const int greenLane3 = 32;

const int redLane4 = 16;
const int yellowLane4 = 4;
const int greenLane4 = 0;

int vehicleCountLane1 = 0;
int vehicleCountLane2 = 0;
int vehicleCountLane3 = 0;
int vehicleCountLane4 = 0;
int highestDensityLane = 1;

// Define flags to track the presence of emergency vehicles
bool emergencyVehicle1Detected = false;
bool emergencyVehicle2Detected = false;

uint8_t tag1[] = {0x06, 0x54, 0xD6, 0x4B};   // Tag 1 UID (Emergency vehicle 1)
uint8_t tag2[] = {0x23, 0xC3, 0x36, 0x95};  // Tag 2 UID (Emergency vehicle 2)

const char webpage[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"> <head> <meta charset="UTF-8" /> <meta name="viewport" content="width=device-width, initial-scale=1.0" /> <title>Smart Traffic Management System</title> <style> body { font-family: Arial, sans-serif; background-color: #eeeeee; color: #003366; margin: 0; padding: 0; display: flex; height: 100vh; box-sizing: border-box; } .container { display: flex; flex-direction: column; align-items: center; text-align: center; flex: 1; } img { width: 100%; max-width: 600px; height: auto; display: block; } .corner-text { position: absolute; color: #23130e; font-size: 1rem; font-weight: bold; } .top-left { top: 15%; left: 15%; transform: translate(-50%, -50%); } .top-right { top: 15%; right: 15%; transform: translate(50%, -50%); } .bottom-left { bottom: 15%; left: 15%; transform: translate(-50%, 50%); } .bottom-right { bottom: 15%; right: 15%; transform: translate(50%, 50%); } .status { font-size: 1.2rem; font-weight: bold; margin-top: 5px; padding: 5px; border-radius: 5px; } .moving { color: #ffffff; background-color: #28a745; } .stopped { color: #ffffff; background-color: #dc3545; } .side-panel { width: 250px; background-color: #ffffff; padding: 20px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); } .side-panel h2 { color: #003366; margin-top: 0; } .side-panel p { color: #666666; } .warning { display: none; background-color: #ffcc00; color: #ffffff; font-size: 1rem; font-weight: bold; padding: 10px; border-radius: 5px; margin-top: 20px; animation: flash 1s infinite; } @keyframes flash { 0%, 50%, 100% { background-color: #ff0000; } 25%, 75% { background-color: #ffcc00; } } </style> </head> <body> <div class="side-panel"> <h1>STMS</h1> <h2>Dashboard</h2> <hr /> <p><strong>Current Mode:</strong> Dynamic</p> <p><strong>System Status:</strong> Online</p> <div id="warning" class="warning">Emergency Vehicle Detected!</div> </div> <div class="container"> <div style="position: relative; width: 100%; max-width: 600px"> <img src="https://cdn.pixabay.com/photo/2023/08/13/22/19/street-8188557_1280.png" alt="four-way-intersection" /> <div class="corner-text top-right" id="Lane1"> <h2>Lane 1</h2> <p>Vehicle count: <span id="lane1">-</span></p> <div id="status1" class="status">-</div> </div> <div class="corner-text bottom-left" id="Lane2"> <div id="status2" class="status">-</div> <p>Vehicle count: <span id="lane2">-</span></p> <h2>Lane 2</h2> </div> <div class="corner-text top-left" id="Lane3"> <h2>Lane 3</h2> <p>Vehicle count: <span id="lane3">-</span></p> <div id="status3" class="status">-</div> </div> <div class="corner-text bottom-right" id="Lane4"> <div id="status4" class="status">-</div> <p>Vehicle count: <span id="lane4">-</span></p> <h2>Lane 4</h2> </div> </div> </div> <script> var Socket; function init() { Socket = new WebSocket("ws://" + window.location.hostname + ":81/"); Socket.onmessage = function (event) { processCommand(event); }; } function processCommand(event) { var obj = JSON.parse(event.data); document.getElementById("lane1").innerHTML = obj.lane1; document.getElementById("lane2").innerHTML = obj.lane2; document.getElementById("lane3").innerHTML = obj.lane3; document.getElementById("lane4").innerHTML = obj.lane4; updateStatus(1, obj.movingLane == 1); updateStatus(2, obj.movingLane == 2); updateStatus(3, obj.movingLane == 3); updateStatus(4, obj.movingLane == 4); if (obj.emergencyDetected) { document.getElementById("warning").style.display = "block"; } else { document.getElementById("warning").style.display = "none"; } } function updateStatus(lane, isMoving) { var statusElement = document.getElementById("status" + lane); if (isMoving) { statusElement.className = "status moving"; statusElement.innerHTML = "Moving..."; } else { statusElement.className = "status stopped"; statusElement.innerHTML = "Stopped!"; } } window.onload = function (event) { init(); }; </script> </body></html>)rawliteral";

void connectToWiFi();
void setup() {
  Serial.begin(115200);

  // NFC setup
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN53x board");
    while (1); // halt
  }
  Serial.print("Found chip PN5");
  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.');
  Serial.println((versiondata >> 8) & 0xFF, DEC);
  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();



  // Initialize pin modes
  pinMode(ledwifi, OUTPUT);
  digitalWrite(ledwifi, LOW);

  pinMode(ledEmergency, OUTPUT);
  pinMode(redLane1, OUTPUT);
  pinMode(yellowLane1, OUTPUT);
  pinMode(greenLane1, OUTPUT);
  pinMode(redLane2, OUTPUT);
  pinMode(yellowLane2, OUTPUT);
  pinMode(greenLane2, OUTPUT);
  pinMode(redLane3, OUTPUT);
  pinMode(yellowLane3, OUTPUT);
  pinMode(greenLane3, OUTPUT);
  pinMode(redLane4, OUTPUT);
  pinMode(yellowLane4, OUTPUT);
  pinMode(greenLane4, OUTPUT);


  // WiFi setup
  //  connectToWiFi();
  connectToWiFi();


  // Initial setup
  initialstate_allred();

  Serial.println("WELCOME ... STMS!");
}

void loop() {
  server.handleClient();
  webSocket.loop();
  checkTagAndControlTraffic();

}

void connectToWiFi() {
  Serial.println();
  Serial.print("Connecting to Wi-Fi network: ");
  Serial.println(ssid);

  int attemptCount = 0;
  digitalWrite(ledwifi, LOW);

  while (WiFi.status() != WL_CONNECTED && attemptCount < 30) {
    WiFi.begin(ssid, password);
    delay(delayBetweenAttempts);
    attemptCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected: " + String(ssid));
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(ledwifi, HIGH);
    setupServers();
    Serial.println("Server started... ");
  } else {
    Serial.println("Failed to connect to Wi-Fi. Please check your credentials or network.");
  }
}

void setupServers() {
  server.on("/", []() {
    // server.send_P(200, "text/html", webpage);
    server.send(200, "text/html", webpage);
  });
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("Client " + String(num) + " connected");
      break;
    case WStype_TEXT:
      Serial.write(payload, length);
      Serial.println();
      break;
  }
}


void checkTagAndControlTraffic() {
  server.handleClient();
  webSocket.loop();
  boolean success;
  uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0};
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
  dynamictraffic();

  if (success) {
    server.handleClient();
    webSocket.loop();
    while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength)) {
      delay(500);
      if (uidLength == sizeof(tag1) && memcmp(uid, tag1, sizeof(tag1)) == 0) {
        if (!emergencyVehicle1Detected) {
          Serial.println("Emergency vehicle1 detected!");
          digitalWrite(ledEmergency, HIGH);
          handleEmergencyVehicle1();
          emergencyVehicle1Detected = true;  // Set the flag
        }
      } else if (uidLength == sizeof(tag2) && memcmp(uid, tag2, sizeof(tag2)) == 0) {
        if (!emergencyVehicle2Detected) {
          Serial.println("Emergency vehicle2 detected!");
          digitalWrite(ledEmergency, HIGH);
          handleEmergencyVehicle2();
          emergencyVehicle2Detected = true;  // Set the flag
        }
      } else {
        Serial.println("Unknown vehicle detected!");
      }
    }
    digitalWrite(ledEmergency, LOW);
  } else {
    // If no tag is detected, reset the flags
    emergencyVehicle1Detected = false;
    emergencyVehicle2Detected = false;
    digitalWrite(ledEmergency, LOW);
  }
}

void handleEmergencyVehicle1() {
  server.handleClient();
  webSocket.loop();

  // Set yellow lights for all lanes before transition
  setYellowLightState(1, true);
  setYellowLightState(2, true);
  setYellowLightState(3, true);
  setYellowLightState(4, true);

  delay(3000); // Transition period

  // Turn off the yellow LEDs
  setYellowLightState(1, false);
  setYellowLightState(2, false);
  setYellowLightState(3, false);
  setYellowLightState(4, false);

  // Set traffic light states for emergency vehicle
  setTrafficLightState(1, false);
  setTrafficLightState(2, false);
  setTrafficLightState(3, true);  // Lane 2 green for emergency vehicle 1
  setTrafficLightState(4, false);

  // Set the highest density lane to lane 3 (emergency vehicle lane)
  highestDensityLane = 3;

  // Indicate that an emergency vehicle is detected
  bool emergencyDetected = true;

  while (vehicleCountLane3 != 0) {
    server.handleClient();
    webSocket.loop();
    vehicleCountLane3--; // Decrement vehicle count
    delay(1500);
    sendVehicleCountsOverWebSocket(emergencyDetected);
  }


  // Reset the vehicle count for lane 3
  vehicleCountLane3 = 0;
  emergencyDetected = false;
  sendVehicleCountsOverWebSocket(emergencyDetected); // Final update after emergency is handled

}

void handleEmergencyVehicle2() {
  server.handleClient();
  webSocket.loop();

  // Indicate that an emergency vehicle is detected
  bool emergencyDetected = true;

  // Set yellow lights for all lanes before transition
  setYellowLightState(1, true);
  setYellowLightState(2, true);
  setYellowLightState(3, true);
  setYellowLightState(4, true);

  delay(3000); // Transition period

  // Turn off the yellow LEDs
  setYellowLightState(1, false);
  setYellowLightState(2, false);
  setYellowLightState(3, false);
  setYellowLightState(4, false);

  // Set traffic light states for emergency vehicle
  setTrafficLightState(1, false);
  setTrafficLightState(2, false);
  setTrafficLightState(3, false);
  setTrafficLightState(4, true);  // Lane 4 green for emergency vehicle 2

  // Set the highest density lane to lane 4 (emergency vehicle lane)
  highestDensityLane = 4;



  // Send initial update to the web client
  sendVehicleCountsOverWebSocket(emergencyDetected);

  while (vehicleCountLane4 != 0) {
    server.handleClient();
    webSocket.loop();
    vehicleCountLane4--; // Decrement vehicle count
    delay(1500);
    sendVehicleCountsOverWebSocket(emergencyDetected);
  }


  // Reset the vehicle count for lane 3
  vehicleCountLane4 = 0;
  emergencyDetected = false;
  sendVehicleCountsOverWebSocket(emergencyDetected); // Final update after emergency is handled


}


void initialstate_allred() {
  digitalWrite(redLane1, HIGH);
  digitalWrite(yellowLane1, LOW);
  digitalWrite(greenLane1, LOW);
  digitalWrite(redLane2, HIGH);
  digitalWrite(yellowLane2, LOW);
  digitalWrite(greenLane2, LOW);
  digitalWrite(redLane3, HIGH);
  digitalWrite(yellowLane3, LOW);
  digitalWrite(greenLane3, LOW);
  digitalWrite(redLane4, HIGH);
  digitalWrite(yellowLane4, LOW);
  digitalWrite(greenLane4, LOW);
}

void dynamictraffic() {
  server.handleClient();
  webSocket.loop();
  // Determine if the traffic light is currently green for any lane
  bool isAnyGreen = digitalRead(greenLane1) || digitalRead(greenLane2) || digitalRead(greenLane3) || digitalRead(greenLane4);

  int currentGreenLane = 0;

  // Find the currently green lane
  if (digitalRead(greenLane1)) currentGreenLane = 1;
  else if (digitalRead(greenLane2)) currentGreenLane = 2;
  else if (digitalRead(greenLane3)) currentGreenLane = 3;
  else if (digitalRead(greenLane4)) currentGreenLane = 4;

  // Update vehicle counts for each lane
  if (isAnyGreen) {
    server.handleClient();
    webSocket.loop();
    // Decrease vehicle count for the currently green lane
    switch (currentGreenLane) {
      case 1:
        vehicleCountLane1 = static_cast<int>(max(vehicleCountLane1 - static_cast<int>(random(8, 15)), 0));
        //        vehicleCountLane1 += random(0, 11); // Adjust the range as needed
        vehicleCountLane2 += random(0, 11);
        vehicleCountLane3 += random(0, 11);
        vehicleCountLane4 += random(0, 11);
        break;

      case 2:
        vehicleCountLane2 = static_cast<int>(max(vehicleCountLane1 - static_cast<int>(random(8, 15)), 0));
        vehicleCountLane1 += random(0, 11); // Adjust the range as needed
        //        vehicleCountLane2 -= random(, 11);
        vehicleCountLane3 += random(0, 11);
        vehicleCountLane4 += random(0, 11);
        break;

      case 3:
        vehicleCountLane3 = static_cast<int>(max(vehicleCountLane1 - static_cast<int>(random(8, 15)), 0));
        vehicleCountLane1 += random(0, 11); // Adjust the range as needed
        vehicleCountLane2 += random(0, 11);
        //        vehicleCountLane3 += random(0, 11);
        vehicleCountLane4 += random(0, 11);
        break;

      case 4:
        vehicleCountLane4 = static_cast<int>(max(vehicleCountLane1 - static_cast<int>(random(8, 15)), 0));
        vehicleCountLane1 += random(0, 11); // Adjust the range as needed
        vehicleCountLane2 += random(0, 11);
        vehicleCountLane3 += random(0, 11);
        //        vehicleCountLane4 += random(0, 11);
        break;
    }

    // Check if the current lane now has the lowest vehicle count
    int lowestCount = vehicleCountLane1;
    int newHighestDensityLane = 1;

    if (vehicleCountLane2 < lowestCount) {
      lowestCount = vehicleCountLane2;
    }
    if (vehicleCountLane3 < lowestCount) {
      lowestCount = vehicleCountLane3;
    }
    if (vehicleCountLane4 < lowestCount) {
      lowestCount = vehicleCountLane4;
    }

    // Determine lane with the highest vehicle density that is not the current green lane
    if (vehicleCountLane1 > lowestCount && vehicleCountLane1 > vehicleCountLane2 && vehicleCountLane1 > vehicleCountLane3 && vehicleCountLane1 > vehicleCountLane4) {
      newHighestDensityLane = 1;
    } else if (vehicleCountLane2 > lowestCount && vehicleCountLane2 > vehicleCountLane1 && vehicleCountLane2 > vehicleCountLane3 && vehicleCountLane2 > vehicleCountLane4) {
      newHighestDensityLane = 2;
    } else if (vehicleCountLane3 > lowestCount && vehicleCountLane3 > vehicleCountLane1 && vehicleCountLane3 > vehicleCountLane2 && vehicleCountLane3 > vehicleCountLane4) {
      newHighestDensityLane = 3;
    } else if (vehicleCountLane4 > lowestCount && vehicleCountLane4 > vehicleCountLane1 && vehicleCountLane4 > vehicleCountLane2 && vehicleCountLane4 > vehicleCountLane3) {
      newHighestDensityLane = 4;
    }

    // Switch to the lane with the highest density if the current lane now has the lowest vehicle count
    if (currentGreenLane != newHighestDensityLane) {
      // Turn on yellow LEDs for both lanes involved in the transition
      setYellowLightState(currentGreenLane, true);
      setYellowLightState(newHighestDensityLane, true);

      delay(3000); // Transition period

      // Turn off the yellow LEDs
      setYellowLightState(currentGreenLane, false);
      setYellowLightState(newHighestDensityLane, false);

      // Switch traffic lights
      setTrafficLightState(currentGreenLane, false);
      setTrafficLightState(newHighestDensityLane, true);
    }
  } else {
    // If no lane is green, randomly increase or keep vehicle counts constant
    vehicleCountLane1 += random(0, 21); // Adjust the range as needed
    vehicleCountLane2 += random(0, 21);
    vehicleCountLane3 += random(0, 21);
    vehicleCountLane4 += random(0, 21);
  }

  Serial.print("Lane 1 vehicle count: ");
  Serial.println(vehicleCountLane1);

  Serial.print("Lane 2 vehicle count: ");
  Serial.println(vehicleCountLane2);

  Serial.print("Lane 3 vehicle count: ");
  Serial.println(vehicleCountLane3);

  Serial.print("Lane 4 vehicle count: ");
  Serial.println(vehicleCountLane4);

  Serial.println("");

  // Determine lane with the highest vehicle density
  int highestDensity = vehicleCountLane1;
  highestDensityLane = 1; // Reset highestDensityLane
  if (vehicleCountLane2 > highestDensity) {
    highestDensity = vehicleCountLane2;
    highestDensityLane = 2;
  }
  if (vehicleCountLane3 > highestDensity) {
    highestDensity = vehicleCountLane3;
    highestDensityLane = 3;
  }
  if (vehicleCountLane4 > highestDensity) {
    highestDensity = vehicleCountLane4;
    highestDensityLane = 4;
  }

  // Control traffic lights based on vehicle density
  setTrafficLightState(1, highestDensityLane == 1);
  setTrafficLightState(2, highestDensityLane == 2);
  setTrafficLightState(3, highestDensityLane == 3);
  setTrafficLightState(4, highestDensityLane == 4);

  // Send vehicle counts over WebSocket whenever they change
  sendVehicleCountsOverWebSocket(false);
}

void setTrafficLightState(int lane, bool green) {
  server.handleClient();
  webSocket.loop();
  switch (lane) {
    case 1:
      digitalWrite(redLane1, !green);
      digitalWrite(yellowLane1, LOW);
      digitalWrite(greenLane1, green);
      break;
    case 2:
      digitalWrite(redLane2, !green);
      digitalWrite(yellowLane2, LOW);
      digitalWrite(greenLane2, green);
      break;
    case 3:
      digitalWrite(redLane3, !green);
      digitalWrite(yellowLane3, LOW);
      digitalWrite(greenLane3, green);
      break;
    case 4:
      digitalWrite(redLane4, !green);
      digitalWrite(yellowLane4, LOW);
      digitalWrite(greenLane4, green);
      break;
    default:
      break;
  }
}

void setYellowLightState(int lane, bool yellow) {
  server.handleClient();
  webSocket.loop();
  switch (lane) {
    case 1:
      digitalWrite(yellowLane1, yellow);
      break;
    case 2:
      digitalWrite(yellowLane2, yellow);
      break;
    case 3:
      digitalWrite(yellowLane3, yellow);
      break;
    case 4:
      digitalWrite(yellowLane4, yellow);
      break;
    default:
      break;
  }
}

//void sendVehicleCountsOverWebSocket() {
//  String vehicleCounts = "";
//  StaticJsonDocument<200> doc;
//  JsonObject object = doc.to<JsonObject>();
//  object["lane1"] = vehicleCountLane1;
//  object["lane2"] = vehicleCountLane2;
//  object["lane3"] = vehicleCountLane3;
//  object["lane4"] = vehicleCountLane4;
//  object["movingLane"] = highestDensityLane; //include the moving lane status
//  serializeJson(doc, vehicleCounts);
//  Serial.println(vehicleCounts);
//  Serial.println("");
//  webSocket.broadcastTXT(vehicleCounts);
//}

void sendVehicleCountsOverWebSocket(bool emergencyDetected) {
  String vehicleCounts = "";
  StaticJsonDocument<200> doc;
  JsonObject object = doc.to<JsonObject>();
  object["lane1"] = vehicleCountLane1;
  object["lane2"] = vehicleCountLane2;
  object["lane3"] = vehicleCountLane3;
  object["lane4"] = vehicleCountLane4;
  object["movingLane"] = highestDensityLane; // include the moving lane status
  object["emergencyDetected"] = emergencyDetected; // emergency detection flag
  serializeJson(doc, vehicleCounts);
  Serial.println(vehicleCounts);
  Serial.println("");
  webSocket.broadcastTXT(vehicleCounts);
}
