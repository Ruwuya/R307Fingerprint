#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define LED_PIN 2

// R307 UART — kept off GPIO1/3 (UART0) since that's shared with USB/Serial.
// Only VCC/GND/TX/RX are used here; T-OUT and T3V3 (touch-wake) are unused.
#define FP_RX_PIN 16 // ESP32 RX2 <- sensor TX
#define FP_TX_PIN 17 // ESP32 TX2 -> sensor RX
#define FP_BAUD 57600

#define DB_PATH "/fingerprints.json"

Adafruit_Fingerprint finger(&Serial2);

unsigned long lastHeartbeat = 0;
bool ledState = false;

unsigned long lastAliveLog = 0;
const unsigned long ALIVE_LOG_INTERVAL = 15000;

JsonDocument loadDatabase()
{
  JsonDocument doc;
  File f = LittleFS.open(DB_PATH, "r");
  if (f)
  {
    deserializeJson(doc, f);
    f.close();
  }
  return doc;
}

void saveDatabase(JsonDocument &doc)
{
  File f = LittleFS.open(DB_PATH, "w");
  serializeJson(doc, f);
  f.close();
}

void recordEnrollment(uint16_t id)
{
  JsonDocument doc = loadDatabase();
  doc[String(id)]["timestamp"] = millis();
  saveDatabase(doc);
}

bool lookupRecord(uint16_t id, unsigned long &timestampOut)
{
  JsonDocument doc = loadDatabase();
  String key = String(id);
  if (!doc[key].isNull())
  {
    timestampOut = doc[key]["timestamp"].as<unsigned long>();
    return true;
  }
  return false;
}

uint8_t enrollFingerprint(uint16_t id)
{
  int p = -1;

  Serial.println("Place finger on sensor...");
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    if (p == FINGERPRINT_OK)
    {
      Serial.println("Image taken");
    }
    else if (p != FINGERPRINT_NOFINGER)
    {
      Serial.printf("Error capturing image (code %d)\n", p);
      return p;
    }
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK)
  {
    Serial.println("Failed to process image");
    return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }

  Serial.println("Place same finger again...");
  p = -1;
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    if (p == FINGERPRINT_OK)
    {
      Serial.println("Image taken");
    }
    else if (p != FINGERPRINT_NOFINGER)
    {
      Serial.println("Error capturing image");
      return p;
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK)
  {
    Serial.println("Failed to process second image");
    return p;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK)
  {
    Serial.println("Prints did not match");
    return p;
  }

  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK)
  {
    Serial.println("Failed to store model");
    return p;
  }

  Serial.printf("Enrolled fingerprint as ID #%d\n", id);
  recordEnrollment(id);
  return FINGERPRINT_OK;
}

void checkForScan()
{
  int p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER)
  {
    return; // idle, nothing on the sensor — stay quiet
  }
  if (p != FINGERPRINT_OK)
  {
    return; // transient read error
  }

  Serial.println("Scanning fingerprint...");

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
  {
    Serial.println("Could not process image");
    Serial.println("Scan complete");
    return;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK)
  {
    unsigned long ts;
    if (lookupRecord(finger.fingerID, ts))
    {
      Serial.printf("Match! ID #%d  confidence: %d  enrolled at: %lu ms uptime\n",
                     finger.fingerID, finger.confidence, ts);
    }
    else
    {
      Serial.printf("Match! ID #%d  confidence: %d  (no JSON record found)\n",
                     finger.fingerID, finger.confidence);
    }
  }
  else
  {
    Serial.println("No match found");
  }

  Serial.println("Scan complete");
}

void handleSerialCommands()
{
  if (!Serial.available())
  {
    return;
  }
  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line.startsWith("e "))
  {
    int id = line.substring(2).toInt();
    if (id > 0 && id <= 200)
    {
      enrollFingerprint((uint16_t)id);
    }
    else
    {
      Serial.println("Usage: e <id 1-200>");
    }
  }
  else if (line == "?")
  {
    Serial.println("Commands: 'e <id>' to enroll. Otherwise, just place a finger to scan.");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP Alive!");

  pinMode(LED_PIN, OUTPUT);

  Serial2.begin(FP_BAUD, SERIAL_8N1, FP_RX_PIN, FP_TX_PIN);
  finger.begin(FP_BAUD);
  if (finger.verifyPassword())
  {
    Serial.println("Fingerprint sensor found!");
  }
  else
  {
    Serial.println("Fingerprint sensor NOT found - check wiring");
  }

  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS mount failed");
  }

  Serial.println("Send 'e <id>' to enroll a new fingerprint (id 1-200).");
  Serial.println("Otherwise, just place a finger on the sensor to scan.");
}

void loop()
{
  unsigned long now = millis();

  if (now - lastHeartbeat >= 500)
  {
    lastHeartbeat = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }

  if (now - lastAliveLog >= ALIVE_LOG_INTERVAL)
  {
    lastAliveLog = now;
    Serial.println("ESP is living!");
  }

  handleSerialCommands();
  checkForScan();
}
