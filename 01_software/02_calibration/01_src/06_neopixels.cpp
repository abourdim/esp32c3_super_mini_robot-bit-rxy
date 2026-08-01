#include "01_includes.h"

CRGB leds[CONFIG_NEOPIXELS_MAX_LEDS];    // LED array (max size, only NB_LEDS used)

// ===========================================================================
void neopixels_init() {
// ===========================================================================  
  FastLED.addLeds<LED_TYPE, CONFIG_PIN_NEOPIXEL, COLOR_ORDER>(leds, CONFIG_NEOPIXELS_NB_LEDS);
  FastLED.setBrightness(CONFIG_NEOPIXELS_BRIGHTNESS);  // Set brightness (0-255)
  #ifdef DEF_DERIAL_DEBUG
  Serial.println("FastLED NeoPixel example started");
  #endif
}

// ===========================================================================
void neopixels_rainbow(uint8_t ledIndex) {
// ===========================================================================
  static uint8_t hue = 0;
  leds[ledIndex] = CHSV(hue++, 255, 255);
  if(hue >= 255) hue = 0;
}

// ===========================================================================
void neopixels_pulse(uint8_t ledIndex, CRGB color) {
// ===========================================================================
  static uint8_t brightness = 0;
  static bool increasing = true;
  
  if(increasing) {
    brightness++;
    if(brightness >= 255) increasing = false;
  } else {
    brightness--;
    if(brightness <= 1) increasing = true;
  }
  
  leds[ledIndex] = color;
  leds[ledIndex].fadeLightBy(255 - brightness);
}

// ===========================================================================
void neopixels_all_clear(CRGB color) {
// ===========================================================================
  fill_solid(leds, CONFIG_NEOPIXELS_NB_LEDS, color);
  FastLED.show();
}

// ===========================================================================
void neopixels_all_blink(CRGB color, uint8_t blinks, uint16_t delayTime) {
// ===========================================================================
  for(uint8_t i=0; i<blinks; i++) {
    neopixels_all_clear(color);
    FastLED.delay(delayTime);
    neopixels_all_clear(CRGB::Black);
    FastLED.delay(delayTime);
   
  }
}


void neopixels_KnightRiderRedEyewithTail() {
  static int pos = 0;        // Current LED position
  static int dir = 1;        // Direction: 1 = forward, -1 = backward

  static unsigned long previousMillis = 0;
  const int interval = 30; // speed in milliseconds 
  
unsigned long currentMillis = millis();

  // Only update animation every `interval` milliseconds
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 40);
    leds[pos] = CRGB::Blue;

    FastLED.show();

    pos += dir;
    if (pos == CONFIG_NEOPIXELS_NB_LEDS - 1 || pos == 0) {
      dir = -dir;
    }
  }
}


// Animation
int pos = 0;
int dir = 1;
unsigned long animMillis = 0;
const int animInterval = 30;

// Color cycling
CRGB colors[] = {CRGB::Red, CRGB::Blue, CRGB::White};
int currentColorIndex = 0;
unsigned long colorMillis = 0;
const int colorInterval = 10000; // Change color every 2 seconds


// Requires >= 32 LEDs (uses indices 0..15 + 16..31)
#if CONFIG_NEOPIXELS_NB_LEDS >= 32
void neopixels_KnightRiderRedEyewithTail2() {
 unsigned long now = millis();

  // --- Color cycling logic ---
  if (now - colorMillis >= colorInterval) {
    colorMillis = now;
    currentColorIndex = (currentColorIndex + 1) % 3; // Cycle through 0, 1, 2
  }

  // --- Symmetrical eye animation ---
  if (now - animMillis >= animInterval) {
    animMillis = now;

    fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 40);

    int leftIndex = 15 - pos;
    int rightIndex = 16 + pos;

    CRGB currentColor = colors[currentColorIndex];
    leds[leftIndex]  = currentColor;
    leds[rightIndex] = currentColor;

    pos += dir;
    if (pos >= 15 || pos <= 0) dir = -dir;

    FastLED.show();
  }
}
#else
void neopixels_KnightRiderRedEyewithTail2() { /* needs >= 32 LEDs */ }
#endif







// Requires >= 32 LEDs (two eyes of 16 LEDs each)
#if CONFIG_NEOPIXELS_NB_LEDS >= 32
void neopixels_duel_eye() {

  // Eye 1 (Left)
static int pos1 = 0;
static int dir1 = 1;
static unsigned long prevMillis1 = 0;
static const int interval1 = 30;

// Eye 2 (Right)
static int pos2 = 0;
static int dir2 = 1;
static unsigned long prevMillis2 = 0;
static const int interval2 = 40;

  unsigned long currentMillis = millis();

  // Fade all LEDs (shared)
  fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 40);

  // Left Eye Animation (LEDs 0–15)
  if (currentMillis - prevMillis1 >= interval1) {
    prevMillis1 = currentMillis;
    leds[pos1] = CRGB::Red;
    pos1 += dir1;
    if (pos1 == 15 || pos1 == 0) dir1 = -dir1;
  }

  // Right Eye Animation (LEDs 16–31)
  if (currentMillis - prevMillis2 >= interval2) {
    prevMillis2 = currentMillis;
    leds[16 + pos2] = CRGB::Blue;  // You can also use Red for both
    pos2 += dir2;
    if (pos2 == 15 || pos2 == 0) dir2 = -dir2;
  }

  FastLED.show();

  // Do other robot stuff here!
}
#else
void neopixels_duel_eye() { /* needs >= 32 LEDs */ }
#endif



// ===========================================================================
void neopixels_demo_legacy() {
// ===========================================================================
  // LED 0: Rainbow cycle
  neopixels_rainbow(0);
  
  // LED 1: Pulsing red
  neopixels_pulse(1, CRGB::Red);
  
  FastLED.show();
  FastLED.delay(20);  // Control speed
}


// Modes: "left", "right", "forward", "stop"
String mode = "left"; // change this to test

unsigned long prevMillis = 0;
int step = 0;

// === Helper: Fill segment progressively ===
void fillSegment(int start, int length, int stepCount, CRGB color) {
  for (int i = 0; i < stepCount && i < length; i++) {
    leds[start + i] = color;
  }
}

// === Animations ===
// These animations assume a 32-LED two-eye layout (16+16).
// Guarded — only compile when the strip is large enough.
#if CONFIG_NEOPIXELS_NB_LEDS >= 32

void animateLeft() {
  fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 60);
  fillSegment(0, 16, step, CRGB::Orange);  // Left eye sweep
  step++;
  if (step > 16) step = 0;
}

void animateRight() {
  fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 60);
  fillSegment(16, 16, step, CRGB::Orange);  // Right eye sweep
  step++;
  if (step > 16) step = 0;
}

void animateForward() {
  fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 80);
  if (step % 2 == 0) {
    leds[7] = CRGB::Green;
    leds[24] = CRGB::Green;
  }
  step++;
  if (step > 10) step = 0;
}

void animateStop() {
  fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 80);
  if (step % 2 == 0) {
    for (int i = 0; i < CONFIG_NEOPIXELS_NB_LEDS; i++) leds[i] = CRGB::Red;
  }
  step++;
  if (step > 10) step = 0;
}

void neopixels_animate() {
  unsigned long now = millis();

  if (now - prevMillis > 60) {
    prevMillis = now;

    // Pick animation based on current mode
    if (mode == "left")      animateLeft();
    else if (mode == "right") animateRight();
    else if (mode == "forward") animateForward();
    else if (mode == "stop")    animateStop();

    FastLED.show();
  }

  // Simulate robot logic here, or change mode with buttons / sensors
}

#else
// Fallback when strip < 32 LEDs — fade all to red as a stop signal
void neopixels_animate() {
  unsigned long now = millis();
  if (now - prevMillis > 60) {
    prevMillis = now;
    fill_solid(leds, CONFIG_NEOPIXELS_NB_LEDS, CRGB::Red);
    FastLED.show();
  }
}
#endif

CRGB palette[3] = { CRGB::Red, CRGB::Blue, CRGB::White };

void neopixels_star() {
  // Fade all LEDs slowly to create twinkle trails
  fadeToBlackBy(leds, CONFIG_NEOPIXELS_NB_LEDS, 20);

  // Occasionally light up a random pixel as a "star"
  if (random8() < 40) { // probability of new star
    int pos = random16(CONFIG_NEOPIXELS_NB_LEDS);
    leds[pos] = palette[random8(3)]; // red, blue, or white
  }

  FastLED.show();
  delay(80); // controls overall twinkle speed
}

