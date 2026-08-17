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
//   device -> app : "CFGBEGIN" / "CFG <chunk>"... / "CFGEND"   (chunk size is
//                   negotiated from the MTU — see sendCfg())
//   device -> app : "UPD <id> <val>"
//
// Two apps drive this robot and they do not agree on the D-pad or the
// joystick, so both encodings of each are accepted. Stock bit-rxy's forms are
// listed first and are handled exactly as before — nothing here changes them:
//
//   D-pad     "SET <id> <up|down|left|right> <0|1>"   bit-rxy, one direction
//             "<a..p>"                                mecanum app, 1-byte mask
//             "M <0..15>"                             text form of that mask
//   joystick  "SET <id> <angle> <distance>"           bit-rxy
//             "SET <id> <x> <y>"                      mecanum app
//
// The two D-pad forms are unambiguous (a mask line is one byte, or starts with
// "M "). The joystick pair is not, so see handleJoystick() for how it is told
// apart. "GETCFGVER" is recognised but deliberately unanswered — it only
// records which app is talking; see handleLine().
// Transport: Nordic-UART-style GATT service, roles reversed to match the
// micro:bit's convention (0002 = notify device->app, 0003 = write app->device).
// ===========================================================================

#include <NimBLEDevice.h>
#include <Preferences.h>   // NVS-backed store for the Beginner/Expert choice

#define UART_SERVICE_UUID   "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define UART_TX_CHAR_UUID   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // notify
#define UART_RX_CHAR_UUID   "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // write

// Layout CFG: two layouts are compiled in, and the Level select in the SYSTEM
// zone switches between them at runtime (see handleWidget's "level" case).
// Their sources are 01_app/layout_beginner.json and layout_expert.json —
// regenerate either with
//   ./layout_cfg.sh encode layout_expert.json
//   ./layout_cfg.sh decode LAYOUT_CFG_EXPERT_BASE64
// and paste the output over the matching chunks below.
//
//   BEGINNER  3 zones, 9 controls, canvas 818x760
//     DRIVE    (#00d4ff)  joy_drive, btn_horn, dpad_drive
//     SENSING  (#ffb020)  gauge_speed, gauge_distance, sound_alert
//     SYSTEM   (#3ddc97)  battery_level, level, logo
//
//   EXPERT    5 zones, 28 controls, canvas 1230x1270
//     DRIVE     (#00d4ff)  joy_drive, dpad_drive, spd, btn_stop, gauge_speed
//     DISTANCE  (#ffb020)  gauge_distance, alert, graph_dist
//     LIGHTS    (#7c5cff)  toggle_led_r, toggle_led_g, toggle_np, np_effect,
//                          np_r, np_g, np_b, np_bright
//     SOUND     (#ff5c8a)  btn_horn, btn_buzz, sound_alert
//     SYSTEM    (#3ddc97)  battery_level, lbl_vbat, lbl_ver, lbl_uptime,
//                          upd, level, led_button, gauge_rssi, logo
//
// Every id in the beginner layout also exists in the expert one with the same
// type and meaning, so switching never changes what a control does — it only
// reveals more of them. `level` itself is in both, which is what makes the
// switch reversible.
//
// Groups and the three separators are visual-only: they carry no state, are
// never SET or UPD targets, and the controls stay ordinary top-level CFG
// widgets, so a client that does not know these types can ignore the grouping
// metadata. Groups are listed first so they render behind the controls; each
// control also carries groupId so a zone drags as one unit.
//
// Not mirrored from the Keyes 4WD layout: an AUTONOMY zone. This robot has no
// line sensors and no avoid mode, so a Mode selector would be decoration.
//
// The Red/Green LED toggles need care, because unlike the Keyes robot's
// headlights these two pins are the link-status indicator (leds_update()).
// The app therefore does not own them by default: the first toggle takes
// ownership, and onDisconnect() gives it back, so a disconnect is always
// visible even if the user left both LEDs switched off.
//
// The logo is referenced as a path relative to the app's index.html, not
// embedded. The CFG is base64'd twice on its way to the browser, so an inline
// data URI would cost ~1.78x the raw file in both flash and transfer — the
// 16KB Workshop-DIY asset would add ~29KB of CFG. As a path it costs ~40
// bytes, and the artwork can be swapped without re-encoding or reflashing.
// An app copy that lacks assets/workshop-diy-logo.svg simply shows the
// widget's label instead.
//
// Cost: beginner 2352 base64 bytes, expert 6384. At the old fixed CHUNK=18
// the expert layout would be 355 chunks and ~18.1s per connect; sendCfg() now
// sizes chunks to the negotiated MTU, so a desktop browser (MTU ~247) gets
// 180-byte chunks — 14 of them (~1.0s) for beginner, 36 (~2.1s) for expert.
// A peer that only grants the 23-byte minimum still falls back to 18 and the
// old timing. That headroom is also what makes switching levels cheap enough
// to re-send the whole CFG live instead of asking for a reconnect.
//
// Widget -> firmware mapping:
//   level                   -> picks which layout sendCfg() serves; persisted
//                              in NVS, re-sends the CFG on change
//   joy_drive / dpad_drive  -> both write the same s_joy_x/s_joy_y (steering)
//   spd                     -> scales the drive mix (remotexy_get_speed_cap)
//   btn_stop                -> clears inputs and calls stopServos()
//   btn_horn                -> horn/alarm (s_button_01)
//   btn_buzz                -> one buzzer_beep() per press
//   toggle_np / np_effect / np_bright
//                           -> strip state, dispatched from loop()
//   np_r / np_g / np_b      -> one channel each, recombined into the packed
//                              0xRRGGBB that remotexy_get_np_color() returns
//   toggle_led_r / toggle_led_g
//                           -> take the board LEDs over from leds_update()
//   led_button              -> board push button, mirrored as an indicator
//   gauge_rssi              -> BLE link strength via ble_gap_conn_rssi()
//   lbl_vbat                -> raw pack voltage alongside battery_level's %
//   upd                     -> telemetry verbosity gate (Off/Basic/All)
//
// Telemetry is sent ON CHANGE, with a per-value deadband, not once per cycle:
// the expert layout would otherwise push 11 notifications every pass for
// values that mostly sit still, and this radio has history with rc=6 under
// notify() pressure. graph_dist is the deliberate exception — a time series
// needs every sample or the trace shape lies. A full refresh is forced after
// every CFG transfer so a freshly rendered panel is never left blank.
//
//   gauge_speed             -> motor speed magnitude 0-100 (output only)
//   gauge_distance          -> ultrasonic distance 0-200cm (output only)
//   graph_dist              -> same measurement as a history (All only)
//   alert                   -> obstacle toast, edge-triggered with hysteresis
//   battery_level           -> battery percentage 0-100 (output only)
//   lbl_ver / lbl_uptime    -> firmware version and uptime (All only)
//   sound_alert             -> plays a tone on the phone (output only) —
//                              restores the old RemoteXY sound_01 element,
//                              which had no bit-rxy equivalent until the
//                              Sound widget was added
static const char* LAYOUT_CFG_BEGINNER_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6IldESVkgUm9ib3QgYjMiLCJjYW52YXMiOnsi"
  "dyI6ODE4LCJoIjo5MzB9LCJ3aWRnZXRzIjpbeyJpZCI6ImdycF9kcml2ZSIsInQiOiJncm91"
  "cCIsImxhYmVsIjoiRFJJVkUiLCJ4Ijo1NiwieSI6NjAsInciOjUwOCwiaCI6Mzc0LCJjb2xv"
  "ciI6IiMwMGQ0ZmYiLCJjaGlsZHJlbiI6WyJkcGFkX2RyaXZlIiwiam95X2RyaXZlIiwiYnRu"
  "X2hvcm4iXX0seyJpZCI6ImdycF9zZW5zZSIsInQiOiJncm91cCIsImxhYmVsIjoiU0VOU0lO"
  "RyIsIngiOjU2LCJ5Ijo1MDAsInciOjQ1OCwiaCI6MjI0LCJjb2xvciI6IiNmZmIwMjAiLCJj"
  "aGlsZHJlbiI6WyJnYXVnZV9zcGVlZCIsImdhdWdlX2Rpc3RhbmNlIiwic291bmRfYWxlcnQi"
  "XX0seyJpZCI6ImdycF9zeXMiLCJ0IjoiZ3JvdXAiLCJsYWJlbCI6IlNZU1RFTSIsIngiOjU1"
  "NCwieSI6NTMwLCJ3IjoyMDgsImgiOjM0NCwiY29sb3IiOiIjM2RkYzk3IiwiY2hpbGRyZW4i"
  "OlsiYmF0dGVyeV9sZXZlbCIsImxldmVsIiwibG9nbyJdfSx7ImlkIjoic2VwX2JhbmQiLCJ0"
  "Ijoic2VwYXJhdG9yIiwieCI6ODAsInkiOjQ2OCwidyI6NTAwLCJoIjo4fSx7ImlkIjoiZHBh"
  "ZF9kcml2ZSIsInQiOiJkcGFkIiwieCI6ODAsInkiOjEwMCwidyI6MjYwLCJoIjoyNjAsImxh"
  "YmVsIjoiRHJpdmUiLCJtb2RlbCI6ImNsYXNzaWMiLCJncm91cElkIjoiZ3JwX2RyaXZlIn0s"
  "eyJpZCI6ImpveV9kcml2ZSIsInQiOiJqb3lzdGljayIsIngiOjM4MCwieSI6MTAwLCJ3Ijox"
  "NjAsImgiOjE2MCwibGFiZWwiOiJEcml2ZSIsIm1vZGVsIjoiY2xhc3NpYyIsImdyb3VwSWQi"
  "OiJncnBfZHJpdmUifSx7ImlkIjoiYnRuX2hvcm4iLCJ0IjoiYnV0dG9uIiwieCI6MzgwLCJ5"
  "IjoyOTAsInciOjEyMCwiaCI6MTIwLCJsYWJlbCI6Ikhvcm4iLCJtb2RlbCI6Im5lbyIsImdy"
  "b3VwSWQiOiJncnBfZHJpdmUifSx7ImlkIjoiZ2F1Z2Vfc3BlZWQiLCJ0IjoiZ2F1Z2UiLCJ4"
  "Ijo4MCwieSI6NTQwLCJ3IjoxNDAsImgiOjE2MCwibGFiZWwiOiJTcGVlZCIsIm1pbiI6MCwi"
  "bWF4IjoxMDAsInVuaXRzIjoiJSIsImRlY2ltYWxzIjowLCJtb2RlbCI6ImNsYXNzaWMiLCJn"
  "cm91cElkIjoiZ3JwX3NlbnNlIn0seyJpZCI6ImdhdWdlX2Rpc3RhbmNlIiwidCI6ImdhdWdl"
  "IiwieCI6MjQwLCJ5Ijo1NDAsInciOjE0MCwiaCI6MTYwLCJsYWJlbCI6IkRpc3RhbmNlIiwi"
  "bWluIjowLCJtYXgiOjIwMCwidW5pdHMiOiJjbSIsImRlY2ltYWxzIjowLCJtb2RlbCI6ImNs"
  "YXNzaWMiLCJncm91cElkIjoiZ3JwX3NlbnNlIn0seyJpZCI6InNvdW5kX2FsZXJ0IiwidCI6"
  "InNvdW5kIiwieCI6NDAwLCJ5Ijo1NzUsInciOjkwLCJoIjo5MCwibGFiZWwiOiJBbGVydCIs"
  "Imdyb3VwSWQiOiJncnBfc2Vuc2UifSx7ImlkIjoiYmF0dGVyeV9sZXZlbCIsInQiOiJiYXR0"
  "ZXJ5IiwieCI6NTc4LCJ5Ijo1NzAsInciOjgwLCJoIjoxMDAsImxhYmVsIjoiQmF0dGVyeSIs"
  "Im1vZGVsIjoidmVydGljYWwiLCJncm91cElkIjoiZ3JwX3N5cyJ9LHsiaWQiOiJsZXZlbCIs"
  "InQiOiJzZWxlY3QiLCJ4Ijo1NzgsInkiOjcwMCwidyI6MTYwLCJoIjo3MCwibGFiZWwiOiJM"
  "ZXZlbCIsIm9wdGlvbnMiOiJCZWdpbm5lcixFeHBlcnQiLCJncm91cElkIjoiZ3JwX3N5cyJ9"
  "LHsiaWQiOiJsb2dvIiwidCI6ImltYWdlIiwieCI6NTc4LCJ5Ijo3OTAsInciOjE2MCwiaCI6"
  "NjAsImxhYmVsIjoiV29ya3Nob3AtRElZIiwiaW1hZ2VTcmMiOiJhc3NldHMvd29ya3Nob3At"
  "ZGl5LWxvZ28uc3ZnIiwiZ3JvdXBJZCI6ImdycF9zeXMifV19";

static const char* LAYOUT_CFG_EXPERT_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoyLCJ0aXRsZSI6IldESVkgUm9ib3QgYjMiLCJ3aWRnZXRzIjpb"
  "eyJpZCI6ImdycF9kcml2ZSIsInQiOiJncm91cCIsImxhYmVsIjoiRFJJVkUiLCJ4Ijo1Mywi"
  "eSI6MTUsInciOjY5MSwiaCI6NTUxLCJjb2xvciI6IiMwMGQ0ZmYiLCJjaGlsZHJlbiI6WyJk"
  "cGFkX2RyaXZlIiwiam95X2RyaXZlIiwic3BkIiwiYnRuX3N0b3AiLCJnYXVnZV9zcGVlZCJd"
  "fSx7ImlkIjoiZ3JwX2Rpc3QiLCJ0IjoiZ3JvdXAiLCJsYWJlbCI6IkRJU1RBTkNFIiwieCI6"
  "Nzc1LCJ5IjoxMywidyI6NDI2LCJoIjo1NTEsImNvbG9yIjoiI2ZmYjAyMCIsImNoaWxkcmVu"
  "IjpbImdhdWdlX2Rpc3RhbmNlIiwiYWxlcnQiLCJncmFwaF9kaXN0Il19LHsiaWQiOiJncnBf"
  "bGlnaHQiLCJ0IjoiZ3JvdXAiLCJsYWJlbCI6IkxJR0hUUyIsIngiOjUwLCJ5Ijo1ODIsInci"
  "OjY4OSwiaCI6Mzc3LCJjb2xvciI6IiM3YzVjZmYiLCJjaGlsZHJlbiI6WyJ0b2dnbGVfbGVk"
  "X3IiLCJ0b2dnbGVfbGVkX2ciLCJ0b2dnbGVfbnAiLCJucF9lZmZlY3QiLCJucF9yIiwibnBf"
  "ZyIsIm5wX2IiLCJucF9icmlnaHQiXX0seyJpZCI6ImdycF9zb3VuZCIsInQiOiJncm91cCIs"
  "ImxhYmVsIjoiU09VTkQiLCJ4Ijo3NjUsInkiOjU4NiwidyI6NDMzLCJoIjoxODAsImNvbG9y"
  "IjoiI2ZmNWM4YSIsImNoaWxkcmVuIjpbImJ0bl9ob3JuIiwiYnRuX2J1enoiLCJzb3VuZF9h"
  "bGVydCJdfSx7ImlkIjoiZ3JwX3N5cyIsInQiOiJncm91cCIsImxhYmVsIjoiU1lTVEVNIiwi"
  "eCI6NDYsInkiOjk5OSwidyI6MTA3MiwiaCI6MjUzLCJjb2xvciI6IiMzZGRjOTciLCJjaGls"
  "ZHJlbiI6WyJiYXR0ZXJ5X2xldmVsIiwibGJsX3ZlciIsImxibF91cHRpbWUiLCJsYmxfdmJh"
  "dCIsInVwZCIsImxldmVsIiwibGVkX2J1dHRvbiIsImdhdWdlX3Jzc2kiLCJsb2dvIl19LHsi"
  "aWQiOiJncnBfZGlzcGxheSIsInQiOiJncm91cCIsImxhYmVsIjoiRElTUExBWSIsIngiOjc2"
  "LCJ5IjoxMjgyLCJ3Ijo2MTgsImgiOjE0NCwiY29sb3IiOiIjMDBlNWZmIiwiY2hpbGRyZW4i"
  "Olsib2xlZF90ZXh0IiwibGJsX29sZWQiXX0seyJpZCI6InNlcF9jb2xzIiwidCI6InNlcGFy"
  "YXRvciIsIngiOjc0OCwieSI6NDksInciOjgsImgiOjQ5M30seyJpZCI6InNlcF9iMSIsInQi"
  "OiJzZXBhcmF0b3IiLCJ4Ijo3NzMsInkiOjU3NCwidyI6NDEyLCJoIjoxMn0seyJpZCI6InNl"
  "cF9iMiIsInQiOiJzZXBhcmF0b3IiLCJ4Ijo1NSwieSI6OTcyLCJ3IjoxMDQ0LCJoIjo4fSx7"
  "ImlkIjoiZHBhZF9kcml2ZSIsInQiOiJkcGFkIiwieCI6NzcsInkiOjczLCJ3IjozNzEsImgi"
  "OjQ3NCwibGFiZWwiOiJEcml2ZSIsIm1vZGVsIjoiY2xhc3NpYyIsImdyb3VwSWQiOiJncnBf"
  "ZHJpdmUifSx7ImlkIjoiam95X2RyaXZlIiwidCI6ImpveXN0aWNrIiwieCI6NDUzLCJ5Ijo3"
  "NiwidyI6MTc0LCJoIjoxNzUsImxhYmVsIjoiRHJpdmUiLCJncm91cElkIjoiZ3JwX2RyaXZl"
  "In0seyJpZCI6InNwZCIsInQiOiJzbGlkZXIiLCJ4Ijo2MzYsInkiOjY1LCJ3Ijo5MCwiaCI6"
  "MjAwLCJsYWJlbCI6IlNwZWVkIiwibWF4IjoxMDAsInZhbHVlIjoxMDAsImdyb3VwSWQiOiJn"
  "cnBfZHJpdmUifSx7ImlkIjoiYnRuX3N0b3AiLCJ0IjoiYnV0dG9uIiwieCI6NDU1LCJ5Ijoz"
  "NTAsInciOjEyMCwiaCI6MTIwLCJsYWJlbCI6IlNUT1AiLCJtb2RlbCI6ImZsYXQiLCJncm91"
  "cElkIjoiZ3JwX2RyaXZlIn0seyJpZCI6ImdhdWdlX3NwZWVkIiwidCI6ImdhdWdlIiwieCI6"
  "NTc3LCJ5IjozMjUsInciOjE0OSwiaCI6MTcxLCJsYWJlbCI6IlNwZWVkIiwibWluIjowLCJt"
  "YXgiOjEwMCwidW5pdHMiOiIlIiwiZGVjaW1hbHMiOjAsIm1vZGVsIjoiY2xhc3NpYyIsImdy"
  "b3VwSWQiOiJncnBfZHJpdmUifSx7ImlkIjoiZ2F1Z2VfZGlzdGFuY2UiLCJ0IjoiZ2F1Z2Ui"
  "LCJ4Ijo4MDYsInkiOjYzLCJ3IjoxNDAsImgiOjE2MywibGFiZWwiOiJEaXN0YW5jZSIsIm1p"
  "biI6MCwibWF4IjoyMDAsInVuaXRzIjoiY20iLCJkZWNpbWFscyI6MCwibW9kZWwiOiJjbGFz"
  "c2ljIiwiZ3JvdXBJZCI6ImdycF9kaXN0In0seyJpZCI6ImFsZXJ0IiwidCI6Im5vdGlmaWNh"
  "dGlvbiIsIngiOjEwMjcsInkiOjEwMCwidyI6OTAsImgiOjkwLCJsYWJlbCI6Ik9ic3RhY2xl"
  "IiwiZ3JvdXBJZCI6ImdycF9kaXN0In0seyJpZCI6ImdyYXBoX2Rpc3QiLCJ0IjoiZ3JhcGgi"
  "LCJ4Ijo4MDEsInkiOjIyNiwidyI6Mzg3LCJoIjozMTMsImxhYmVsIjoiRGlzdGFuY2UgY20i"
  "LCJtYXgiOjIwMCwiZ3JvdXBJZCI6ImdycF9kaXN0In0seyJpZCI6InRvZ2dsZV9sZWRfciIs"
  "InQiOiJ0b2dnbGUiLCJ4Ijo3NCwieSI6NjMyLCJ3IjoxMDAsImgiOjEwMCwibGFiZWwiOiJS"
  "ZWQgTEVEIiwibW9kZWwiOiJwaWxsIiwiZ3JvdXBJZCI6ImdycF9saWdodCJ9LHsiaWQiOiJ0"
  "b2dnbGVfbGVkX2ciLCJ0IjoidG9nZ2xlIiwieCI6MTk0LCJ5Ijo2MzIsInciOjEwMCwiaCI6"
  "MTAwLCJsYWJlbCI6IkdyZWVuIExFRCIsIm1vZGVsIjoicGlsbCIsImdyb3VwSWQiOiJncnBf"
  "bGlnaHQifSx7ImlkIjoidG9nZ2xlX25wIiwidCI6InRvZ2dsZSIsIngiOjMxNCwieSI6NjMy"
  "LCJ3IjoxMDAsImgiOjEwMCwibGFiZWwiOiJTdHJpcCIsIm1vZGVsIjoicGlsbCIsImdyb3Vw"
  "SWQiOiJncnBfbGlnaHQifSx7ImlkIjoibnBfZWZmZWN0IiwidCI6InNlbGVjdCIsIngiOjQz"
  "NCwieSI6NjQ3LCJ3IjoxNjAsImgiOjcwLCJsYWJlbCI6IlN0cmlwIGVmZmVjdCIsIm9wdGlv"
  "bnMiOiJTb2xpZCxSYWluYm93LEtuaWdodCBSaWRlcixEdWVsIGV5ZSxGcmVuY2ggZmxhZyIs"
  "Imdyb3VwSWQiOiJncnBfbGlnaHQifSx7ImlkIjoibnBfciIsInQiOiJzbGlkZXIiLCJ4Ijo5"
  "MywieSI6NzU2LCJ3Ijo3MCwiaCI6MTgwLCJsYWJlbCI6IlIiLCJtYXgiOjI1NSwidmFsdWUi"
  "OjI1NSwiZ3JvdXBJZCI6ImdycF9saWdodCJ9LHsiaWQiOiJucF9nIiwidCI6InNsaWRlciIs"
  "IngiOjE4OSwieSI6NzU5LCJ3Ijo3MCwiaCI6MTgwLCJsYWJlbCI6IkciLCJtYXgiOjI1NSwi"
  "dmFsdWUiOjAsImdyb3VwSWQiOiJncnBfbGlnaHQifSx7ImlkIjoibnBfYiIsInQiOiJzbGlk"
  "ZXIiLCJ4IjoyNzQsInkiOjc2MSwidyI6NzAsImgiOjE4MCwibGFiZWwiOiJCIiwibWF4Ijoy"
  "NTUsInZhbHVlIjowLCJncm91cElkIjoiZ3JwX2xpZ2h0In0seyJpZCI6Im5wX2JyaWdodCIs"
  "InQiOiJzbGlkZXIiLCJ4Ijo0NjcsInkiOjc1MywidyI6OTAsImgiOjE4MCwibGFiZWwiOiJC"
  "cmlnaHRuZXNzIiwibWF4IjoyNTUsInN0ZXAiOjUsInZhbHVlIjoxNSwiZ3JvdXBJZCI6Imdy"
  "cF9saWdodCJ9LHsiaWQiOiJidG5faG9ybiIsInQiOiJidXR0b24iLCJ4Ijo3ODMsInkiOjYz"
  "OSwidyI6MTAwLCJoIjoxMDAsImxhYmVsIjoiSG9ybiIsImdyb3VwSWQiOiJncnBfc291bmQi"
  "fSx7ImlkIjoiYnRuX2J1enoiLCJ0IjoiYnV0dG9uIiwieCI6OTI2LCJ5Ijo2MzYsInciOjEw"
  "MCwiaCI6MTAwLCJsYWJlbCI6IkJ1enoiLCJncm91cElkIjoiZ3JwX3NvdW5kIn0seyJpZCI6"
  "InNvdW5kX2FsZXJ0IiwidCI6InNvdW5kIiwieCI6MTA4MywieSI6NjQwLCJ3Ijo5MCwiaCI6"
  "OTAsImxhYmVsIjoiQWxlcnQiLCJncm91cElkIjoiZ3JwX3NvdW5kIn0seyJpZCI6ImJhdHRl"
  "cnlfbGV2ZWwiLCJ0IjoiYmF0dGVyeSIsIngiOjYwMiwieSI6MTEwNywidyI6ODAsImgiOjEw"
  "MCwibGFiZWwiOiJCYXR0ZXJ5IiwibW9kZWwiOiJ2ZXJ0aWNhbCIsImdyb3VwSWQiOiJncnBf"
  "c3lzIn0seyJpZCI6ImxibF92ZXIiLCJ0IjoibGFiZWwiLCJ4Ijo2MSwieSI6MTA0OSwidyI6"
  "MjAwLCJoIjo1MCwibGFiZWwiOiJGaXJtd2FyZSIsIm1vZGVsIjoiY2FyZCIsImdyb3VwSWQi"
  "OiJncnBfc3lzIn0seyJpZCI6ImxibF91cHRpbWUiLCJ0IjoibGFiZWwiLCJ4IjoyNzEsInki"
  "OjEwNTEsInciOjIwMCwiaCI6NTAsImxhYmVsIjoiVXB0aW1lIiwibW9kZWwiOiJjYXJkIiwi"
  "Z3JvdXBJZCI6ImdycF9zeXMifSx7ImlkIjoibGJsX3ZiYXQiLCJ0IjoibGFiZWwiLCJ4Ijo0"
  "ODgsInkiOjEwNTEsInciOjIwMCwiaCI6NTAsImxhYmVsIjoiQmF0dGVyeSBWIiwibW9kZWwi"
  "OiJjYXJkIiwiZ3JvdXBJZCI6ImdycF9zeXMifSx7ImlkIjoidXBkIiwidCI6InNlbGVjdCIs"
  "IngiOjg0LCJ5IjoxMTI3LCJ3IjoxNjAsImgiOjcwLCJsYWJlbCI6IlRlbGVtZXRyeSIsIm9w"
  "dGlvbnMiOiJPZmYsQmFzaWMsQWxsIiwiZ3JvdXBJZCI6ImdycF9zeXMifSx7ImlkIjoibGV2"
  "ZWwiLCJ0Ijoic2VsZWN0IiwieCI6Mjg0LCJ5IjoxMTI5LCJ3IjoxNjAsImgiOjcwLCJsYWJl"
  "bCI6IkxldmVsIiwib3B0aW9ucyI6IkJlZ2lubmVyLEV4cGVydCIsImdyb3VwSWQiOiJncnBf"
  "c3lzIn0seyJpZCI6ImxlZF9idXR0b24iLCJ0IjoibGVkIiwieCI6NDg1LCJ5IjoxMTE5LCJ3"
  "Ijo4MCwiaCI6ODAsImxhYmVsIjoiQnV0dG9uIiwibW9kZWwiOiJkb3QiLCJjb2xvck9uIjoi"
  "IzAwZmY4OCIsImdyb3VwSWQiOiJncnBfc3lzIn0seyJpZCI6ImdhdWdlX3Jzc2kiLCJ0Ijoi"
  "Z2F1Z2UiLCJ4Ijo3MDQsInkiOjEwNTgsInciOjE4OSwiaCI6MTkwLCJsYWJlbCI6IlNpZ25h"
  "bCIsIm1pbiI6LTEwMCwibWF4IjotMzAsInVuaXRzIjoiZEJtIiwiZGVjaW1hbHMiOjAsIm1v"
  "ZGVsIjoiY2xhc3NpYyIsImdyb3VwSWQiOiJncnBfc3lzIn0seyJpZCI6ImxvZ28iLCJ0Ijoi"
  "aW1hZ2UiLCJ4Ijo5MTAsInkiOjEwNTIsInciOjE5MiwiaCI6MTY0LCJsYWJlbCI6Ildvcmtz"
  "aG9wLURJWSIsImltYWdlU3JjIjoiYXNzZXRzL3dvcmtzaG9wLWRpeS1sb2dvLnN2ZyIsImdy"
  "b3VwSWQiOiJncnBfc3lzIn0seyJpZCI6Im9sZWRfdGV4dCIsInQiOiJlZGl0ZmllbGQiLCJ4"
  "IjoxMDAsInkiOjEzMzIsInciOjI2MCwiaCI6NzAsImxhYmVsIjoiT0xFRCB0ZXh0IiwicGxh"
  "Y2Vob2xkZXIiOiJUeXBlIGZvciB0aGUgcm9ib3Qgc2NyZWVuLi4uIiwiZ3JvdXBJZCI6Imdy"
  "cF9kaXNwbGF5In0seyJpZCI6ImxibF9vbGVkIiwidCI6ImxhYmVsIiwieCI6MzkwLCJ5Ijox"
  "MzM3LCJ3IjoyODAsImgiOjYwLCJsYWJlbCI6Ik9MRUQgc2hvd3MiLCJtb2RlbCI6ImNhcmQi"
  "LCJncm91cElkIjoiZ3JwX2Rpc3BsYXkifV0sImNhbnZhcyI6eyJ3IjoxMjQ3LCJoIjoxNDcy"
  "fX0=";

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

// Negotiated ATT MTU, updated by onMTUChange(). sendCfg() sizes its chunks
// from this: the old fixed 18 came from the rxy MakeCode template, not from
// any limit of this radio, and it made a large layout take tens of seconds.
static volatile uint16_t     s_peerMtu = 23;

// Connection handle of the live link, kept so link quality can be sampled.
// BLE_HS_CONN_HANDLE_NONE means "not connected" and is what ble_gap_conn_rssi()
// is guarded against — querying a stale handle returns an error rather than a
// reading, and would otherwise publish a nonsense value every cycle.
static volatile uint16_t     s_connHandle = BLE_HS_CONN_HANDLE_NONE;

static int8_t s_joy_x = 0;   // -100..100, drives tasks_joysticks() (turn)
static int8_t s_joy_y = 0;   // -100..100, drives tasks_joysticks() (forward/back)
static uint8_t s_button_01 = 0;

// dpad_drive writes the same s_joy_x/s_joy_y as joy_drive — both widgets
// drive the identical robot state, whichever was touched most recently wins.
static bool s_dpad_up = false, s_dpad_down = false, s_dpad_left = false, s_dpad_right = false;

// Which app is on the other end, for the ambiguous joystick encoding below.
// Only the mecanum app (keystudio_4wd_mecanum_rxy, v2.13+) opens with
// GETCFGVER; stock bit-rxy has no such command, so seeing it identifies the
// peer. Latched, never cleared on reconnect, because both flags only ever
// widen what we accept — a false positive costs nothing that the value-range
// checks in handleJoystick() do not already override.
static bool s_peerSawCfgVer = false;
// Set once a joystick sample proves the x/y encoding (a negative field).
static bool s_joyIsXY       = false;

// Reported to the app in the SYSTEM zone's Firmware label. Bump on release.
#define B3_FIRMWARE_VERSION "B3-v1"

// ---------------------------------------------------------------------------
// Widget state owned by the app (see 03_bit-rxy.h for the accessors).
// ---------------------------------------------------------------------------
static uint8_t  s_speed_cap   = 100;              // DRIVE  — Speed slider
static uint8_t  s_np_on       = 1;                // LIGHTS — Strip toggle
static uint8_t  s_np_effect   = NP_EFFECT_FRENCH; // LIGHTS — matches the old
                                                  // hardcoded loop() behaviour
static uint8_t  s_np_bright   = CONFIG_NEOPIXELS_BRIGHTNESS;
static uint32_t s_np_color    = 0xFF0000;         // LIGHTS — Strip colour

// LIGHTS — the two board LEDs. These are normally the link-status indicator
// (leds_update() blinks green when connected, red when not), so the app does
// not get them for free: the first toggle takes ownership, and ownership is
// dropped again on disconnect so the "not connected" signal always comes back.
static bool s_led_manual = false;
static bool s_led_r_on   = false;
static bool s_led_g_on   = false;
static uint8_t  s_upd_level   = UPD_ALL;          // SYSTEM — Telemetry select

// Which layout the robot serves. Both are compiled in; the Level select in
// the SYSTEM zone switches between them, and the choice is kept in NVS so a
// power cycle does not drop an expert user back to the simple panel. A fresh
// device starts on Beginner.
//
// The `level` widget exists in BOTH layouts on purpose — if it were only in
// the expert one, switching to beginner would strand the robot there until a
// reflash.
static Preferences s_prefs;
static uint8_t     s_layout_level = LAYOUT_BEGINNER;

static void layoutLevelLoad(void) {
  // Read-only open: nothing else in this firmware uses NVS, so a missing
  // namespace on a fresh chip is expected rather than an error.
  if (s_prefs.begin("b3", true)) {
    s_layout_level = s_prefs.getUChar("level", LAYOUT_BEGINNER);
    s_prefs.end();
  }
  if (s_layout_level > LAYOUT_EXPERT) s_layout_level = LAYOUT_BEGINNER;
}

static void layoutLevelStore(uint8_t lvl) {
  if (lvl == s_layout_level) return;   // don't burn a flash write on a no-op
  s_layout_level = lvl;
  if (s_prefs.begin("b3", false)) {
    s_prefs.putUChar("level", lvl);
    s_prefs.end();
  }
}

uint8_t remotexy_get_layout_level(void) { return s_layout_level; }

// ---------------------------------------------------------------------------
// Telemetry is sent on change, not every cycle. With the expert layout the
// naive version pushed 11 notifications per cycle for values that mostly do
// not move — and this radio has a documented history of drowning in rc=6
// (BLE_HS_ENOMEM) when notify() is called too freely.
//
// Each value carries a deadband so sensor jitter alone cannot make it "change"
// forever: the ultrasonic reading wobbles by a few mm at rest and RSSI drifts
// a dB or two constantly.
//
// s_telemForce makes the next pass send everything regardless. It is set after
// every CFG transfer, because a freshly rendered panel has no values in it —
// without it, a widget whose value had not changed since before the reconnect
// would sit blank until it happened to move.
// ---------------------------------------------------------------------------
static bool     s_telemForce      = true;
static float    s_lastDist        = NAN;
static float    s_lastSpeed       = NAN;
static float    s_lastBatt        = NAN;
static float    s_lastVbat        = NAN;
static int16_t  s_lastRssi        = 32767;
static int8_t   s_lastBtn         = -1;
static uint32_t s_lastUptimeSec   = 0xFFFFFFFFu;
static bool     s_verSent         = false;
static String   s_lastOled        = "";   // impossible first value -> always sends once

// The beginner layout carries only dpad/joystick/horn, the two gauges, the
// sound widget, battery, level and the logo. Everything else in the expert
// layout simply does not exist there, so publishing it would be notifications
// sent to widget ids no panel contains — pure waste on the same radio that
// this file already works hard to keep out of rc=6.
static inline bool telemExpertOnly(void) {
  return s_layout_level == LAYOUT_EXPERT;
}

// Returns true (and latches the new value) when `now` has moved far enough
// from the last transmitted value to be worth a notification.
static bool telemChanged(float now, float& last, float deadband) {
  if (s_telemForce || isnan(last) || fabsf(now - last) >= deadband) {
    last = now;
    return true;
  }
  return false;
}

// Obstacle notification is edge-triggered: the app shows a toast per message,
// so re-sending every cycle while an obstacle sits in front would spam it.
static bool     s_obstacleLatched = false;
#define B3_OBSTACLE_CM      30.0f
#define B3_OBSTACLE_CLEAR_CM 40.0f   // hysteresis, so a jittering reading
                                     // near the threshold cannot chatter

uint8_t  remotexy_get_speed_cap(void)      { return s_speed_cap; }
uint8_t  remotexy_get_np_on(void)          { return s_np_on; }
uint8_t  remotexy_get_np_effect(void)      { return s_np_effect; }
uint8_t  remotexy_get_np_brightness(void)  { return s_np_bright; }
uint32_t remotexy_get_np_color(void)       { return s_np_color; }
uint8_t  remotexy_get_upd_level(void)      { return s_upd_level; }

static void handleLine(const String& line);
static void handleWidget(const String& id, const String& val);
static void handleDpadMask(uint8_t mask);
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
    s_peerMtu    = 23;  // assume the BLE minimum until the peer negotiates up
    s_connHandle = info.getConnHandle();
  }
  // Records the negotiated MTU so sendCfg() can size its chunks to it. The
  // central drives this: browsers on desktop typically settle well above the
  // 23-byte minimum, but nothing guarantees it, so the value is only ever
  // used as an upper bound with a conservative floor.
  void onMTUChange(uint16_t MTU, NimBLEConnInfo& /*info*/) override {
    s_peerMtu = MTU;
    #ifdef DEF_DERIAL_DEBUG
    Serial.printf("[BLE] MTU negotiated: %u\n", (unsigned)MTU);
    #endif
  }
  void onDisconnect(NimBLEServer* /*server*/, NimBLEConnInfo& /*info*/, int reason) override {
    s_connected = false;
    s_joy_x = 0;
    s_joy_y = 0;
    s_button_01 = 0;
    s_dpad_up = s_dpad_down = s_dpad_left = s_dpad_right = false;
    s_connHandle = BLE_HS_CONN_HANDLE_NONE;
    // Hand the two board LEDs back to the status indicator, so a disconnect
    // is always visible even if the user had taken them over from the app.
    s_led_manual = false;
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
  const char* p  = (s_layout_level == LAYOUT_EXPERT) ? LAYOUT_CFG_EXPERT_BASE64
                                                     : LAYOUT_CFG_BEGINNER_BASE64;
  const size_t n = strlen(p);

  // Chunk size follows the negotiated MTU rather than the rxy MakeCode
  // template's fixed 18. The app never cares how big a chunk is — it just
  // appends line.substring(4) — so the only real limit is what fits in one
  // notification: MTU minus 3 bytes of ATT header, minus "CFG " and the
  // trailing newline. Capped at 180 to stay clear of controller buffer
  // pressure, floored at 18 so a peer that only grants the 23-byte minimum
  // still behaves exactly as before. On a desktop browser (MTU ~247) this
  // turns a 260-chunk, 13-second burst into ~26 chunks and ~1.6 seconds.
  size_t chunk = 18;
  if (s_peerMtu > 23 + 5) {
    chunk = (size_t)s_peerMtu - 3 - 5;   // ATT header + "CFG " + '\n'
    if (chunk > 180) chunk = 180;
    if (chunk < 18)  chunk = 18;
  }
  const size_t CHUNK = chunk;
  const unsigned total = (unsigned)((n + CHUNK - 1) / CHUNK);
  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[BLE] CFG %u bytes, MTU %u -> chunk %u (%u chunks)\n",
                (unsigned)n, (unsigned)s_peerMtu, (unsigned)CHUNK, total);
  #endif

  // Announce how many chunks are coming. Both apps match this line with
  // startsWith("CFGBEGIN"), so the argument is ignored by anything that does
  // not want it — but a client that reads it can show a truthful progress bar
  // instead of guessing. The existing guess (12 + 4*chunk, capped at 90) was
  // tuned for a ~20-chunk transfer and now pins at 90% for most of the burst,
  // because MTU-sized chunking made the count vary from 14 to 40-plus.
  sendLine("CFGBEGIN " + String(total));

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
  // The app has just rendered a fresh panel with no values in it, so the next
  // telemetry pass must publish everything rather than only what has moved.
  s_telemForce = true;
  s_sendingCfg = false;
  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[BLE] Sent CFG (dropped=%d)\n", dropped);
  #endif
}

static void handleLine(const String& line) {
  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[BLE] RX line: '%s'\n", line.c_str());
  #endif

  // Fastest D-pad wire format, and the only one the mecanum app sends: one
  // byte 'a'..'p' encodes the complete 4-bit direction mask (up=1, down=2,
  // left=4, right=8), followed by the newline this line was already split on.
  // Tested before everything else because while driving it is by far the most
  // frequent line — a held direction is re-sent every 300ms as a keepalive.
  if (line.length() == 1) {
    const char c = line.charAt(0);
    if (c >= 'a' && c <= 'p') { handleDpadMask((uint8_t)(c - 'a')); return; }
  }

  // Text form of the same mask. The micro:bit firmware honours it and the
  // mecanum app still filters "M " out of its own send queue, so a future app
  // version may well emit it again; sharing the handler costs one branch.
  if (line.startsWith("M ")) {
    handleDpadMask((uint8_t)line.substring(2).toInt());
    return;
  }

  // Deliberately not answered. Replying properly means reproducing the app's
  // config-revision hash exactly, and a mismatch is silent — no error, just a
  // permanent cache miss. The app retries 3x900ms then falls back to GETCFG on
  // its own, which is the behaviour this firmware already had. Recording it is
  // free, and it tells handleJoystick() which encoding to expect: only the
  // mecanum app sends GETCFGVER, stock bit-rxy has no such command.
  if (line == "GETCFGVER") { s_peerSawCfgVer = true; return; }

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

// Two apps put two different encodings on the same "SET joy_drive ..." line:
//
//   stock bit-rxy   "<angle> <distance>"  angle 0deg=right, 90deg=up,
//                                         180deg=left, 270deg=down — standard
//                                         math convention, confirmed
//                                         empirically against the live app;
//                                         the "90=down" convention documented
//                                         in some rxy reference comments does
//                                         not match actual runtime behavior.
//                                         distance 0..100.
//   mecanum app     "<x> <y>"             both -100..100, +x = right,
//                                         +y = up.
//
// Both are two numbers, so a sample inside the 0..100 quadrant is genuinely
// ambiguous. Resolved in this order, strongest evidence first:
//
//   1. either field negative   -> x/y    (a distance is never negative)
//   2. peer sent GETCFGVER     -> x/y    (only the mecanum app sends it)
//   3. first field above 100   -> angle  (a valid angle, an impossible x)
//   4. otherwise               -> angle  (bit-rxy's long-standing reading, so
//                                         its behaviour is bit-for-bit
//                                         unchanged by this function)
//
// Rule 1 latches for the session: once x/y is proven, later samples that fall
// back inside the ambiguous quadrant keep being read as x/y.
//
// Either way the output is the -100..100 pair tasks_joysticks() expects
// (x = turn right, y = forward).
static void handleJoystick(const String& val) {
  int sp = val.indexOf(' ');
  if (sp < 0) return;
  const float a = val.substring(0, sp).toFloat();   // angle, or x
  const float b = val.substring(sp + 1).toFloat();  // distance, or y

  if (a < 0.0f || b < 0.0f) s_joyIsXY = true;
  const bool xy = s_joyIsXY || (s_peerSawCfgVer && a <= 100.0f);

  if (xy) {
    s_joy_x = (int8_t)constrain((long)round(a), -100, 100);
    // Negated for the same reason as the angle path below.
    s_joy_y = (int8_t)constrain((long)round(-b), -100, 100);
    return;
  }

  const float rad = a * PI / 180.0f;
  s_joy_x = (int8_t)constrain((long)round(b * cos(rad)), -100, 100);
  // Negated: the robot's forward/back sense turned out to be the mirror
  // of the joystick's up/down sense (confirmed empirically) — turning
  // (x) was already correct, only this axis needed flipping.
  s_joy_y = (int8_t)constrain((long)round(-b * sin(rad)), -100, 100);
}

// D-Pad sends "<up|down|left|right> <0|1>" immediately on press/release
// (unlike the joystick, which only streams while actively dragging), so it
// drives s_joy_x/s_joy_y at a fixed full-speed magnitude from the combined
// state of whichever direction buttons are currently held.
static const int8_t DPAD_SPEED = 100;

// Shared by both D-pad wire formats: whichever one set the four booleans,
// the drive state is derived from their combined value the same way.
static void dpadRecompute() {
  s_joy_x = (int8_t)((s_dpad_right ? DPAD_SPEED : 0) - (s_dpad_left ? DPAD_SPEED : 0));
  // Same forward/back mirror as the joystick (see handleJoystick) — down
  // is physically forward on this chassis.
  s_joy_y = (int8_t)((s_dpad_down  ? DPAD_SPEED : 0) - (s_dpad_up   ? DPAD_SPEED : 0));
}

// "<up|down|left|right> <0|1>" — one direction per message, as stock bit-rxy
// sends it. Unchanged.
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

  dpadRecompute();
}

// The mecanum app's mask carries the COMPLETE current button state rather than
// one transition, so a dropped packet is corrected by the next one and stale
// queued events never need replaying. Diagonals need no protocol support: a
// corner press just sets two cardinal bits at once, which dpadRecompute()
// already sums. Mask 0 is the centre STOP, and also every-button-released.
static void handleDpadMask(uint8_t mask) {
  s_dpad_up    = (mask & 0x1) != 0;
  s_dpad_down  = (mask & 0x2) != 0;
  s_dpad_left  = (mask & 0x4) != 0;
  s_dpad_right = (mask & 0x8) != 0;
  dpadRecompute();
}

// np_effect's options list is the one remaining string coupling: these names
// must match the select in the layout JSON.
static uint8_t effectFromName(const String& name) {
  if (name == "Solid")        return NP_EFFECT_SOLID;
  if (name == "Rainbow")      return NP_EFFECT_RAINBOW;
  if (name == "Knight Rider") return NP_EFFECT_KNIGHT;
  if (name == "Duel eye")     return NP_EFFECT_DUEL;
  return NP_EFFECT_FRENCH;
}

static void handleWidget(const String& id, const String& val) {
  // --- DRIVE ---------------------------------------------------------------
  if (id == "joy_drive")   { handleJoystick(val); return; }
  if (id == "dpad_drive")  { handleDpad(val); return; }
  if (id == "spd")         { s_speed_cap = (uint8_t)constrain(val.toInt(), 0, 100); return; }
  if (id == "btn_stop") {
    if (val != "1") return;   // act on press, ignore the release
    // Clear every input that could immediately re-assert motion, then stop
    // the servos outright rather than waiting for the next drive mix.
    s_joy_x = s_joy_y = 0;
    s_dpad_up = s_dpad_down = s_dpad_left = s_dpad_right = false;
    stopServos();
    return;
  }

  // --- SOUND ---------------------------------------------------------------
  if (id == "btn_horn")    { s_button_01 = (val == "1") ? 1 : 0; return; }
  if (id == "btn_buzz")    { if (val == "1") buzzer_beep(); return; }

  // --- LIGHTS --------------------------------------------------------------
  if (id == "toggle_led_r") { s_led_manual = true; s_led_r_on = (val == "1"); return; }
  if (id == "toggle_led_g") { s_led_manual = true; s_led_g_on = (val == "1"); return; }
  if (id == "toggle_np")   { s_np_on = (val == "1") ? 1 : 0; return; }
  // Colour arrives one channel at a time; recombine into the packed 0xRRGGBB
  // that neopixels_all_clear() wants.
  if (id == "np_r") { s_np_color = (s_np_color & 0x00FFFF) | ((uint32_t)constrain(val.toInt(),0,255) << 16); return; }
  if (id == "np_g") { s_np_color = (s_np_color & 0xFF00FF) | ((uint32_t)constrain(val.toInt(),0,255) <<  8); return; }
  if (id == "np_b") { s_np_color = (s_np_color & 0xFFFF00) | ((uint32_t)constrain(val.toInt(),0,255)      ); return; }
  if (id == "np_effect")   { s_np_effect = effectFromName(val); return; }
  if (id == "np_bright")   { s_np_bright = (uint8_t)constrain(val.toInt(), 0, 255); return; }

  // --- DISPLAY -------------------------------------------------------------
  // Text typed in the app appears on the robot's physical OLED. Clearing the
  // field restores the normal banner. s_telemForce is raised so the mirror
  // label re-publishes immediately rather than waiting for a change.
  if (id == "oled_text") {
    oled_text_set(val.c_str());
    s_telemForce = true;
    return;
  }

  // --- SYSTEM --------------------------------------------------------------
  if (id == "level") {
    const uint8_t want = (val == "Expert") ? LAYOUT_EXPERT : LAYOUT_BEGINNER;
    if (want == s_layout_level) return;
    layoutLevelStore(want);
    // Re-send the whole CFG so the app re-renders on the new layout with no
    // reconnect and no Reload Config. Deliberately via the deferred flag and
    // NOT by calling sendCfg() here: this runs inside NimBLE's host task, and
    // running the burst synchronously from it is the documented ENOMEM bug
    // that s_getCfgRequested exists to avoid.
    s_getCfgRequested = true;
    return;
  }
  if (id == "upd") {
    if (val == "Off")        s_upd_level = UPD_OFF;
    else if (val == "Basic") s_upd_level = UPD_BASIC;
    else                     s_upd_level = UPD_ALL;
    return;
  }
}

// ===========================================================================
// Public API (same signatures as the old RemoteXY adapter)
// ===========================================================================
void remotexy_init(void) {
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("[BLE] init - device_name: " CONFIG_BLE_DEVICE_NAME);
  #endif

  // Restore the Beginner/Expert choice before advertising, so the very first
  // GETCFG after a power cycle already answers with the right layout.
  layoutLevelLoad();
  #ifdef DEF_DERIAL_DEBUG
  Serial.printf("[BLE] layout level: %s\n",
                s_layout_level == LAYOUT_EXPERT ? "Expert" : "Beginner");
  #endif

  NimBLEDevice::init(CONFIG_BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  // No explicit setMTU() call — NimBLE-Arduino's own built-in default
  // preferred MTU (255) is what sendCfg() sizes its chunks against.
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

// The three original gauges are "Basic" telemetry: they are what the robot is
// doing right now, so they survive everything except Telemetry = Off. All three
// display whole numbers, so a deadband of 1 costs nothing visible.
void remotexy_set_onlineGraph_01_distance(float p_onlineGraph_01_distance) {
  if (s_upd_level == UPD_OFF) return;
  if (!telemChanged(p_onlineGraph_01_distance, s_lastDist, 1.0f)) return;
  sendValue("gauge_distance", String(p_onlineGraph_01_distance, 0));
}

void remotexy_set_onlineGraph_02_speed( float p_onlineGraph_02_speed) {
  if (s_upd_level == UPD_OFF) return;
  if (!telemChanged(p_onlineGraph_02_speed, s_lastSpeed, 1.0f)) return;
  sendValue("gauge_speed", String(p_onlineGraph_02_speed, 0));
}

void remotexy_set_onlineGraph_03_battery( float p_onlineGraph_03_battery) {
  if (s_upd_level == UPD_OFF) return;
  if (!telemChanged(p_onlineGraph_03_battery, s_lastBatt, 1.0f)) return;
  sendValue("battery_level", String(p_onlineGraph_03_battery, 0));
}

// Deliberately NOT send-on-change. A graph is a time series: the app plots a
// point per message, so suppressing repeats would compress the flat stretches
// and distort the shape of the trace rather than just saving traffic. This is
// the one value that genuinely wants every cycle — which is why it is "All"
// only.
void remotexy_send_graph_distance(float cm) {
  if (!telemExpertOnly()) return;   // graph_dist exists only in the expert layout
  if (s_upd_level != UPD_ALL) return;
  sendValue("graph_dist", String(cm, 0));
}

// Edge-triggered, with hysteresis: the app raises a toast per message, so
// this fires once on entering the obstacle band and re-arms only after the
// robot has backed well clear of it.
void remotexy_send_obstacle_alert(float cm) {
  if (!telemExpertOnly()) return;   // `alert` exists only in the expert layout
  if (s_upd_level == UPD_OFF) return;
  // A zero reading means "no echo", not "wall against the sensor" — treating
  // it as an obstacle would fire the alert continuously with nothing there.
  if (cm <= 0.0f) { return; }

  if (!s_obstacleLatched && cm < B3_OBSTACLE_CM) {
    s_obstacleLatched = true;
    sendValue("alert", "Obstacle " + String(cm, 0) + "cm");
  } else if (s_obstacleLatched && cm > B3_OBSTACLE_CLEAR_CM) {
    s_obstacleLatched = false;
  }
}

// Firmware version, uptime and pack voltage. Static-ish text, so "All" only —
// there is no value in spending a notification per cycle on a string that
// rarely changes.
void remotexy_send_system_labels(void) {
  if (!telemExpertOnly()) return;   // lbl_* exist only in the expert layout
  if (s_upd_level != UPD_ALL) return;

  // The version string never changes at runtime — one send per CFG is enough.
  if (s_telemForce || !s_verSent) {
    sendValue("lbl_ver", B3_FIRMWARE_VERSION);
    s_verSent = true;
  }

  // Uptime is only ever displayed to the second, so only send when the second
  // actually rolls over rather than on every telemetry cycle.
  const uint32_t upSec = g_elapsed_time_hours * 3600u
                       + g_elapsed_time_minutes * 60u
                       + g_elapsed_time_seconds;
  if (s_telemForce || upSec != s_lastUptimeSec) {
    s_lastUptimeSec = upSec;
    char up[16];
    snprintf(up, sizeof(up), "%02u:%02u:%02u",
             (unsigned)g_elapsed_time_hours,
             (unsigned)g_elapsed_time_minutes,
             (unsigned)g_elapsed_time_seconds);
    sendValue("lbl_uptime", String(up));
  }

  // The battery widget already shows percentage; the raw voltage is what
  // actually reveals a sagging cell, so publish both. 0.05V deadband keeps
  // ADC noise from making the last decimal flicker every cycle.
  if (telemChanged(g_battery_voltage, s_lastVbat, 0.05f)) {
    sendValue("lbl_vbat", String(g_battery_voltage, 2) + "V");
  }
}

// Board push button, mirrored as an indicator. On change only — it is the only
// physical input the robot has and it spends nearly all of its time released.
void remotexy_send_button_state(void) {
  if (!telemExpertOnly()) return;   // led_button exists only in the expert layout
  if (s_upd_level == UPD_OFF) return;
  const int8_t now = button_pressed() ? 1 : 0;
  if (!s_telemForce && now == s_lastBtn) return;
  s_lastBtn = now;
  sendValue("led_button", now ? "1" : "0");
}

// BLE link quality. ble_gap_conn_rssi() is a host-side query against the live
// connection, so it needs the handle captured in onConnect() and returns
// non-zero if that handle is stale — in which case publish nothing rather
// than a fabricated number.
void remotexy_send_link_rssi(void) {
  if (!telemExpertOnly()) return;   // gauge_rssi exists only in the expert layout
  if (s_upd_level != UPD_ALL) return;
  if (s_connHandle == BLE_HS_CONN_HANDLE_NONE) return;
  int8_t rssi = 0;
  if (ble_gap_conn_rssi(s_connHandle, &rssi) != 0) return;
  // 2 dBm deadband: RSSI drifts a dB or so continuously even on a stationary
  // robot, which would otherwise make this the chattiest widget on the panel.
  if (!s_telemForce && abs((int)rssi - (int)s_lastRssi) < 2) return;
  s_lastRssi = rssi;
  sendValue("gauge_rssi", String((int)rssi));
}

// Mirrors the OLED's top line back to the panel, so the virtual display shows
// what the physical one actually reads rather than what was requested. They can
// differ: text longer than 21 characters is truncated to the panel width, and
// with no override the screen shows its banner instead. Sent on change only —
// this string is static most of the time.
void remotexy_send_oled_mirror(void) {
  if (!telemExpertOnly()) return;   // lbl_oled exists only in the expert layout
  if (s_upd_level == UPD_OFF) return;
  const String now = oled_text_active()
                       ? String(oled_text_get())
                       : (remotexy_get_connect_flag() ? String("Workshop.3")
                                                      : String(CONFIG_BLE_DEVICE_NAME));
  if (!s_telemForce && now == s_lastOled) return;
  s_lastOled = now;
  sendValue("lbl_oled", now);
  // Echo the field itself too, so a reconnect repopulates what was typed
  // instead of showing an empty box over a screen that still has text on it.
  if (s_telemForce) sendValue("oled_text", oled_text_active() ? oled_text_get() : "");
}

// Echoes the two selectors back to the app. Without this they render showing
// their FIRST option regardless of the real state — a panel serving the expert
// layout would still read "Level: Beginner", and "Telemetry: Off" while
// telemetry was visibly streaming. Deliberately NOT gated on s_upd_level: the
// Telemetry selector must show its own true position even when set to Off,
// otherwise there is no way to see what you are about to change.
void remotexy_send_control_echo(void) {
  if (!s_telemForce) return;   // only after a CFG transfer; they cannot drift
  // `level` is in BOTH layouts — it is the way back — so it is always echoed.
  sendValue("level", s_layout_level == LAYOUT_EXPERT ? "Expert" : "Beginner");
  // `upd` only exists in the expert layout.
  if (!telemExpertOnly()) return;
  sendValue("upd", s_upd_level == UPD_OFF   ? "Off"
                 : s_upd_level == UPD_BASIC ? "Basic"
                                            : "All");
}

// Called once at the end of each telemetry pass. Clearing the force flag here
// rather than inside a particular sender means the "send everything" pass is
// exactly one full cycle, whichever widgets happen to be in the layout.
void remotexy_telemetry_end(void) {
  s_telemForce = false;
}

uint8_t remotexy_get_led_manual(void) { return s_led_manual ? 1 : 0; }
uint8_t remotexy_get_led_r(void)      { return s_led_r_on ? 1 : 0; }
uint8_t remotexy_get_led_g(void)      { return s_led_g_on ? 1 : 0; }

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
