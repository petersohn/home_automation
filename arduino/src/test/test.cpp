#include <Arduino.h>

int ledPin = 2;                 // LED connected to digital pin 13

void setup()
{
  Serial.begin(115200);
  Serial.println("START");
  pinMode(ledPin, OUTPUT);      // sets the digital pin as output
}

void loop()
{
  digitalWrite(ledPin, HIGH);   // sets the LED on
  delay(1000);                  // waits for a second
  digitalWrite(ledPin, LOW);    // sets the LED off
  delay(1000);                  // waits for a second
}
