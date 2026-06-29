#include <DHT.h>

// ---------------- PIN CONFIG ----------------
#define DHTPIN PA4        // Correct STM32 pin naming!
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------------- UART CONFIG ----------------
// EC200S → Serial3 (PB11 = RX, PB10 = TX)
HardwareSerial Serial3(PB11, PB10);   //C (RX, TX)

// ---------------- HELPERS ----------------
bool waitFor(const String &match, uint32_t timeout) {
  unsigned long start = millis();
  String buf = "";
  while (millis() - start < timeout) {
    while (Serial3.available()) {
      char c = Serial3.read();
      buf += c;
      Serial.write(c);  
      if (buf.indexOf(match) != -1) return true;
    }
  }
  return false;
}

bool sendAT(const String &cmd, const String &match, uint32_t timeout) {
  Serial.print(">> ");
  Serial.println(cmd);
  Serial3.println(cmd);
  return waitFor(match, timeout);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);     // Debug to FTDI (PA9/PA10)
  Serial3.begin(115200);    // EC200S (PB11/PB10)
  dht.begin();

  delay(1500);
  Serial.println("\nSTM32 + EC200S + DHT11 + ThingSpeak BEGIN");

  // BASIC CHECKS
  sendAT("AT", "OK", 1000);
  sendAT("AT+CPIN?", "READY", 2000);
  sendAT("AT+CGATT?", "1", 3000);
  sendAT("AT+CSQ", "OK", 2000);
  sendAT("AT+CREG?", "1", 3000);

  // APN setup
  sendAT("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1", "OK", 3000);

  // Restart PDP context
  sendAT("AT+QIDEACT=1", "OK", 4000);
  sendAT("AT+QIACT=1", "OK", 8000);
  sendAT("AT+QIACT?", "+QIACT:", 5000);
}

// ---------------- LOOP (MAIN TASK) ----------------
void loop() {

  // Read DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("!! DHT READ FAILED");
    delay(3000);
    return;
  }

  Serial.print("Temp: "); Serial.print(t);
  Serial.print("  Humidity: "); Serial.println(h);


  // ---- STEP 1: SET URL ----
  Serial3.println("AT+QHTTPURL=32,80");
  if (!waitFor("CONNECT", 8000)) {
    Serial.println("!! URL ERROR");
    delay(20000);
    return;
  }

  Serial3.print("http://api.thingspeak.com/update");
  waitFor("OK", 3000);


  // ---- STEP 2: SEND POST ----
  String body =
    String("api_key=W1UHOLI0M4EF1939") +
    "&field1=" + String(t, 1) +
    "&field2=" + String(h, 1);

  String cmd = "AT+QHTTPPOST=" + String(body.length()) + ",30,60";
  Serial3.println(cmd);

  if (!waitFor("CONNECT", 8000)) {
    Serial.println("!! POST CONNECT FAIL");
    delay(20000);
    return;
  }

  Serial3.print(body);

  if (waitFor("+QHTTPPOST:", 15000)) {
    Serial.println("POST OK!");
  } else {
    Serial.println("POST FAIL");
  }

  delay(20000);  // 20 seconds (ThingSpeak limit)
}
