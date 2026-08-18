/**
 * TEST 1: BOARD LEDS — flash this alone.
 *
 * No Bluetooth, no app, no other files. That is the point: it answers
 * "is this board powered and am I flashing the thing I think I am?"
 * before anything else is believed. Every later test assumes this passed.
 *
 * PASS: red, green, both, none — repeating once a second.
 * FAIL: nothing ever lights  -> wrong board selected, or no power
 *       only one ever lights -> that pin or LED is dead
 *       both always on       -> pins shorted, or wrong pin numbers below
 */
#define PIN_LED_RED   10   // CONFIG_PIN_LED_RED in 01_app/01_src/00_config.h
#define PIN_LED_GREEN  1   // CONFIG_PIN_LED_GREEN

static void show(const char* name, int r, int g) {
  digitalWrite(PIN_LED_RED, r);
  digitalWrite(PIN_LED_GREEN, g);
  Serial.println(name);
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  Serial.println("\n[TEST] board LEDs - expect red, green, both, none");
}

void loop() {
  show("red",   HIGH, LOW);
  show("green", LOW,  HIGH);
  show("both",  HIGH, HIGH);
  show("none",  LOW,  LOW);
}
