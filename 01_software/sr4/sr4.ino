// Pin assignments for ESP32-C3
#define TRIG_PIN 21
#define ECHO_PIN 20

#define PIN_BUZZER 4

long duration;
float distance_cm;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT_PULLUP);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

}

void loop() {
  // Ensure trigger pin is LOW before starting
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Send a 10 microsecond HIGH pulse to trigger the ultrasonic burst
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(8);
  digitalWrite(TRIG_PIN, LOW);

  // Measure the duration of the echo pulse
  duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms to avoid lockups

  // Calculate distance in cm
  distance_cm = duration * 0.0343 / 2;

  // Display result
  if (duration == 0) {
    //Serial.println("No echo received (out of range)");
  } else {
    Serial.print("Distance: ");
    Serial.print(distance_cm);
    Serial.println(" cm");
  }
return;
  delay(500); // wait before next reading
}
