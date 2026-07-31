#include <Ultrasonic.h>

// Trigger pin = 21, Echo pin = 20
Ultrasonic ultrasonic(21, 20);

void setup() {
  Serial.begin(115200);
}

void loop() {
  float distance = ultrasonic.read(); // distance in cm

  if (distance == 0) {
    Serial.println("No echo (out of range)");
  } else {
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }

  delay(500);
}
