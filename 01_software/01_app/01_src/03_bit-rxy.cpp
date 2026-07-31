#include "01_includes.h"

// ===========================================================================
// BLE control layer — speaks the same protocol as a BBC micro:bit running
// the rxy MakeCode template (https://abourdim.github.io/bit-rxy/), so the
// unmodified web app can drive this robot's widgets over Web Bluetooth.
//
// The function names below are kept identical to the old RemoteXY adapter
// so none of the calling code (04_tasks.cpp, 01_src.ino, 11_events.cpp) had
// to change — only what's behind them did.
//
// Protocol summary:
//   app -> device : "GETCFG"            (sent ~500ms after connect)
//   app -> device : "SET <id> <val...>"
//   device -> app : "CFGBEGIN" / "CFG <18-char chunk>"... / "CFGEND"
//   device -> app : "UPD <id> <val>"
// Transport: Nordic-UART-style GATT service, roles reversed to match the
// micro:bit's convention (0002 = notify device->app, 0003 = write app->device).
// ===========================================================================

#include <NimBLEDevice.h>

#define UART_SERVICE_UUID   "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define UART_TX_CHAR_UUID   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // notify
#define UART_RX_CHAR_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // write

// Layout CFG: LAYOUT_CFG_BASE64 below is this JSON, base64-encoded. Regenerate
// with layout_cfg.sh (decode/encode) after editing 01_app/layout_cfg.sh's
// input JSON, or by hand:
//   python3 -c "import json,base64; print(base64.b64encode(json.dumps(<paste dict here>,separators=(',',':')).encode()).decode())"
//
// {
//   "schemaVersion": 1,
//   "title": "WDIY Robot b3",
//   "widgets": [
//     {"id":"joy_drive","t":"joystick","x":20,"y":20,"w":140,"h":140,"label":"Drive","model":"classic"},
//     {"id":"btn_horn","t":"button","x":200,"y":50,"w":100,"h":100,"label":"Horn","model":"neo"},
//     {"id":"dpad_drive","t":"dpad","x":340,"y":20,"w":140,"h":140,"label":"Drive","model":"classic"},
//     {"id":"gauge_speed","t":"gauge","x":20,"y":180,"w":140,"h":160,"label":"Speed","min":0,"max":100,"units":"%","decimals":0,"model":"classic"},
//     {"id":"gauge_distance","t":"gauge","x":180,"y":180,"w":140,"h":160,"label":"Distance","min":0,"max":200,"units":"cm","decimals":0,"model":"classic"},
//     {"id":"battery_level","t":"battery","x":340,"y":200,"w":80,"h":100,"label":"Battery","model":"vertical"},
//     {"id":"sound_alert","t":"sound","x":440,"y":180,"w":90,"h":90,"label":"Alert"}
//   ]
// }
//
// Widget -> firmware mapping:
//   joy_drive / dpad_drive  -> both write the same s_joy_x/s_joy_y (steering)
//   btn_horn                -> horn/alarm (s_button_01)
//   gauge_speed             -> motor speed magnitude 0-100 (output only)
//   gauge_distance          -> ultrasonic distance 0-200cm (output only)
//   battery_level           -> battery percentage 0-100 (output only)
//   sound_alert             -> plays a tone on the phone (output only) —
//                              restores the old RemoteXY sound_01 element,
//                              which had no bit-rxy equivalent until the
//                              Sound widget was added
static const char* LAYOUT_CFG_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6IldESVkgUm9ib3QgYjMiLCJ3aWRnZXRzIjpb"
  "eyJpZCI6ImpveV9kcml2ZSIsInQiOiJqb3lzdGljayIsIngiOjIwLCJ5IjoyMCwidyI6MTQw"
  "LCJoIjoxNDAsImxhYmVsIjoiRHJpdmUiLCJtb2RlbCI6ImNsYXNzaWMifSx7ImlkIjoiYnRu"
  "X2hvcm4iLCJ0IjoiYnV0dG9uIiwieCI6MjAwLCJ5Ijo1MCwidyI6MTAwLCJoIjoxMDAsImxh"
  "YmVsIjoiSG9ybiIsIm1vZGVsIjoibmVvIn0seyJpZCI6ImRwYWRfZHJpdmUiLCJ0IjoiZHBh"
  "ZCIsIngiOjM0MCwieSI6MjAsInciOjE0MCwiaCI6MTQwLCJsYWJlbCI6IkRyaXZlIiwibW9k"
  "ZWwiOiJjbGFzc2ljIn0seyJpZCI6ImdhdWdlX3NwZWVkIiwidCI6ImdhdWdlIiwieCI6MjAs"
  "InkiOjE4MCwidyI6MTQwLCJoIjoxNjAsImxhYmVsIjoiU3BlZWQiLCJtaW4iOjAsIm1heCI6"
  "MTAwLCJ1bml0cyI6IiUiLCJkZWNpbWFscyI6MCwibW9kZWwiOiJjbGFzc2ljIn0seyJpZCI6"
  "ImdhdWdlX2Rpc3RhbmNlIiwidCI6ImdhdWdlIiwieCI6MTgwLCJ5IjoxODAsInciOjE0MCwi"
  "aCI6MTYwLCJsYWJlbCI6IkRpc3RhbmNlIiwibWluIjowLCJtYXgiOjIwMCwidW5pdHMiOiJj"
  "bSIsImRlY2ltYWxzIjowLCJtb2RlbCI6ImNsYXNzaWMifSx7ImlkIjoiYmF0dGVyeV9sZXZl"
  "bCIsInQiOiJiYXR0ZXJ5IiwieCI6MzQwLCJ5IjoyMDAsInciOjgwLCJoIjoxMDAsImxhYmVs"
  "IjoiQmF0dGVyeSIsIm1vZGVsIjoidmVydGljYWwifSx7ImlkIjoic291bmRfYWxlcnQiLCJ0"
  "Ijoic291bmQiLCJ4Ijo0NDAsInkiOjE4MCwidyI6OTAsImgiOjkwLCJsYWJlbCI6IkFsZXJ0"
  "In1dfQ==";

// ===========================================================================
// State
// ===========================================================================
static NimBLECharacteristic* s_txChar     = nullptr;
static volatile bool         s_connected  = false;
// Guards against a future periodic-output task racing sendCfg()'s burst,
// same as the reference firmware's gSendingCfg.
static volatile bool         s_sendingCfg = false;
static String                s_rxBuffer;

// Set from onWrite() (NimBLE's own host task), consumed from remotexy_handler()
// (the ordinary Arduino loop() task). Ported from esp32-rxy's post-mortem fix:
// calling sendCfg() — a ~900ms burst of ~60 notify() calls — directly and
// synchronously from inside onWrite() blocks NimBLE's host task from doing
// its own buffer-completion housekeeping for the whole burst, starving the
// notify() mbuf pool and causing most sends to fail with rc=6 (ENOMEM).
// Deferring the burst to loop() lets the host task keep servicing itself
// concurrently, so the pool never starves.
static volatile bool         s_getCfgRequested = false;

static int8_t s_joy_x = 0;   // -100..100, drives tasks_joysticks() (turn)
static int8_t s_joy_y = 0;   // -100..100, drives tasks_joysticks() (forward/back)
static uint8_t s_button_01 = 0;

// dpad_drive writes the same s_joy_x/s_joy_y as joy_drive — both widgets
// drive the identical robot state, whichever was touched most recently wins.
static bool s_dpad_up = false, s_dpad_down = false, s_dpad_left = false, s_dpad_right = false;

static void handleLine(const String& line);
static void handleWidget(const String& id, const String& val);
static bool sendLine(const String& line);
static void sendCfg();

static inline void sendValue(const String& id, const String& val) {
  if (s_sendingCfg) return;  // don't interleave widget updates with a CFG burst
  sendLine("UPD " + id + " " + val);
}

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
    // Request a fast connection interval (7.5-15ms, units of 1.25ms) so the
    // controller drains notify()'s buffer pool quickly enough to survive
    // sendCfg()'s ~60-chunk burst.
    server->updateConnParams(info.getConnHandle(), 6, 12, 0, 400);
  }
  void onDisconnect(NimBLEServer* /*server*/, NimBLEConnInfo& /*info*/, int reason) override {
    s_connected = false;
    s_joy_x = 0;
    s_joy_y = 0;
    s_button_01 = 0;
    s_dpad_up = s_dpad_down = s_dpad_left = s_dpad_right = false;
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
  // onConnect() requests a fast connection interval via updateConnParams(),
  // but that's an async negotiation with the central — it takes a round
  // trip or more to actually take effect. The app sends GETCFG almost
  // immediately after connecting, so without this delay the burst below
  // starts while still on the central's slower default interval, which
  // can't drain notify()'s buffer pool fast enough (rc=6 / BLE_HS_ENOMEM
  // on nearly every packet). Give the renegotiation time to land first.
  delay(300);
  sendLine("CFGBEGIN");
  const char* p  = LAYOUT_CFG_BASE64;
  const size_t n = strlen(p);
  const size_t CHUNK = 18;  // matches the rxy MakeCode template
  int dropped = 0;
  for (size_t i = 0; i < n; i += CHUNK) {
    String line = "CFG ";
    for (size_t j = 0; j < CHUNK && (i + j) < n; ++j) line += p[i + j];
    // 50ms (vs. the reference firmware's 15ms): this sketch also runs
    // FastLED/OLED/Servo alongside NimBLE, leaving less controller buffer
    // headroom than the reference's minimal sketch, so the same burst
    // rate that worked there (rc=6 / BLE_HS_ENOMEM) overflows here.
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

  // Defer to remotexy_handler() — do NOT call sendCfg() directly here.
  // See s_getCfgRequested above for why running the burst synchronously
  // from this callback (NimBLE's own host task) was a real, documented bug.
  if (line == "GETCFG") { s_getCfgRequested = true; return; }

  if (line.startsWith("SET ")) {
    int sp = line.indexOf(' ', 4);
    if (sp < 0) return;
    String id  = line.substring(4, sp);
    String val = line.substring(sp + 1);
    handleWidget(id, val);
  }
}

// Converts the joystick's "angle distance" (angle: 0deg=right, 90deg=up,
// 180deg=left, 270deg=down — standard math convention, confirmed empirically
// against the live app; the "90=down" convention documented in some rxy
// reference comments does not match actual runtime behavior) into the
// -100..100 x/y pair tasks_joysticks() expects (x=turn right, y=forward).
static void handleJoystick(const String& val) {
  int sp = val.indexOf(' ');
  if (sp < 0) return;
  float angleDeg = val.substring(0, sp).toFloat();
  float dist     = val.substring(sp + 1).toFloat();
  float rad = angleDeg * PI / 180.0f;
  s_joy_x = (int8_t)constrain((long)round(dist * cos(rad)), -100, 100);
  // Negated: the robot's forward/back sense turned out to be the mirror
  // of the joystick's up/down sense (confirmed empirically) — turning
  // (x) was already correct, only this axis needed flipping.
  s_joy_y = (int8_t)constrain((long)round(-dist * sin(rad)), -100, 100);
}

// D-Pad sends "<up|down|left|right> <0|1>" immediately on press/release
// (unlike the joystick, which only streams while actively dragging), so it
// drives s_joy_x/s_joy_y at a fixed full-speed magnitude from the combined
// state of whichever direction buttons are currently held.
static const int8_t DPAD_SPEED = 100;

static void handleDpad(const String& val) {
  int sp = val.indexOf(' ');
  if (sp < 0) return;
  String dir     = val.substring(0, sp);
  bool   pressed = val.substring(sp + 1) == "1";

  if      (dir == "up")    s_dpad_up    = pressed;
  else if (dir == "down")  s_dpad_down  = pressed;
  else if (dir == "left")  s_dpad_left  = pressed;
  else if (dir == "right") s_dpad_right = pressed;
  else return;

  s_joy_x = (int8_t)((s_dpad_right ? DPAD_SPEED : 0) - (s_dpad_left ? DPAD_SPEED : 0));
  // Same forward/back mirror as the joystick (see handleJoystick) — down
  // is physically forward on this chassis.
  s_joy_y = (int8_t)((s_dpad_down  ? DPAD_SPEED : 0) - (s_dpad_up   ? DPAD_SPEED : 0));
}

static void handleWidget(const String& id, const String& val) {
  if (id == "joy_drive")   { handleJoystick(val); return; }
  if (id == "dpad_drive")  { handleDpad(val); return; }
  if (id == "btn_horn")    { s_button_01 = (val == "1") ? 1 : 0; return; }
}

// ===========================================================================
// Public API (same signatures as the old RemoteXY adapter)
// ===========================================================================
void remotexy_init(void) {
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("[BLE] init - device_name: " CONFIG_BLE_DEVICE_NAME);
  #endif

  NimBLEDevice::init(CONFIG_BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  // No explicit setMTU() call — NimBLE-Arduino's own built-in default
  // preferred MTU (255) already gives plenty of room for a 23-byte
  // "CFG <18-char>\n" line.
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
  adv->setMinInterval(0x140);  // 320 * 0.625ms = 200ms
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
  // Runs the deferred CFG burst on the ordinary Arduino task, not on
  // NimBLE's host task — see s_getCfgRequested for why that matters.
  if (s_getCfgRequested) {
    s_getCfgRequested = false;
    sendCfg();
  }
}

int8_t remotexy_get_joystick_01_x( ) {
  return s_joy_x;
}

int8_t remotexy_get_joystick_01_y( ) {
  return s_joy_y;
}

uint8_t remotexy_get_button_01( ) {
  return s_button_01;
}

float remotexy_get_onlineGraph_01_distance( ) {
  return 0;  // output-only widget, never read back
}

float remotexy_get_onlineGraph_02_speed( ) {
  return 0;
}

float remotexy_get_onlineGraph_03_battery( ) {
  return 0;
}

int8_t remotexy_get_circularBar_01( ) {
  return 0;
}

int16_t remotexy_get_sound_01( ) {
  return 0;
}

uint8_t remotexy_get_connect_flag( ) {
  return s_connected ? 1 : 0;
}

// ===========================================================================
// ===========================================================================
// ===========================================================================

void remotexy_set_joystick_01_x( int8_t p_joystick_01_x) {
  (void)p_joystick_01_x;  // not writable from firmware side
}

void remotexy_set_joystick_01_y( int8_t p_joystick_01_y) {
  (void)p_joystick_01_y;
}

void remotexy_set_button_01(uint8_t p_button_01) {
  (void)p_button_01;
}

void remotexy_set_onlineGraph_01_distance(float p_onlineGraph_01_distance) {
  sendValue("gauge_distance", String(p_onlineGraph_01_distance, 0));
}

void remotexy_set_onlineGraph_02_speed( float p_onlineGraph_02_speed) {
  sendValue("gauge_speed", String(p_onlineGraph_02_speed, 0));
}

void remotexy_set_onlineGraph_03_battery( float p_onlineGraph_03_battery) {
  sendValue("battery_level", String(p_onlineGraph_03_battery, 0));
}

void remotexy_set_circularBar_01( int8_t p_circularBar_01) {
  (void)p_circularBar_01;  // covered by gauge_speed already
}

// Maps RemoteXY's sound catalog (07_sounds.h) onto bit-rxy's Sound widget,
// which only has 5 tone effects (see bit-rxy's playSoundEffect()) rather
// than a full audio-file library. p_sound_01 == 0 is tasks_rmotexy_sound()'s
// periodic "no sound" reset — must not itself trigger a sound.
void remotexy_set_sound_01(int16_t p_sound_01) {
  if (p_sound_01 == 0) return;

  const char* effect;
  switch (p_sound_01) {
    case REMOTEXY_SOUND_SUCCESS:
    case REMOTEXY_SOUND_SUCCESS_ALT:
    case REMOTEXY_SOUND_SUCCESS_BEEP:
    case REMOTEXY_SOUND_POWER_ON:
    case REMOTEXY_SOUND_CONFIRM:
    case REMOTEXY_SOUND_CHIME:
      effect = "success"; break;
    case REMOTEXY_SOUND_WARNING:
    case REMOTEXY_SOUND_TIMER:
    case REMOTEXY_SOUND_COUNTDOWN:
      effect = "warn"; break;
    case REMOTEXY_SOUND_ERROR:
    case REMOTEXY_SOUND_CRITICAL:
    case REMOTEXY_SOUND_ALARM:
    case REMOTEXY_SOUND_FAILURE:
    case REMOTEXY_SOUND_ERROR_BEEP:
      effect = "danger"; break;
    case REMOTEXY_SOUND_DOUBLE_BEEP:
    case REMOTEXY_SOUND_SELECT:
      effect = "toggle"; break;
    default:
      effect = "beep"; break;  // BEEP, BEEP_SHORT, BEEP_LONG, CLICK, TAP, etc.
  }
  sendValue("sound_alert", effect);
}

void remotexy_set_connect_flag( uint8_t p_connect_flag) {
  (void)p_connect_flag;  // connect state is owned by the BLE server callbacks
}
