#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT11  // ganti DHT11 jika pakai DHT11
#define LEDPIN 21
#define SUHU_BATAS 30.0

const char* ssid = "Meriah Ukur";
const char* password = "bismillah";
const String base = "http://192.168.1.8/iot/api/";

DHT dht(DHTPIN, DHTTYPE);

String modeKontrol = "manual";
String perintah = "OFF";
String statusLed = "";
float suhuTerakhir = NAN;
unsigned long lastKontrol = 0;
unsigned long lastSensor = 0;

int kirimPost(const String& endpoint, const String& body) {
  HTTPClient http;
  http.begin(base + endpoint);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  http.end();
  return code;
}

void kirimStatus() {
  StaticJsonDocument<64> doc;
  doc["status"] = statusLed;
  String body;
  serializeJson(doc, body);
  int code = kirimPost("status_led.php", body);
  Serial.print("Kirim status LED ");
  Serial.print(statusLed);
  Serial.print(" -> HTTP ");
  Serial.println(code);
}

void terapkanLed() {
  bool on;
  if (modeKontrol == "otomatis") {
    if (isnan(suhuTerakhir)) return;
    on = suhuTerakhir > SUHU_BATAS;
  } else {
    on = (perintah == "ON");
  }

  digitalWrite(LEDPIN, on ? HIGH : LOW);

  String s = on ? "ON" : "OFF";
  if (s != statusLed) {
    statusLed = s;
    kirimStatus();
  }
}

void ambilKontrol() {
  HTTPClient http;
  http.begin(base + "kontrol.php");
  int code = http.GET();

  if (code == 200) {
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    if (!err) {
      modeKontrol = doc["data"]["mode"].as<String>();
      perintah = doc["data"]["perintah"].as<String>();
      Serial.print("Mode: ");
      Serial.print(modeKontrol);
      Serial.print(" | Perintah: ");
      Serial.println(perintah);
    }
  } else {
    Serial.print("Gagal ambil perintah, HTTP ");
    Serial.println(code);
  }
  http.end();
}

void kirimSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Gagal membaca sensor DHT");
    return;
  }
  suhuTerakhir = t;

  StaticJsonDocument<128> doc;
  doc["suhu"] = t;
  doc["kelembapan"] = h;
  String body;
  serializeJson(doc, body);

  int code = kirimPost("simpan.php", body);
  Serial.print("Suhu ");
  Serial.print(t);
  Serial.print(" | Kelembapan ");
  Serial.print(h);
  Serial.print(" -> HTTP ");
  Serial.println(code);
}

void setup() {
  Serial.begin(115200);
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);
  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus, mencoba ulang...");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  unsigned long now = millis();

  if (now - lastSensor >= 5000 || lastSensor == 0) {
    lastSensor = now;
    kirimSensor();
    terapkanLed();
  }

  if (now - lastKontrol >= 2000 || lastKontrol == 0) {
    lastKontrol = now;
    ambilKontrol();
    terapkanLed();
  }
}