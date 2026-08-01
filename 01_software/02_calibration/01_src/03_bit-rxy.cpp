#include "01_includes.h"

// ===========================================================================
// BLE control layer for the wheel-calibration sketch — same protocol as the
// main app's 03_bit-rxy.cpp (ported straight from it, see that file's own
// comments for the full GETCFG/CFG handshake and rc=6/BLE_HS_ENOMEM
// post-mortem this is built around), but with a purpose-built layout: two
// sliders drive each servo's raw pulse (0-180) directly, with no arcade
// mixing, no joystick, no gauges — just enough to find each wheel's true
// stop point and write it into 00_config.h in 01_app.
//
// Protocol summary:
//   app -> device : "GETCFG"            (sent ~500ms after connect)
//   app -> device : "SET <id> <val...>"
//   device -> app : "CFGBEGIN" / "CFG <18-char chunk>"... / "CFGEND"
// Transport: Nordic-UART-style GATT service, roles reversed to match the
// micro:bit's convention (0002 = notify device->app, 0003 = write app->device).
// ===========================================================================

#include <NimBLEDevice.h>

#define UART_SERVICE_UUID   "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define UART_TX_CHAR_UUID   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // notify
#define UART_RX_CHAR_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // write

// Layout CFG: LAYOUT_CFG_BASE64 below is this JSON, base64-encoded. Regenerate
// with layout_cfg.sh (decode/encode) after editing, or by hand:
//   python3 -c "import json,base64; print(base64.b64encode(json.dumps(<paste dict here>,separators=(',',':')).encode()).decode())"
//
// {
//   "schemaVersion": 1,
//   "title": "WDIY Calib",
//   "widgets": [
//     {"id":"slider_left","t":"slider","x":20,"y":20,"w":90,"h":200,"label":"Left","min":0,"max":180,"step":1,"model":"track"},
//     {"id":"edit_left","t":"editfield","x":20,"y":230,"w":90,"h":60,"label":"Left #"},
//     {"id":"slider_right","t":"slider","x":290,"y":20,"w":90,"h":200,"label":"Right","min":0,"max":180,"step":1,"model":"track"},
//     {"id":"edit_right","t":"editfield","x":290,"y":230,"w":90,"h":60,"label":"Right #"},
//     {"id":"btn_center","t":"button","x":140,"y":60,"w":120,"h":120,"label":"Center","model":"neo"}
//   ]
// }
//
// Widget -> firmware mapping:
//   slider_left / edit_left   -> both write the same s_pulse_left  (0-180) —
//   slider_right / edit_right -> both write the same s_pulse_right (0-180) —
//     drag the slider for coarse adjustment, or type an exact number into
//     the matching Edit Field for fine adjustment; whichever was touched
//     most recently wins, same "shared state" pattern as the main app's
//     joystick/D-Pad. Each Edit Field sits directly under its slider
//     (same x) so the pairing is visually obvious; the Center button sits
//     between the two columns, both horizontally and vertically centered.
//   btn_center                -> resets both back to 90 (stop)
static const char* LAYOUT_CFG_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6IldESVkgQ2FsaWIiLCJ3aWRnZXRzIjpbeyJp"
  "ZCI6InNsaWRlcl9sZWZ0IiwidCI6InNsaWRlciIsIngiOjIwLCJ5IjoyMCwidyI6OTAsImgi"
  "OjIwMCwibGFiZWwiOiJMZWZ0IiwibWluIjowLCJtYXgiOjE4MCwic3RlcCI6MSwibW9kZWwi"
  "OiJ0cmFjayJ9LHsiaWQiOiJlZGl0X2xlZnQiLCJ0IjoiZWRpdGZpZWxkIiwieCI6MjAsInki"
  "OjIzMCwidyI6OTAsImgiOjYwLCJsYWJlbCI6IkxlZnQgIyJ9LHsiaWQiOiJzbGlkZXJfcmln"
  "aHQiLCJ0Ijoic2xpZGVyIiwieCI6MjkwLCJ5IjoyMCwidyI6OTAsImgiOjIwMCwibGFiZWwi"
  "OiJSaWdodCIsIm1pbiI6MCwibWF4IjoxODAsInN0ZXAiOjEsIm1vZGVsIjoidHJhY2sifSx7"
  "ImlkIjoiZWRpdF9yaWdodCIsInQiOiJlZGl0ZmllbGQiLCJ4IjoyOTAsInkiOjIzMCwidyI6"
  "OTAsImgiOjYwLCJsYWJlbCI6IlJpZ2h0ICMifSx7ImlkIjoiYnRuX2NlbnRlciIsInQiOiJi"
  "dXR0b24iLCJ4IjoxNDAsInkiOjYwLCJ3IjoxMjAsImgiOjEyMCwibGFiZWwiOiJDZW50ZXIi"
  "LCJtb2RlbCI6Im5lbyJ9XX0=";

// ===========================================================================
// State
// ===========================================================================
static NimBLECharacteristic* s_txChar     = nullptr;
static volatile bool         s_connected  = false;
static volatile bool         s_sendingCfg = false;
static String                s_rxBuffer;
static volatile bool         s_getCfgRequested = false;

// 0..180 raw servo pulse, defaults to stop (90) until the app sends a slider
// value — same defaults 09_servos.cpp already expects at boot.
static int16_t s_pulse_left  = 90;
static int16_t s_pulse_right = 90;

static void handleLine(const String& line);
static void handleWidget(const String& id, const String& val);
static bool sendLine(const String& line);
static void sendCfg();

// ===========================================================================
// BLE callbacks
// ===========================================================================
class RemoteXYServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
    s_connected = true;
    s_rxBuffer  = "";
    #ifdef DEF_DERIAL_DEBUG
    Serial.printf("[BLE] Client connected  peer=%s\n", info.getAddress().toString().c_str());
    #endif
    server->updateConnParams(info.getConnHandle(), 6, 12, 0, 400);
  }
  void onDisconnect(NimBLEServer* /*server*/, NimBLEConnInfo& /*info*/, int reason) override {
    s_connected = false;
    // Stop both wheels on disconnect, same safety rule as the main app.
    s_pulse_left  = 90;
    s_pulse_right = 90;
    #ifdef DEF_DERIAL_DEBUG
    Serial.printf("[BLE] Client disconnected (reason 0x%02x) - re-advertising\n", reason);
    #endif
    NimBLEDevice::startAdvertising();
  }
};

class RemoteXYRxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo& /*info*/) override {
    std::string v = chr->getValue();
    #ifdef DEF_DERIAL_DEBUG
    Serial.printf("[BLE] RX %u bytes: '%s'\n", (unsigned)v.size(), v.c_str());
    #endif
    for (size_t i = 0; i < v.size(); ++i) {
      const char c = v[i];
      if (c == '\r') continue;
      if (c == '\n') {
        if (s_rxBuffer.length() > 0) {
          handleLine(s_rxBuffer);
          s_rxBuffer = "";
        }
      } else {
        s_rxBuffer += c;
        if (s_rxBuffer.length() > 256) s_rxBuffer = "";  // overflow guard
      }
    }
  }
};

// ===========================================================================
// Protocol
// ===========================================================================
static bool sendLine(const String& line) {
  if (!s_connected || s_txChar == nullptr) return false;
  String out = line + "\n";
  return s_txChar->notify((const uint8_t*)out.c_str(), out.length());
}

static void sendCfg() {
  s_sendingCfg = true;
  delay(300);  // let the fast-connection-interval request land first
  sendLine("CFGBEGIN");
  const char* p  = LAYOUT_CFG_BASE64;
  const size_t n = strlen(p);
  const size_t CHUNK = 18;
  int dropped = 0;
  for (size_t i = 0; i < n; i += CHUNK) {
    String line = "CFG ";
    for (size_t j = 0; j < CHUNK && (i + j) < n; ++j) line += p[i + j];
    if (!sendLine(line)) dropped++;
    delay(50);
  }
  sendLine("CFGEND");
  s_sendingCfg = false;
  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[BLE] Sent CFG (dropped=%d)\n", dropped);
  #endif
}

static void handleLine(const String& line) {
  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[BLE] RX line: '%s'\n", line.c_str());
  #endif

  // Deferred to remotexy_handler() — never call sendCfg() directly from
  // here. See the main app's 03_bit-rxy.cpp for the full post-mortem on
  // why running the CFG burst synchronously from this callback (NimBLE's
  // own host task) causes rc=6/BLE_HS_ENOMEM.
  if (line == "GETCFG") { s_getCfgRequested = true; return; }

  if (line.startsWith("SET ")) {
    int sp = line.indexOf(' ', 4);
    if (sp < 0) return;
    String id  = line.substring(4, sp);
    String val = line.substring(sp + 1);
    handleWidget(id, val);
  }
}

static void handleWidget(const String& id, const String& val) {
  // slider_* and edit_* both write the same pulse state — see the layout
  // comment above. Edit Field lets you type an exact value the slider's
  // drag resolution can't reliably hit.
  if (id == "slider_left"  || id == "edit_left")  { s_pulse_left  = (int16_t)constrain(val.toInt(), 0, 180); return; }
  if (id == "slider_right" || id == "edit_right") { s_pulse_right = (int16_t)constrain(val.toInt(), 0, 180); return; }
  if (id == "btn_center" && val == "1") { s_pulse_left = 90; s_pulse_right = 90; return; }
}

// ===========================================================================
// Public API
// ===========================================================================
void remotexy_init(void) {
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("[BLE] init - device_name: " CONFIG_BLE_DEVICE_NAME);
  #endif

  NimBLEDevice::init(CONFIG_BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::deleteAllBonds();

  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[BLE] local_mac: %s\n", NimBLEDevice::getAddress().toString().c_str());
  #endif

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new RemoteXYServerCallbacks());

  NimBLEService* svc = server->createService(UART_SERVICE_UUID);

  s_txChar = svc->createCharacteristic(UART_TX_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);

  NimBLECharacteristic* rxChar = svc->createCharacteristic(
      UART_RX_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RemoteXYRxCallbacks());

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(CONFIG_BLE_DEVICE_NAME);
  adv->enableScanResponse(false);
  adv->setMinInterval(0x140);
  adv->setMaxInterval(0x140);
  NimBLEDevice::startAdvertising();

  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[BLE] service_uuid: %s\n", UART_SERVICE_UUID);
  Serial.printf("[BLE] tx_char_uuid: %s (notify)\n", UART_TX_CHAR_UUID);
  Serial.printf("[BLE] rx_char_uuid: %s (write)\n", UART_RX_CHAR_UUID);
  Serial.printf("[BLE] advertising: %s\n", (adv && adv->isAdvertising()) ? "YES" : "no");
  #endif
}

void remotexy_handler(void) {
  if (s_getCfgRequested) {
    s_getCfgRequested = false;
    sendCfg();
  }
}

int16_t remotexy_get_pulse_left( ) {
  return s_pulse_left;
}

int16_t remotexy_get_pulse_right( ) {
  return s_pulse_right;
}

uint8_t remotexy_get_connect_flag( ) {
  return s_connected ? 1 : 0;
}
