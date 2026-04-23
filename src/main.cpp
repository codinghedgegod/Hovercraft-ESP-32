#include <Arduino.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define LED_PIN 2 

Servo motorL, motorR, steeringServo;
bool deviceConnected = false;
BLEServer* pServer = NULL; 

// --- CONFIGURATION ---
int rudderTrim = 0; 
int speedL = 120; 
int speedR = 120; 

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(LED_PIN, HIGH); 
      Serial.println(">>> HARDWARE: CONNECTED <<<");
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(LED_PIN, LOW); 
      motorL.write(40); motorR.write(40);
      steeringServo.write(90 + rudderTrim);
      
      delay(2000); 
      pServer->startAdvertising(); 
      Serial.println(">>> HARDWARE: DISCONNECTED - RE-ADVERTISING <<<");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        char prefix = value[0];
        
        // --- STEERING SLIDER (NEW) ---
        // Format expected from App: "C90", "C110", etc.
        if (prefix == 'C' && value.length() > 1 && isDigit(value[1])) {
          int angle = atoi(value.substr(1).c_str());
          if (angle >= 0 && angle <= 180) {
            steeringServo.write(angle + rudderTrim);
            Serial.print("Steering Slider: "); Serial.println(angle);
          }
        }
        // --- INDIVIDUAL SPEED SLIDERS ---
        else if (prefix == 'A') {
          speedL = atoi(value.substr(1).c_str());
          if (motorL.read() > 40) motorL.write(speedL);
          Serial.print("Left Speed: "); Serial.println(speedL);
        }
        else if (prefix == 'B') {
          speedR = atoi(value.substr(1).c_str());
          if (motorR.read() > 40) motorR.write(speedR);
          Serial.print("Right Speed: "); Serial.println(speedR);
        }
        // --- MOTOR BUTTONS ---
        else if (prefix == '1') { motorL.write(speedL); } 
        else if (prefix == '2') { motorL.write(40);     } 
        else if (prefix == '3') { motorR.write(speedR); } 
        else if (prefix == '4') { motorR.write(40);     } 
        // --- GLOBAL COMMANDS ---
        else if (prefix == 'F') { motorL.write(speedL); motorR.write(speedR); }
        else if (prefix == 'S') { motorL.write(40); motorR.write(40); }
        // --- BUTTON STEERING (BACKUP) ---
        else if (prefix == 'L') { steeringServo.write(45 + rudderTrim); }
        else if (prefix == 'R') { steeringServo.write(135 + rudderTrim); }
        else if (prefix == 'C') { steeringServo.write(90 + rudderTrim); }
      }
    }
};

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  motorL.attach(12, 1000, 2000);
  motorR.attach(13, 1000, 2000);
  steeringServo.attach(14);

  motorL.write(40); motorR.write(40);
  steeringServo.write(90 + rudderTrim); 
  delay(2000);

  BLEDevice::init("UNIT_01");
  pServer = BLEDevice::createServer(); 
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pChar = pService->createCharacteristic(
                                CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE |
                                BLECharacteristic::PROPERTY_NOTIFY |
                                BLECharacteristic::PROPERTY_WRITE_NR
                             );

  pChar->addDescriptor(new BLE2902());
  pChar->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  
  pAdvertising->setMinInterval(0x20); 
  pAdvertising->setMaxInterval(0x40); 
  
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMinPreferred(0x12);
  
  pServer->getAdvertising()->start();
  Serial.println("System Ready. High-Speed Advertising Active.");
}

void loop() {}