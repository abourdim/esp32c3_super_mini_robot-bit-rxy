#define TRIG_PIN 21
#define ECHO_PIN 20

enum SR04State { IDLE, TRIGGER_HIGH, WAIT_FOR_ECHO_HIGH, WAIT_FOR_ECHO_LOW };
SR04State state = IDLE;

unsigned long stateStart = 0;
unsigned long echoStart = 0;
float lastDistance = 0;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void loop() {
  unsigned long now = micros();

  switch (state) {
    case IDLE:
      // Start trigger pulse every 100ms
      if (millis() - stateStart >= 100) {
        digitalWrite(TRIG_PIN, HIGH);
        stateStart = now;
        state = TRIGGER_HIGH;
      }
      break;

    case TRIGGER_HIGH:
      if (now - stateStart >= 10) { // 10 µs HIGH
        digitalWrite(TRIG_PIN, LOW);
        state = WAIT_FOR_ECHO_HIGH;
      }
      break;

    case WAIT_FOR_ECHO_HIGH:
      if (digitalRead(ECHO_PIN) == HIGH) {
        echoStart = micros();
        state = WAIT_FOR_ECHO_LOW;
      }
      // Timeout after 30ms (no echo)
      else if (micros() - stateStart > 30000) {
        lastDistance = 0;
        Serial.println("No echo");
        state = IDLE;
        stateStart = millis();
      }
      break;

    case WAIT_FOR_ECHO_LOW:
      if (digitalRead(ECHO_PIN) == LOW) {
        unsigned long echoEnd = micros();
        unsigned long duration = echoEnd - echoStart;
        lastDistance = duration * 0.0343 / 2; // cm
        Serial.print("Distance: ");
        Serial.print(lastDistance);
        Serial.println(" cm");
        state = IDLE;
        stateStart = millis();
      }
      break;
  }

  // Other non-blocking code can run here
}
