/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE"
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define JOYSTICK_X 38
#define JOYSTICK_Y 37
#define JOYSTICK_SW 36
// #define JOYSTICK_VCC D3
// #define JOYSTICK_GND D4

// Threshold for determining centered position
#define CENTER_THRESHOLD 600

// Center values (you might need to calibrate these)
int centerX = 2300;
int centerY = 2230;

void setup_sensors() {
  // Serial.begin(115200);

  // Configure pins
  // pinMode(JOYSTICK_VCC, OUTPUT);
  // pinMode(JOYSTICK_GND, OUTPUT);
  pinMode(JOYSTICK_SW, INPUT_PULLUP);

  // Power the joystick
  // digitalWrite(JOYSTICK_VCC, HIGH);
  // digitalWrite(JOYSTICK_GND, LOW);
}

String prev_event = "";
String cur_event = "";
bool prev_btn_state = false;
void loop_sensors() {
  int xVal = analogRead(JOYSTICK_X);
  int yVal = analogRead(JOYSTICK_Y);
  bool buttonPressed = digitalRead(JOYSTICK_SW) == LOW;  // LOW since it's PULLUP
  // Serial.println("RAW: " + String(xVal) + "   " + String(yVal));
  // Check for joystick movement
  cur_event = "";
  if (abs(xVal - centerX) > CENTER_THRESHOLD) {
    if (xVal > centerX) {
      // Serial.println("Left");
      cur_event = "left";
    } else {
      // Serial.println("Right");
      cur_event = "right";
    }
  }

  if (abs(yVal - centerY) > CENTER_THRESHOLD) {
    if (yVal > centerY) {
      // Serial.println("Up");
      cur_event = "up";
    } else {
      // Serial.println("Down");
      cur_event = "down";
    }
  }


  if (cur_event != prev_event && cur_event != "") {
    Serial.println("NEW EVENT: " + String(cur_event));
    ble_send(cur_event);
  }
  prev_event = cur_event;

  // Check for button press
  if (buttonPressed && prev_btn_state != buttonPressed) {
    Serial.println("Button Pressed EVENT");
    ble_send("shortpress");
  }
  prev_btn_state = buttonPressed;

  delay(50);  // Add a small delay to avoid flooding the serial monitor
}




BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
      }

      Serial.println();
      Serial.println("*********");
    }
  }
};

void ble_send(String msg) {
  if (deviceConnected) {
    // last_notify = millis();
    pTxCharacteristic->setValue(msg.c_str());
    pTxCharacteristic->notify();
    Serial.println("notify " + msg);
    // txValue++;
    delay(10);  // bluetooth stack will go into congestion, if too many packets are sent
  }
}
void setup() {
  Serial.begin(115200);
  setup_sensors();
  // Create the BLE Device
  BLEDevice::init("UART Service");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {

  if (deviceConnected) {
    pTxCharacteristic->setValue(&txValue, 1);
    pTxCharacteristic->notify();
    txValue++;
    delay(10);  // bluetooth stack will go into congestion, if too many packets are sent
  }
  loop_sensors();
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}
