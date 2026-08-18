/**
 * TEST 2: BLUETOOTH — flash this alone.
 *
 * Advertises and does nothing else: no motors, no sensors, no layout. It
 * separates "the radio works" from "the robot works", which the app-driven
 * test panels cannot do — on those, a dead subsystem and a failed connection
 * look identical to a child.
 *
 * PASS: a scanner (or the app's Connect dialog) shows "diy_app_b3_bletest",
 *       and the green LED goes solid while a phone is connected.
 * FAIL: never appears        -> radio or firmware fault, stop here; nothing
 *                               app-driven downstream can be trusted
 *       appears, won't connect -> pairing/cache on the phone, try forgetting
 *                               the device
 */
#include <NimBLEDevice.h>

#define PIN_LED_GREEN 1
#define DEVICE_NAME   "diy_app_b3_bletest"

static volatile bool s_connected = false;

class Cb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    s_connected = true;
    Serial.printf("connected: %s\n", info.getAddress().toString().c_str());
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    s_connected = false;
    Serial.printf("disconnected (0x%02x) - re-advertising\n", reason);
    NimBLEDevice::startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_GREEN, OUTPUT);
  Serial.println("\n[TEST] BLE - advertising as " DEVICE_NAME);

  NimBLEDevice::init(DEVICE_NAME);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new Cb());
  // Same UART service the real firmware uses, so a scanner shows the same
  // shape of device and this is a fair test of the actual configuration.
  NimBLEService* svc = server->createService("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
  svc->createCharacteristic("6e400002-b5a3-f393-e0a9-e50e24dcca9e", NIMBLE_PROPERTY::NOTIFY);
  svc->createCharacteristic("6e400003-b5a3-f393-e0a9-e50e24dcca9e", NIMBLE_PROPERTY::WRITE);
  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(svc->getUUID());
  adv->setName(DEVICE_NAME);
  NimBLEDevice::startAdvertising();
}

void loop() {
  // Solid while connected, slow blink while only advertising — readable from
  // across a classroom without a serial monitor.
  if (s_connected) { digitalWrite(PIN_LED_GREEN, HIGH); delay(200); }
  else { digitalWrite(PIN_LED_GREEN, HIGH); delay(100);
         digitalWrite(PIN_LED_GREEN, LOW);  delay(900); }
}
