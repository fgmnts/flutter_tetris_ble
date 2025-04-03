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

#include "rotary.h"

#define BUTTON_PIN 25
#define LONG_PRESS_DURATION 300  // Duration in milliseconds

volatile unsigned long buttonPressStartTime = 0;
volatile bool buttonPressed = false;
volatile unsigned long pressDuration = 0;
volatile bool hasFallen = 0;
volatile bool hasRisen = 0;
volatile unsigned long lastStableTime = 0;

// void IRAM_ATTR fallingEdgeISR() {
//   buttonPressStartTime = millis();
//   hasFallen = 1;
// }

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

void IRAM_ATTR risingEdgeISR() {
  unsigned long currentTime = millis();

  // if (currentTime - lastStableTime > DEBOUNCE_DELAY) {

  if (digitalRead(BUTTON_PIN) == 0) {
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
      hasFallen = 1;
    }
  } else {
    if (hasRisen == 0) {
      pressDuration = millis() - buttonPressStartTime;
      buttonPressStartTime = 0;
      hasRisen = 1;
    }
  }
  // }
}




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
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < rxValue.length(); i++)
        Serial.print(rxValue[i]);

      Serial.println();
      Serial.println("*********");
    }
  }
};


void setup() {
  Serial.begin(115200);
  setup_rotary();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), risingEdgeISR(), RISING);
  // attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), fallingEdgeISR(), FALLING);

  // attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), fallingEdgeISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), risingEdgeISR, CHANGE);

  // Create the BLE Device
  BLEDevice::init("ESP32Test");

  // Get the MAC address
  String address = BLEDevice::getAddress().toString().c_str();

  // Print the MAC address
  Serial.print("BLE MAC Address: ");
  Serial.println(address);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY);

  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->addServiceUUID(pService->getUUID());
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}
unsigned long last_notify = 0;

long last_val = 0;
bool prev_deviceConnected = 0;
void loop() {
  if (prev_deviceConnected != deviceConnected) {
    Serial.println("deviceConnected: " + String(deviceConnected));
  }
  prev_deviceConnected = deviceConnected;
  if (hasFallen) {
    Serial.println("Fallen");
    hasFallen = 0;
  }
  if (hasRisen) {
    Serial.println("Risen " + String(pressDuration));
    hasRisen = 0;
  }
  if (pressDuration > 0) {
    if (pressDuration >= LONG_PRESS_DURATION) {
      Serial.println("longpress");
      ble_send("longpress");
    } else if (pressDuration >= 30) {  // debounce
      Serial.println("shortpress");
      ble_send("shortpress");
    }
    pressDuration = 0;
  }
  // if (buttonPressed) {
  //   unsigned long pressDuration = millis() - buttonPressStartTime;
  //   if (pressDuration >= LONG_PRESS_DURATION) {
  //     Serial.println("longpress");
  //   } else {
  //     Serial.println("shortpress");
  //   }
  //   buttonPressed = false;  // Reset for next press detection
  // }
  // if (deviceConnected) {
  //dont print anything unless value changed
  if (rotaryEncoder.encoderChanged()) {
    Serial.print("Value: ");
    int val = rotaryEncoder.readEncoder();
    if (val < last_val) {
      Serial.println("Left");
      ble_send("left");
    } else if (val > last_val) {
      Serial.println("Right");
      ble_send("right");
    }
    last_val = val;
    Serial.println(val);

    // pTxCharacteristic->setValue(">Hello from the ESP<");
    // pTxCharacteristic->notify();
    // Serial.println("notify");
    // txValue++;
    delay(10);
  }
  // if (rotaryEncoder.isEncoderButtonClicked())
  // {
  // 	rotary_onButtonClick();
  // }
  // }

  // if (deviceConnected && millis() >= last_notify + 1000) {
  //   last_notify = millis();
  //   pTxCharacteristic->setValue(">Hello from the ESP<");
  //   pTxCharacteristic->notify();
  //   Serial.println("notify");
  //   txValue++;
  //   delay(10);  // bluetooth stack will go into congestion, if too many packets are sent
  // }

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