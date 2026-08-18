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
  "ZXZlbCIsIm9wdGlvbnMiOiJCZWdpbm5lcixFeHBlcnQsTW90b3JzLERpc3RhbmNlLExpZ2h0"
  "cyxTb3VuZCxEaXNwbGF5LFBvd2VyIiwiZ3JvdXBJZCI6ImdycF9zeXMifSx7ImlkIjoibG9n"
  "byIsInQiOiJpbWFnZSIsIngiOjU3OCwieSI6NzkwLCJ3IjoxNjAsImgiOjYwLCJsYWJlbCI6"
  "IldvcmtzaG9wLURJWSIsImltYWdlU3JjIjoiYXNzZXRzL3dvcmtzaG9wLWRpeS1sb2dvLnN2"
  "ZyIsImdyb3VwSWQiOiJncnBfc3lzIn1dfQ==";

static const char* LAYOUT_CFG_EXPERT_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoyLCJ0aXRsZSI6IldESVkgUm9ib3QgYjMiLCJ3aWRnZXRzIjpb"
  "eyJpZCI6ImdycF9kcml2ZSIsInQiOiJncm91cCIsImxhYmVsIjoiRFJJVkUiLCJ4Ijo1Mywi"
  "eSI6MTUsInciOjY5MSwiaCI6NTUxLCJjb2xvciI6IiMwMGQ0ZmYiLCJjaGlsZHJlbiI6WyJk"
  "cGFkX2RyaXZlIiwiam95X2RyaXZlIiwic3BkIiwiYnRuX3N0b3AiLCJnYXVnZV9zcGVlZCJd"
  "fSx7ImlkIjoiZ3JwX2Rpc3QiLCJ0IjoiZ3JvdXAiLCJsYWJlbCI6IkRJU1RBTkNFIiwieCI6"
  "Nzc1LCJ5IjoxMywidyI6NDI2LCJoIjo1NTEsImNvbG9yIjoiI2ZmYjAyMCIsImNoaWxkcmVu"
  "IjpbImdhdWdlX2Rpc3RhbmNlIiwiYWxlcnQiLCJncmFwaF9kaXN0Il19LHsiaWQiOiJncnBf"
  "bGlnaHQiLCJ0IjoiZ3JvdXAiLCJsYWJlbCI6IkxJR0hUUyIsIngiOjUwLCJ5Ijo1NzQsInci"
  "OjY4OSwiaCI6Mzg1LCJjb2xvciI6IiM3YzVjZmYiLCJjaGlsZHJlbiI6WyJ0b2dnbGVfbGVk"
  "X3IiLCJ0b2dnbGVfbGVkX2ciLCJ0b2dnbGVfbnAiLCJucF9lZmZlY3QiLCJucF9yIiwibnBf"
  "ZyIsIm5wX2IiLCJucF9icmlnaHQiLCJsZWRfcl9zdGF0ZSIsImxlZF9nX3N0YXRlIl19LHsi"
  "aWQiOiJncnBfc291bmQiLCJ0IjoiZ3JvdXAiLCJsYWJlbCI6IlNPVU5EIiwieCI6NzYzLCJ5"
  "Ijo2MTAsInciOjQzMywiaCI6MTgwLCJjb2xvciI6IiNmZjVjOGEiLCJjaGlsZHJlbiI6WyJi"
  "dG5faG9ybiIsImJ0bl9idXp6Iiwic291bmRfYWxlcnQiXX0seyJpZCI6ImdycF9zeXMiLCJ0"
  "IjoiZ3JvdXAiLCJsYWJlbCI6IlNZU1RFTSIsIngiOjQ2LCJ5Ijo5OTksInciOjEwNzIsImgi"
  "OjI1MywiY29sb3IiOiIjM2RkYzk3IiwiY2hpbGRyZW4iOlsiYmF0dGVyeV9sZXZlbCIsImxi"
  "bF92ZXIiLCJsYmxfdXB0aW1lIiwibGJsX3ZiYXQiLCJ1cGQiLCJsZXZlbCIsImxlZF9idXR0"
  "b24iLCJnYXVnZV9yc3NpIiwibG9nbyJdfSx7ImlkIjoiZ3JwX2Rpc3BsYXkiLCJ0IjoiZ3Jv"
  "dXAiLCJsYWJlbCI6IkRJU1BMQVkiLCJ4Ijo3NiwieSI6MTI4MiwidyI6NjE4LCJoIjoxNDQs"
  "ImNvbG9yIjoiIzAwZTVmZiIsImNoaWxkcmVuIjpbIm9sZWRfdGV4dCIsImxibF9vbGVkIl19"
  "LHsiaWQiOiJzZXBfY29scyIsInQiOiJzZXBhcmF0b3IiLCJ4Ijo3NTMsInkiOjQ5LCJ3Ijo4"
  "LCJoIjo0OTN9LHsiaWQiOiJzZXBfYjEiLCJ0Ijoic2VwYXJhdG9yIiwieCI6NzczLCJ5Ijo1"
  "NzQsInciOjQxMiwiaCI6MTJ9LHsiaWQiOiJzZXBfYjIiLCJ0Ijoic2VwYXJhdG9yIiwieCI6"
  "NTUsInkiOjk3MiwidyI6MTA0NCwiaCI6OH0seyJpZCI6ImRwYWRfZHJpdmUiLCJ0IjoiZHBh"
  "ZCIsIngiOjc3LCJ5Ijo3MywidyI6MzcxLCJoIjo0NzQsImxhYmVsIjoiRHJpdmUiLCJtb2Rl"
  "bCI6ImNsYXNzaWMiLCJncm91cElkIjoiZ3JwX2RyaXZlIn0seyJpZCI6ImpveV9kcml2ZSIs"
  "InQiOiJqb3lzdGljayIsIngiOjQ1NywieSI6NzQsInciOjE3NCwiaCI6MTc1LCJsYWJlbCI6"
  "IkRyaXZlIiwiZ3JvdXBJZCI6ImdycF9kcml2ZSJ9LHsiaWQiOiJzcGQiLCJ0Ijoic2xpZGVy"
  "IiwieCI6NjQxLCJ5Ijo3NSwidyI6OTEsImgiOjI5MCwibGFiZWwiOiJTcGVlZCIsIm1heCI6"
  "MTAwLCJ2YWx1ZSI6MTAwLCJncm91cElkIjoiZ3JwX2RyaXZlIn0seyJpZCI6ImJ0bl9zdG9w"
  "IiwidCI6ImJ1dHRvbiIsIngiOjQ1NSwieSI6NDIzLCJ3IjoxMjAsImgiOjEyMCwibGFiZWwi"
  "OiJTVE9QIiwibW9kZWwiOiJmbGF0IiwiZ3JvdXBJZCI6ImdycF9kcml2ZSJ9LHsiaWQiOiJn"
  "YXVnZV9zcGVlZCIsInQiOiJnYXVnZSIsIngiOjU4MiwieSI6Mzc1LCJ3IjoxNDksImgiOjE3"
  "MSwibGFiZWwiOiJTcGVlZCIsIm1pbiI6MCwibWF4IjoxMDAsInVuaXRzIjoiJSIsImRlY2lt"
  "YWxzIjowLCJtb2RlbCI6ImNsYXNzaWMiLCJncm91cElkIjoiZ3JwX2RyaXZlIn0seyJpZCI6"
  "ImdhdWdlX2Rpc3RhbmNlIiwidCI6ImdhdWdlIiwieCI6ODA2LCJ5Ijo2MywidyI6MTQwLCJo"
  "IjoxNjMsImxhYmVsIjoiRGlzdGFuY2UiLCJtaW4iOjAsIm1heCI6MjAwLCJ1bml0cyI6ImNt"
  "IiwiZGVjaW1hbHMiOjAsIm1vZGVsIjoiY2xhc3NpYyIsImdyb3VwSWQiOiJncnBfZGlzdCJ9"
  "LHsiaWQiOiJhbGVydCIsInQiOiJub3RpZmljYXRpb24iLCJ4IjoxMDI3LCJ5IjoxMDAsInci"
  "OjkwLCJoIjo5MCwibGFiZWwiOiJPYnN0YWNsZSIsImdyb3VwSWQiOiJncnBfZGlzdCJ9LHsi"
  "aWQiOiJncmFwaF9kaXN0IiwidCI6ImdyYXBoIiwieCI6ODAxLCJ5IjoyMjYsInciOjM4Nywi"
  "aCI6MzEzLCJsYWJlbCI6IkRpc3RhbmNlIGNtIiwibWF4IjoyMDAsImdyb3VwSWQiOiJncnBf"
  "ZGlzdCJ9LHsiaWQiOiJ0b2dnbGVfbGVkX3IiLCJ0IjoidG9nZ2xlIiwieCI6NzQsInkiOjYz"
  "MiwidyI6MTA4LCJoIjoxMDAsImxhYmVsIjoiUmVkIExFRCIsIm1vZGVsIjoicGlsbCIsImdy"
  "b3VwSWQiOiJncnBfbGlnaHQifSx7ImlkIjoidG9nZ2xlX2xlZF9nIiwidCI6InRvZ2dsZSIs"
  "IngiOjE5NCwieSI6NjMyLCJ3IjoxMDcsImgiOjEwMCwibGFiZWwiOiJHcmVlbiBMRUQiLCJt"
  "b2RlbCI6InBpbGwiLCJncm91cElkIjoiZ3JwX2xpZ2h0In0seyJpZCI6InRvZ2dsZV9ucCIs"
  "InQiOiJ0b2dnbGUiLCJ4Ijo0MzQsInkiOjYxMiwidyI6MTAwLCJoIjoxMDAsImxhYmVsIjoi"
  "U3RyaXAiLCJtb2RlbCI6InBpbGwiLCJncm91cElkIjoiZ3JwX2xpZ2h0In0seyJpZCI6Im5w"
  "X2VmZmVjdCIsInQiOiJzZWxlY3QiLCJ4Ijo1NjYsInkiOjYxNiwidyI6MTYwLCJoIjo3MCwi"
  "bGFiZWwiOiJTdHJpcCBlZmZlY3QiLCJvcHRpb25zIjoiU29saWQsUmFpbmJvdyxLbmlnaHQg"
  "UmlkZXIsRHVlbCBleWUsRnJlbmNoIGZsYWciLCJncm91cElkIjoiZ3JwX2xpZ2h0In0seyJp"
  "ZCI6Im5wX3IiLCJ0Ijoic2xpZGVyIiwieCI6Mzc4LCJ5Ijo3NjIsInciOjcwLCJoIjoxODAs"
  "ImxhYmVsIjoiUiIsIm1heCI6MjU1LCJ2YWx1ZSI6MjU1LCJncm91cElkIjoiZ3JwX2xpZ2h0"
  "In0seyJpZCI6Im5wX2ciLCJ0Ijoic2xpZGVyIiwieCI6NDU2LCJ5Ijo3NjAsInciOjcwLCJo"
  "IjoxODAsImxhYmVsIjoiRyIsIm1heCI6MjU1LCJ2YWx1ZSI6MCwiZ3JvdXBJZCI6ImdycF9s"
  "aWdodCJ9LHsiaWQiOiJucF9iIiwidCI6InNsaWRlciIsIngiOjUzNywieSI6NzYyLCJ3Ijo3"
  "MCwiaCI6MTgwLCJsYWJlbCI6IkIiLCJtYXgiOjI1NSwidmFsdWUiOjAsImdyb3VwSWQiOiJn"
  "cnBfbGlnaHQifSx7ImlkIjoibnBfYnJpZ2h0IiwidCI6InNsaWRlciIsIngiOjY0MywieSI6"
  "NzYxLCJ3Ijo5MCwiaCI6MTgwLCJsYWJlbCI6IkJyaWdodG5lc3MiLCJtYXgiOjI1NSwic3Rl"
  "cCI6NSwidmFsdWUiOjE1LCJncm91cElkIjoiZ3JwX2xpZ2h0In0seyJpZCI6ImJ0bl9ob3Ju"
  "IiwidCI6ImJ1dHRvbiIsIngiOjc4MSwieSI6NjYzLCJ3IjoxMDAsImgiOjEwMCwibGFiZWwi"
  "OiJIb3JuIiwiZ3JvdXBJZCI6ImdycF9zb3VuZCJ9LHsiaWQiOiJidG5fYnV6eiIsInQiOiJi"
  "dXR0b24iLCJ4Ijo5MjQsInkiOjY2MCwidyI6MTAwLCJoIjoxMDAsImxhYmVsIjoiQnV6eiIs"
  "Imdyb3VwSWQiOiJncnBfc291bmQifSx7ImlkIjoic291bmRfYWxlcnQiLCJ0Ijoic291bmQi"
  "LCJ4IjoxMDgxLCJ5Ijo2NjQsInciOjkwLCJoIjo5MCwibGFiZWwiOiJBbGVydCIsImdyb3Vw"
  "SWQiOiJncnBfc291bmQifSx7ImlkIjoiYmF0dGVyeV9sZXZlbCIsInQiOiJiYXR0ZXJ5Iiwi"
  "eCI6NjAyLCJ5IjoxMTA3LCJ3Ijo4MCwiaCI6MTAwLCJsYWJlbCI6IkJhdHRlcnkiLCJtb2Rl"
  "bCI6InZlcnRpY2FsIiwiZ3JvdXBJZCI6ImdycF9zeXMifSx7ImlkIjoibGJsX3ZlciIsInQi"
  "OiJsYWJlbCIsIngiOjYxLCJ5IjoxMDQ5LCJ3IjoyMDAsImgiOjUwLCJsYWJlbCI6IkZpcm13"
  "YXJlIiwibW9kZWwiOiJjYXJkIiwiZ3JvdXBJZCI6ImdycF9zeXMifSx7ImlkIjoibGJsX3Vw"
  "dGltZSIsInQiOiJsYWJlbCIsIngiOjI3MSwieSI6MTA1MSwidyI6MjAwLCJoIjo1MCwibGFi"
  "ZWwiOiJVcHRpbWUiLCJtb2RlbCI6ImNhcmQiLCJncm91cElkIjoiZ3JwX3N5cyJ9LHsiaWQi"
  "OiJsYmxfdmJhdCIsInQiOiJsYWJlbCIsIngiOjQ4OCwieSI6MTA1MSwidyI6MjAwLCJoIjo1"
  "MCwibGFiZWwiOiJCYXR0ZXJ5IFYiLCJtb2RlbCI6ImNhcmQiLCJncm91cElkIjoiZ3JwX3N5"
  "cyJ9LHsiaWQiOiJ1cGQiLCJ0Ijoic2VsZWN0IiwieCI6ODQsInkiOjExMjcsInciOjE2MCwi"
  "aCI6NzAsImxhYmVsIjoiVGVsZW1ldHJ5Iiwib3B0aW9ucyI6Ik9mZixCYXNpYyxBbGwiLCJn"
  "cm91cElkIjoiZ3JwX3N5cyJ9LHsiaWQiOiJsZXZlbCIsInQiOiJzZWxlY3QiLCJ4IjoyODQs"
  "InkiOjExMjksInciOjE2MCwiaCI6NzAsImxhYmVsIjoiTGV2ZWwiLCJvcHRpb25zIjoiQmVn"
  "aW5uZXIsRXhwZXJ0LE1vdG9ycyxEaXN0YW5jZSxMaWdodHMsU291bmQsRGlzcGxheSxQb3dl"
  "ciIsImdyb3VwSWQiOiJncnBfc3lzIn0seyJpZCI6ImxlZF9idXR0b24iLCJ0IjoibGVkIiwi"
  "eCI6NDg1LCJ5IjoxMTE5LCJ3Ijo4MCwiaCI6ODAsImxhYmVsIjoiQnV0dG9uIiwibW9kZWwi"
  "OiJkb3QiLCJjb2xvck9uIjoiIzAwZmY4OCIsImdyb3VwSWQiOiJncnBfc3lzIn0seyJpZCI6"
  "ImdhdWdlX3Jzc2kiLCJ0IjoiZ2F1Z2UiLCJ4Ijo3MDQsInkiOjEwNTgsInciOjE4OSwiaCI6"
  "MTkwLCJsYWJlbCI6IlNpZ25hbCIsIm1pbiI6LTEwMCwibWF4IjotMzAsInVuaXRzIjoiZEJt"
  "IiwiZGVjaW1hbHMiOjAsIm1vZGVsIjoiY2xhc3NpYyIsImdyb3VwSWQiOiJncnBfc3lzIn0s"
  "eyJpZCI6ImxvZ28iLCJ0IjoiaW1hZ2UiLCJ4Ijo5MTAsInkiOjEwNTIsInciOjE5MiwiaCI6"
  "MTY0LCJsYWJlbCI6IldvcmtzaG9wLURJWSIsImltYWdlU3JjIjoiYXNzZXRzL3dvcmtzaG9w"
  "LWRpeS1sb2dvLnN2ZyIsImdyb3VwSWQiOiJncnBfc3lzIn0seyJpZCI6Im9sZWRfdGV4dCIs"
  "InQiOiJlZGl0ZmllbGQiLCJ4IjoxMDAsInkiOjEzMzIsInciOjI2MCwiaCI6NzAsImxhYmVs"
  "IjoiT0xFRCB0ZXh0IiwicGxhY2Vob2xkZXIiOiJUeXBlIGZvciB0aGUgcm9ib3Qgc2NyZWVu"
  "Li4uIiwiZ3JvdXBJZCI6ImdycF9kaXNwbGF5In0seyJpZCI6ImxibF9vbGVkIiwidCI6Imxh"
  "YmVsIiwieCI6MzkwLCJ5IjoxMzM3LCJ3IjoyODAsImgiOjYwLCJsYWJlbCI6Ik9MRUQgc2hv"
  "d3MiLCJtb2RlbCI6ImNhcmQiLCJncm91cElkIjoiZ3JwX2Rpc3BsYXkifSx7ImlkIjoibGVk"
  "X3Jfc3RhdGUiLCJ0IjoibGVkIiwieCI6OTMsInkiOjc2OSwidyI6ODAsImgiOjgwLCJsYWJl"
  "bCI6IlJlZCBzdGF0dXMiLCJtb2RlbCI6ImRvdCIsImNvbG9yT24iOiIjZmY1MjUyIiwiZ3Jv"
  "dXBJZCI6ImdycF9saWdodCJ9LHsiaWQiOiJsZWRfZ19zdGF0ZSIsInQiOiJsZWQiLCJ4Ijoy"
  "MDUsInkiOjc2NSwidyI6ODAsImgiOjgwLCJsYWJlbCI6IkdyZWVuIHN0YXR1cyIsIm1vZGVs"
  "IjoiZG90IiwiY29sb3JPbiI6IiMwMGZmODgiLCJncm91cElkIjoiZ3JwX2xpZ2h0In1dLCJj"
  "YW52YXMiOnsidyI6MTI0NywiaCI6MTQ3Mn19";

static const char* LAYOUT_CFG_TEST_MOTORS_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6ImIzIC0gTW90b3JzIHRlc3QiLCJjYW52YXMi"
  "OnsidyI6ODAwLCJoIjo3MTZ9LCJ3aWRnZXRzIjpbeyJpZCI6ImdycF90ZXN0IiwidCI6Imdy"
  "b3VwIiwibGFiZWwiOiJNT1RPUlMiLCJjb2xvciI6IiMwMGQ0ZmYiLCJ4Ijo1NiwieSI6NDIs"
  "InciOjY4OCwiaCI6NTQyLCJjaGlsZHJlbiI6WyJkcGFkX2RyaXZlIiwic3BkIiwiYnRuX3N0"
  "b3AiLCJnYXVnZV9zcGVlZCIsImxldmVsIl19LHsiaWQiOiJkcGFkX2RyaXZlIiwidCI6ImRw"
  "YWQiLCJ4Ijo4MCwieSI6MTAwLCJ3IjozMDAsImgiOjMwMCwibGFiZWwiOiJEcml2ZSIsIm1v"
  "ZGVsIjoiY2xhc3NpYyIsImdyb3VwSWQiOiJncnBfdGVzdCJ9LHsiaWQiOiJzcGQiLCJ0Ijoi"
  "c2xpZGVyIiwieCI6NDIwLCJ5IjoxMDAsInciOjkwLCJoIjoyMDAsImxhYmVsIjoiU3BlZWQi"
  "LCJtYXgiOjEwMCwidmFsdWUiOjEwMCwiZ3JvdXBJZCI6ImdycF90ZXN0In0seyJpZCI6ImJ0"
  "bl9zdG9wIiwidCI6ImJ1dHRvbiIsIngiOjQyMCwieSI6MzMwLCJ3IjoxMjAsImgiOjEyMCwi"
  "bGFiZWwiOiJTVE9QIiwibW9kZWwiOiJmbGF0IiwiZ3JvdXBJZCI6ImdycF90ZXN0In0seyJp"
  "ZCI6ImdhdWdlX3NwZWVkIiwidCI6ImdhdWdlIiwieCI6NTcwLCJ5IjoxMDAsInciOjE1MCwi"
  "aCI6MTkwLCJsYWJlbCI6IlNwZWVkIiwibWF4IjoxMDAsInVuaXRzIjoiJSIsImRlY2ltYWxz"
  "IjowLCJncm91cElkIjoiZ3JwX3Rlc3QifSx7ImlkIjoibGV2ZWwiLCJ0Ijoic2VsZWN0Iiwi"
  "eCI6ODAsInkiOjQ5MCwidyI6MjAwLCJoIjo3MCwibGFiZWwiOiJUZXN0Iiwib3B0aW9ucyI6"
  "IkJlZ2lubmVyLEV4cGVydCxNb3RvcnMsRGlzdGFuY2UsTGlnaHRzLFNvdW5kLERpc3BsYXks"
  "UG93ZXIiLCJncm91cElkIjoiZ3JwX3Rlc3QifSx7ImlkIjoibGJsX2hpbnQiLCJ0IjoibGFi"
  "ZWwiLCJ4Ijo4MCwieSI6NjAwLCJ3Ijo2NDAsImgiOjYwLCJsYWJlbCI6IlByZXNzIGFuIGFy"
  "cm93LiBUaGUgd2hlZWxzIHNob3VsZCB0dXJuIHRoYXQgd2F5LiIsInZhbHVlIjoiUHJlc3Mg"
  "YW4gYXJyb3cuIFRoZSB3aGVlbHMgc2hvdWxkIHR1cm4gdGhhdCB3YXkuIiwibW9kZWwiOiJj"
  "YXJkIn1dfQ==";

static const char* LAYOUT_CFG_TEST_DISTANCE_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6ImIzIC0gRGlzdGFuY2UgdGVzdCIsImNhbnZh"
  "cyI6eyJ3Ijo1NDAsImgiOjc4Nn0sIndpZGdldHMiOlt7ImlkIjoiZ3JwX3Rlc3QiLCJ0Ijoi"
  "Z3JvdXAiLCJsYWJlbCI6IkRJU1RBTkNFIiwiY29sb3IiOiIjZmZiMDIwIiwieCI6NTYsInki"
  "OjQyLCJ3Ijo0MjgsImgiOjYxMiwiY2hpbGRyZW4iOlsiZ2F1Z2VfZGlzdGFuY2UiLCJhbGVy"
  "dCIsImdyYXBoX2Rpc3QiLCJsZXZlbCJdfSx7ImlkIjoiZ2F1Z2VfZGlzdGFuY2UiLCJ0Ijoi"
  "Z2F1Z2UiLCJ4Ijo4MCwieSI6MTAwLCJ3IjoxNTAsImgiOjE5MCwibGFiZWwiOiJEaXN0YW5j"
  "ZSIsIm1heCI6MjAwLCJ1bml0cyI6ImNtIiwiZGVjaW1hbHMiOjAsImdyb3VwSWQiOiJncnBf"
  "dGVzdCJ9LHsiaWQiOiJhbGVydCIsInQiOiJub3RpZmljYXRpb24iLCJ4IjoyNjAsInkiOjEx"
  "MCwidyI6MTEwLCJoIjoxMTAsImxhYmVsIjoiT2JzdGFjbGUiLCJncm91cElkIjoiZ3JwX3Rl"
  "c3QifSx7ImlkIjoiZ3JhcGhfZGlzdCIsInQiOiJncmFwaCIsIngiOjgwLCJ5IjozMjAsInci"
  "OjM4MCwiaCI6MjAwLCJsYWJlbCI6IkRpc3RhbmNlIGNtIiwibWF4IjoyMDAsImdyb3VwSWQi"
  "OiJncnBfdGVzdCJ9LHsiaWQiOiJsZXZlbCIsInQiOiJzZWxlY3QiLCJ4Ijo4MCwieSI6NTYw"
  "LCJ3IjoyMDAsImgiOjcwLCJsYWJlbCI6IlRlc3QiLCJvcHRpb25zIjoiQmVnaW5uZXIsRXhw"
  "ZXJ0LE1vdG9ycyxEaXN0YW5jZSxMaWdodHMsU291bmQsRGlzcGxheSxQb3dlciIsImdyb3Vw"
  "SWQiOiJncnBfdGVzdCJ9LHsiaWQiOiJsYmxfaGludCIsInQiOiJsYWJlbCIsIngiOjgwLCJ5"
  "Ijo2NzAsInciOjM4MCwiaCI6NjAsImxhYmVsIjoiTW92ZSB5b3VyIGhhbmQgaW4gZnJvbnQg"
  "b2YgdGhlIHNlbnNvci4iLCJ2YWx1ZSI6Ik1vdmUgeW91ciBoYW5kIGluIGZyb250IG9mIHRo"
  "ZSBzZW5zb3IuIiwibW9kZWwiOiJjYXJkIn1dfQ==";

static const char* LAYOUT_CFG_TEST_LIGHTS_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6ImIzIC0gTGlnaHRzIHRlc3QiLCJjYW52YXMi"
  "OnsidyI6NjMwLCJoIjo4NTZ9LCJ3aWRnZXRzIjpbeyJpZCI6ImdycF90ZXN0IiwidCI6Imdy"
  "b3VwIiwibGFiZWwiOiJMSUdIVFMiLCJjb2xvciI6IiM3YzVjZmYiLCJ4Ijo1NiwieSI6NDIs"
  "InciOjUxOCwiaCI6NjgyLCJjaGlsZHJlbiI6WyJ0b2dnbGVfbGVkX3IiLCJ0b2dnbGVfbGVk"
  "X2ciLCJsZWRfcl9zdGF0ZSIsImxlZF9nX3N0YXRlIiwidG9nZ2xlX25wIiwibnBfZWZmZWN0"
  "IiwibnBfciIsIm5wX2ciLCJucF9iIiwibnBfYnJpZ2h0IiwibGV2ZWwiXX0seyJpZCI6InRv"
  "Z2dsZV9sZWRfciIsInQiOiJ0b2dnbGUiLCJ4Ijo4MCwieSI6MTAwLCJ3IjoxMTAsImgiOjEx"
  "MCwibGFiZWwiOiJSZWQgTEVEIiwibW9kZWwiOiJwaWxsIiwiZ3JvdXBJZCI6ImdycF90ZXN0"
  "In0seyJpZCI6InRvZ2dsZV9sZWRfZyIsInQiOiJ0b2dnbGUiLCJ4IjoyMTAsInkiOjEwMCwi"
  "dyI6MTEwLCJoIjoxMTAsImxhYmVsIjoiR3JlZW4gTEVEIiwibW9kZWwiOiJwaWxsIiwiZ3Jv"
  "dXBJZCI6ImdycF90ZXN0In0seyJpZCI6ImxlZF9yX3N0YXRlIiwidCI6ImxlZCIsIngiOjM1"
  "MCwieSI6MTEwLCJ3Ijo5MCwiaCI6OTAsImxhYmVsIjoiUmVkIGlzIiwibW9kZWwiOiJkb3Qi"
  "LCJjb2xvck9uIjoiI2ZmNTI1MiIsImNvbG9yT2ZmIjoiIzJhMmEzYSIsImdyb3VwSWQiOiJn"
  "cnBfdGVzdCJ9LHsiaWQiOiJsZWRfZ19zdGF0ZSIsInQiOiJsZWQiLCJ4Ijo0NjAsInkiOjEx"
  "MCwidyI6OTAsImgiOjkwLCJsYWJlbCI6IkdyZWVuIGlzIiwibW9kZWwiOiJkb3QiLCJjb2xv"
  "ck9uIjoiIzAwZmY4OCIsImNvbG9yT2ZmIjoiIzJhMmEzYSIsImdyb3VwSWQiOiJncnBfdGVz"
  "dCJ9LHsiaWQiOiJ0b2dnbGVfbnAiLCJ0IjoidG9nZ2xlIiwieCI6ODAsInkiOjI1MCwidyI6"
  "MTEwLCJoIjoxMTAsImxhYmVsIjoiU3RyaXAiLCJtb2RlbCI6InBpbGwiLCJncm91cElkIjoi"
  "Z3JwX3Rlc3QifSx7ImlkIjoibnBfZWZmZWN0IiwidCI6InNlbGVjdCIsIngiOjIxMCwieSI6"
  "MjcwLCJ3IjoxOTAsImgiOjcwLCJsYWJlbCI6IkVmZmVjdCIsIm9wdGlvbnMiOiJTb2xpZCxS"
  "YWluYm93LEtuaWdodCBSaWRlcixEdWVsIGV5ZSxGcmVuY2ggZmxhZyIsImdyb3VwSWQiOiJn"
  "cnBfdGVzdCJ9LHsiaWQiOiJucF9yIiwidCI6InNsaWRlciIsIngiOjgwLCJ5Ijo0MDAsInci"
  "OjgwLCJoIjoxOTAsImxhYmVsIjoiUiIsIm1heCI6MjU1LCJ2YWx1ZSI6MjU1LCJncm91cElk"
  "IjoiZ3JwX3Rlc3QifSx7ImlkIjoibnBfZyIsInQiOiJzbGlkZXIiLCJ4IjoxODAsInkiOjQw"
  "MCwidyI6ODAsImgiOjE5MCwibGFiZWwiOiJHIiwibWF4IjoyNTUsInZhbHVlIjowLCJncm91"
  "cElkIjoiZ3JwX3Rlc3QifSx7ImlkIjoibnBfYiIsInQiOiJzbGlkZXIiLCJ4IjoyODAsInki"
  "OjQwMCwidyI6ODAsImgiOjE5MCwibGFiZWwiOiJCIiwibWF4IjoyNTUsInZhbHVlIjowLCJn"
  "cm91cElkIjoiZ3JwX3Rlc3QifSx7ImlkIjoibnBfYnJpZ2h0IiwidCI6InNsaWRlciIsIngi"
  "OjM5MCwieSI6NDAwLCJ3IjoxMDAsImgiOjE5MCwibGFiZWwiOiJCcmlnaHQiLCJtYXgiOjI1"
  "NSwic3RlcCI6NSwidmFsdWUiOjE1LCJncm91cElkIjoiZ3JwX3Rlc3QifSx7ImlkIjoibGV2"
  "ZWwiLCJ0Ijoic2VsZWN0IiwieCI6ODAsInkiOjYzMCwidyI6MjAwLCJoIjo3MCwibGFiZWwi"
  "OiJUZXN0Iiwib3B0aW9ucyI6IkJlZ2lubmVyLEV4cGVydCxNb3RvcnMsRGlzdGFuY2UsTGln"
  "aHRzLFNvdW5kLERpc3BsYXksUG93ZXIiLCJncm91cElkIjoiZ3JwX3Rlc3QifSx7ImlkIjoi"
  "bGJsX2hpbnQiLCJ0IjoibGFiZWwiLCJ4Ijo4MCwieSI6NzQwLCJ3Ijo0NzAsImgiOjYwLCJs"
  "YWJlbCI6IlN3aXRjaCB0aGUgbGlnaHRzIG9uLCB0aGVuIG1peCBhIGNvbG91ci4iLCJ2YWx1"
  "ZSI6IlN3aXRjaCB0aGUgbGlnaHRzIG9uLCB0aGVuIG1peCBhIGNvbG91ci4iLCJtb2RlbCI6"
  "ImNhcmQifV19";

static const char* LAYOUT_CFG_TEST_SOUND_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6ImIzIC0gU291bmQgdGVzdCIsImNhbnZhcyI6"
  "eyJ3Ijo1NzAsImgiOjQ4Nn0sIndpZGdldHMiOlt7ImlkIjoiZ3JwX3Rlc3QiLCJ0IjoiZ3Jv"
  "dXAiLCJsYWJlbCI6IlNPVU5EIiwiY29sb3IiOiIjZmY1YzhhIiwieCI6NTYsInkiOjQyLCJ3"
  "Ijo0NTgsImgiOjMxMiwiY2hpbGRyZW4iOlsiYnRuX2hvcm4iLCJidG5fYnV6eiIsInNvdW5k"
  "X2FsZXJ0IiwibGV2ZWwiXX0seyJpZCI6ImJ0bl9ob3JuIiwidCI6ImJ1dHRvbiIsIngiOjgw"
  "LCJ5IjoxMDAsInciOjEyMCwiaCI6MTIwLCJsYWJlbCI6Ikhvcm4iLCJtb2RlbCI6Im5lbyIs"
  "Imdyb3VwSWQiOiJncnBfdGVzdCJ9LHsiaWQiOiJidG5fYnV6eiIsInQiOiJidXR0b24iLCJ4"
  "IjoyMzAsInkiOjEwMCwidyI6MTIwLCJoIjoxMjAsImxhYmVsIjoiQnV6eiIsIm1vZGVsIjoi"
  "bmVvIiwiZ3JvdXBJZCI6ImdycF90ZXN0In0seyJpZCI6InNvdW5kX2FsZXJ0IiwidCI6InNv"
  "dW5kIiwieCI6MzgwLCJ5IjoxMDUsInciOjExMCwiaCI6MTEwLCJsYWJlbCI6IkFsZXJ0Iiwi"
  "Z3JvdXBJZCI6ImdycF90ZXN0In0seyJpZCI6ImxldmVsIiwidCI6InNlbGVjdCIsIngiOjgw"
  "LCJ5IjoyNjAsInciOjIwMCwiaCI6NzAsImxhYmVsIjoiVGVzdCIsIm9wdGlvbnMiOiJCZWdp"
  "bm5lcixFeHBlcnQsTW90b3JzLERpc3RhbmNlLExpZ2h0cyxTb3VuZCxEaXNwbGF5LFBvd2Vy"
  "IiwiZ3JvdXBJZCI6ImdycF90ZXN0In0seyJpZCI6ImxibF9oaW50IiwidCI6ImxhYmVsIiwi"
  "eCI6ODAsInkiOjM3MCwidyI6NDEwLCJoIjo2MCwibGFiZWwiOiJFYWNoIGJ1dHRvbiBtYWtl"
  "cyBhIGRpZmZlcmVudCBzb3VuZC4iLCJ2YWx1ZSI6IkVhY2ggYnV0dG9uIG1ha2VzIGEgZGlm"
  "ZmVyZW50IHNvdW5kLiIsIm1vZGVsIjoiY2FyZCJ9XX0=";

static const char* LAYOUT_CFG_TEST_DISPLAY_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6ImIzIC0gRGlzcGxheSB0ZXN0IiwiY2FudmFz"
  "Ijp7InciOjc5MCwiaCI6NDQ2fSwid2lkZ2V0cyI6W3siaWQiOiJncnBfdGVzdCIsInQiOiJn"
  "cm91cCIsImxhYmVsIjoiRElTUExBWSIsImNvbG9yIjoiIzAwZTVmZiIsIngiOjU2LCJ5Ijo0"
  "MiwidyI6Njc4LCJoIjoyNzIsImNoaWxkcmVuIjpbIm9sZWRfdGV4dCIsImxibF9vbGVkIiwi"
  "bGV2ZWwiXX0seyJpZCI6Im9sZWRfdGV4dCIsInQiOiJlZGl0ZmllbGQiLCJ4Ijo4MCwieSI6"
  "MTAwLCJ3IjozMDAsImgiOjgwLCJsYWJlbCI6IldyaXRlIGhlcmUiLCJwbGFjZWhvbGRlciI6"
  "IlR5cGUgeW91ciBuYW1lLi4uIiwiZ3JvdXBJZCI6ImdycF90ZXN0In0seyJpZCI6ImxibF9v"
  "bGVkIiwidCI6ImxhYmVsIiwieCI6NDEwLCJ5IjoxMDUsInciOjMwMCwiaCI6NzAsImxhYmVs"
  "IjoiU2NyZWVuIHNob3dzIiwibW9kZWwiOiJjYXJkIiwiZ3JvdXBJZCI6ImdycF90ZXN0In0s"
  "eyJpZCI6ImxldmVsIiwidCI6InNlbGVjdCIsIngiOjgwLCJ5IjoyMjAsInciOjIwMCwiaCI6"
  "NzAsImxhYmVsIjoiVGVzdCIsIm9wdGlvbnMiOiJCZWdpbm5lcixFeHBlcnQsTW90b3JzLERp"
  "c3RhbmNlLExpZ2h0cyxTb3VuZCxEaXNwbGF5LFBvd2VyIiwiZ3JvdXBJZCI6ImdycF90ZXN0"
  "In0seyJpZCI6ImxibF9oaW50IiwidCI6ImxhYmVsIiwieCI6ODAsInkiOjMzMCwidyI6NjMw"
  "LCJoIjo2MCwibGFiZWwiOiJUeXBlLCB0aGVuIGxvb2sgYXQgdGhlIHJvYm90J3MgbGl0dGxl"
  "IHNjcmVlbi4iLCJ2YWx1ZSI6IlR5cGUsIHRoZW4gbG9vayBhdCB0aGUgcm9ib3QncyBsaXR0"
  "bGUgc2NyZWVuLiIsIm1vZGVsIjoiY2FyZCJ9XX0=";

static const char* LAYOUT_CFG_TEST_POWER_BASE64 =
  "eyJzY2hlbWFWZXJzaW9uIjoxLCJ0aXRsZSI6ImIzIC0gUG93ZXIgdGVzdCIsImNhbnZhcyI6"
  "eyJ3Ijo2ODAsImgiOjY1Nn0sIndpZGdldHMiOlt7ImlkIjoiZ3JwX3Rlc3QiLCJ0IjoiZ3Jv"
  "dXAiLCJsYWJlbCI6IlBPV0VSIiwiY29sb3IiOiIjM2RkYzk3IiwieCI6NTYsInkiOjQyLCJ3"
  "Ijo1NjgsImgiOjQ4MiwiY2hpbGRyZW4iOlsiYmF0dGVyeV9sZXZlbCIsImxibF92YmF0Iiwi"
  "Z2F1Z2VfcnNzaSIsImJ0bl9idXp6IiwibGV2ZWwiXX0seyJpZCI6ImJhdHRlcnlfbGV2ZWwi"
  "LCJ0IjoiYmF0dGVyeSIsIngiOjgwLCJ5IjoxMDAsInciOjkwLCJoIjoxMjAsImxhYmVsIjoi"
  "QmF0dGVyeSIsIm1vZGVsIjoidmVydGljYWwiLCJncm91cElkIjoiZ3JwX3Rlc3QifSx7Imlk"
  "IjoibGJsX3ZiYXQiLCJ0IjoibGFiZWwiLCJ4IjoyMDAsInkiOjEyMCwidyI6MjIwLCJoIjo2"
  "MCwibGFiZWwiOiJWb2x0cyIsIm1vZGVsIjoiY2FyZCIsImdyb3VwSWQiOiJncnBfdGVzdCJ9"
  "LHsiaWQiOiJnYXVnZV9yc3NpIiwidCI6ImdhdWdlIiwieCI6NDUwLCJ5IjoxMDAsInciOjE1"
  "MCwiaCI6MTkwLCJsYWJlbCI6IlNpZ25hbCIsIm1pbiI6LTEwMCwibWF4IjotMzAsInVuaXRz"
  "IjoiZEJtIiwiZGVjaW1hbHMiOjAsImdyb3VwSWQiOiJncnBfdGVzdCJ9LHsiaWQiOiJidG5f"
  "YnV6eiIsInQiOiJidXR0b24iLCJ4Ijo4MCwieSI6MjcwLCJ3IjoxMjAsImgiOjEyMCwibGFi"
  "ZWwiOiJCdXp6IiwibW9kZWwiOiJuZW8iLCJncm91cElkIjoiZ3JwX3Rlc3QifSx7ImlkIjoi"
  "bGV2ZWwiLCJ0Ijoic2VsZWN0IiwieCI6ODAsInkiOjQzMCwidyI6MjAwLCJoIjo3MCwibGFi"
  "ZWwiOiJUZXN0Iiwib3B0aW9ucyI6IkJlZ2lubmVyLEV4cGVydCxNb3RvcnMsRGlzdGFuY2Us"
  "TGlnaHRzLFNvdW5kLERpc3BsYXksUG93ZXIiLCJncm91cElkIjoiZ3JwX3Rlc3QifSx7Imlk"
  "IjoibGJsX2hpbnQiLCJ0IjoibGFiZWwiLCJ4Ijo4MCwieSI6NTQwLCJ3Ijo1MjAsImgiOjYw"
  "LCJsYWJlbCI6IkJ1enogdXNlcyB0aGUgc2FtZSBwaW4gYXMgdGhlIGJhdHRlcnkgc2Vuc29y"
  "IC0tIHdhdGNoIHRoZSB2b2x0cy4iLCJ2YWx1ZSI6IkJ1enogdXNlcyB0aGUgc2FtZSBwaW4g"
  "YXMgdGhlIGJhdHRlcnkgc2Vuc29yIC0tIHdhdGNoIHRoZSB2b2x0cy4iLCJtb2RlbCI6ImNh"
  "cmQifV19";

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
  if (s_layout_level >= LAYOUT_COUNT) s_layout_level = LAYOUT_BEGINNER;
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
static String   s_lastOled        = "";
static int8_t   s_lastLedR        = -1;
static int8_t   s_lastLedG        = -1;   // impossible first value -> always sends once


// ---------------------------------------------------------------------------
// Which blob each mode serves, and which telemetry ids that panel contains.
//
// The old code gated telemetry on "is this the expert layout", which stopped
// working the moment there was more than one small panel: a Sound panel has no
// gauges at all, and publishing to widget ids the active layout does not
// contain is a notification into nothing. One bit per readout keeps each
// sender honest about where its value is actually displayed.
// ---------------------------------------------------------------------------
#define T_DIST   (1u<<0)
#define T_SPEED  (1u<<1)
#define T_BATT   (1u<<2)
#define T_GRAPH  (1u<<3)
#define T_ALERT  (1u<<4)
#define T_VER    (1u<<5)
#define T_UPTIME (1u<<6)
#define T_VBAT   (1u<<7)
#define T_RSSI   (1u<<8)
#define T_BTN    (1u<<9)
#define T_LEDS   (1u<<10)
#define T_OLED   (1u<<11)
#define T_UPDSEL (1u<<12)

static const char* const LAYOUT_BLOBS[LAYOUT_COUNT] = {
  LAYOUT_CFG_BEGINNER_BASE64,
  LAYOUT_CFG_EXPERT_BASE64,
  LAYOUT_CFG_TEST_MOTORS_BASE64,
  LAYOUT_CFG_TEST_DISTANCE_BASE64,
  LAYOUT_CFG_TEST_LIGHTS_BASE64,
  LAYOUT_CFG_TEST_SOUND_BASE64,
  LAYOUT_CFG_TEST_DISPLAY_BASE64,
  LAYOUT_CFG_TEST_POWER_BASE64,
};

static const uint16_t LAYOUT_TELEM[LAYOUT_COUNT] = {
  /* Beginner */ T_DIST | T_SPEED | T_BATT,
  /* Expert   */ 0xFFFF,
  /* Motors   */ T_SPEED,
  /* Distance */ T_DIST | T_GRAPH | T_ALERT,
  /* Lights   */ T_LEDS,
  /* Sound    */ 0,
  /* Display  */ T_OLED,
  /* Power    */ T_BATT | T_VBAT | T_RSSI,
};

// Names shown in the Level select. Must match the enum order and the options
// string baked into every layout's `level` widget.
static const char* const LEVEL_NAMES[LAYOUT_COUNT] = {
  "Beginner", "Expert", "Motors", "Distance",
  "Lights", "Sound", "Display", "Power"
};

static inline bool modeHas(uint16_t bit) {
  return (LAYOUT_TELEM[s_layout_level] & bit) != 0;
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
  const char* p  = LAYOUT_BLOBS[s_layout_level];
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
    uint8_t want = LAYOUT_BEGINNER;
    for (uint8_t i = 0; i < LAYOUT_COUNT; ++i) {
      if (val == LEVEL_NAMES[i]) { want = i; break; }
    }
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
                LEVEL_NAMES[s_layout_level]);
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
  if (!modeHas(T_DIST)) return;
  if (s_upd_level == UPD_OFF) return;
  if (!telemChanged(p_onlineGraph_01_distance, s_lastDist, 1.0f)) return;
  sendValue("gauge_distance", String(p_onlineGraph_01_distance, 0));
}

void remotexy_set_onlineGraph_02_speed( float p_onlineGraph_02_speed) {
  if (!modeHas(T_SPEED)) return;
  if (s_upd_level == UPD_OFF) return;
  if (!telemChanged(p_onlineGraph_02_speed, s_lastSpeed, 1.0f)) return;
  sendValue("gauge_speed", String(p_onlineGraph_02_speed, 0));
}

void remotexy_set_onlineGraph_03_battery( float p_onlineGraph_03_battery) {
  if (!modeHas(T_BATT)) return;
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
  if (!modeHas(T_GRAPH)) return;   // graph_dist is not on this panel
  if (s_upd_level != UPD_ALL) return;
  sendValue("graph_dist", String(cm, 0));
}

// Edge-triggered, with hysteresis: the app raises a toast per message, so
// this fires once on entering the obstacle band and re-arms only after the
// robot has backed well clear of it.
void remotexy_send_obstacle_alert(float cm) {
  if (!modeHas(T_ALERT)) return;   // alert is not on this panel
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
  if (!modeHas(T_VER | T_UPTIME | T_VBAT)) return;   // no labels on this panel
  if (s_upd_level != UPD_ALL) return;

  // The version string never changes at runtime — one send per CFG is enough.
  if (modeHas(T_VER) && (s_telemForce || !s_verSent)) {
    sendValue("lbl_ver", B3_FIRMWARE_VERSION);
    s_verSent = true;
  }

  // Uptime is only ever displayed to the second, so only send when the second
  // actually rolls over rather than on every telemetry cycle.
  const uint32_t upSec = g_elapsed_time_hours * 3600u
                       + g_elapsed_time_minutes * 60u
                       + g_elapsed_time_seconds;
  if (modeHas(T_UPTIME) && (s_telemForce || upSec != s_lastUptimeSec)) {
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
  if (modeHas(T_VBAT) && telemChanged(g_battery_voltage, s_lastVbat, 0.05f)) {
    sendValue("lbl_vbat", String(g_battery_voltage, 2) + "V");
  }
}

// Board push button, mirrored as an indicator. On change only — it is the only
// physical input the robot has and it spends nearly all of its time released.
void remotexy_send_button_state(void) {
  if (!modeHas(T_BTN)) return;   // led_button is not on this panel
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
  if (!modeHas(T_RSSI)) return;   // gauge_rssi is not on this panel
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
  if (!modeHas(T_OLED)) return;   // lbl_oled is not on this panel
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

// Physical state of the two board LEDs. Sent on change; because these report
// intent rather than blink phase they are steady, so a parked robot costs
// nothing here even while the status indicator is actively flashing.
void remotexy_send_led_state(void) {
  if (!modeHas(T_LEDS)) return;   // led_r/g_state is not on this panel
  if (s_upd_level == UPD_OFF) return;
  const int8_t r = leds_state_r() ? 1 : 0;
  const int8_t g = leds_state_g() ? 1 : 0;
  if (s_telemForce || r != s_lastLedR) { s_lastLedR = r; sendValue("led_r_state", r ? "1" : "0"); }
  if (s_telemForce || g != s_lastLedG) { s_lastLedG = g; sendValue("led_g_state", g ? "1" : "0"); }
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
  sendValue("level", LEVEL_NAMES[s_layout_level]);
  // `upd` only exists in the expert layout.
  if (!modeHas(T_UPDSEL)) return;
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
