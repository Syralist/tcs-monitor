const int analogIn = A0;

int sensorValue = 0;
float voltageDivider = 0;
float actualVoltage = 0;

void setup() {
    // initialize serial communication at 115200
    Serial.begin(115200);
}

void loop() {
    sensorValue = analogRead(analogIn);
    voltageDivider = map(sensorValue, 0, 1023, 0, 320) / 100.0;
    actualVoltage = voltageDivider * 24.0 / 2.2;
    Serial.print("Digits: ");
    Serial.print(sensorValue);
    Serial.print(", Divider: ");
    Serial.print(voltageDivider);
    Serial.print(", Actual: ");
    Serial.println(actualVoltage);
    delay(500);
}