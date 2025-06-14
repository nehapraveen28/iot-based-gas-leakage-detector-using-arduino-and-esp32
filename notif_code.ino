#define BLYNK_TEMPLATE_ID "TMPL3gdYIkouO"
#define BLYNK_TEMPLATE_NAME "esp32 gas"
#define BLYNK_AUTH_TOKEN "_56EHh79zybYJxiimMhmvMDUxQnbzTDm"

#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>

// ------------------- WiFi Credentials -------------------
const char* ssid = "Neha";
const char* pass = "zyxwabcd";

// ------------------- Blynk Auth Token -------------------
char auth[] = "_56EHh79zybYJxiimMhmvMDUxQnbzTDm";
 
// ------------------- Email SMTP Credentials -------------------
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "nidhi.harish0726@gmail.com"
#define AUTHOR_PASSWORD "zhud gimv oukp aftm"
#define RECIPIENT_EMAIL "nehapraveen2810@gmail.com"

// ------------------- Telegram Bot Credentials -------------------
const char* telegramToken = "7606395781:AAHhnHZToIcnw9y8CTwtrKhguyp9z8orNcI";
const char* chatID = "1210949323";

// ------------------- Hardware Pins -------------------
#define MQ5_PIN 34
#define BUZZER_PIN 25
#define LED_PIN 26
#define SERVO_PIN 23   // ✅ Set to the correct GPIO pin used for servo

// ------------------- Variables -------------------
int gasThreshold = 500;
bool leakReported = false;
bool valveControlManual = false;

// ------------------- LCD and Servo -------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Use correct I2C address
Servo gasServo;
SMTPSession smtp;

// ------------------- EMAIL ALERT -------------------
void sendEmailAlert() {
  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;

  SMTP_Message message;
  message.sender.name = "ESP32 Gas Sensor";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "Gas Leak Detected!";
  message.addRecipient("Owner", RECIPIENT_EMAIL);
  message.text.content = "Warning: Gas leak detected. Please check your premises.";
  message.text.charSet = "utf-8";

  if (!smtp.connect(&session)) {
    Serial.println("SMTP connect failed: " + smtp.errorReason());
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Email send failed: " + smtp.errorReason());
  } else {
    Serial.println("Email sent.");
  }

  smtp.closeSession();
}

// ------------------- TELEGRAM ALERT -------------------
void sendTelegramAlert(const char* msg) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(telegramToken)
               + "/sendMessage?chat_id=" + String(chatID)
               + "&text=" + String(msg);
    http.begin(url);
    int code = http.GET();
    if (code > 0) {
      Serial.println("Telegram alert sent.");
    } else {
      Serial.println("Telegram failed.");
    }
    http.end();
  }
}

// ------------------- BLYNK TOGGLE (V0) -------------------
BLYNK_WRITE(V0) {
  int pinValue = param.asInt();
  valveControlManual = (pinValue == 1);  // true if ON

  if (valveControlManual) {
    gasServo.write(120);  // Open valve
    Serial.println("Valve opened via Blynk.");
  } else {
    gasServo.write(0);    // Close valve
    Serial.println("Valve closed via Blynk.");
  }
}

// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // SDA = 21, SCL = 22 for ESP32

  lcd.init();
  lcd.backlight();

  pinMode(MQ5_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  gasServo.attach(SERVO_PIN, 500, 2400);  // Typical servo pulse width range
  gasServo.write(0);  // Initial position

  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  Blynk.begin(auth, ssid, pass);  // Start Blynk

  smtp.callback([](SMTP_Status status) {
    Serial.println(status.info());
  });
}

// ------------------- LOOP -------------------
void loop() {
  Blynk.run();

  int gasVal = analogRead(MQ5_PIN);
  Serial.print("Gas: ");
  Serial.println(gasVal);

  lcd.setCursor(0, 0);
  lcd.print("Gas: ");
  lcd.print(gasVal);
  lcd.print("    ");

  if (gasVal > gasThreshold) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);

    if (!valveControlManual) {
      gasServo.write(120);
    }

    lcd.setCursor(0, 81);
    lcd.print("GAS LEAK DETECT");

    if (!leakReported) {
      sendEmailAlert();
      sendTelegramAlert("🚨 Gas leak detected by ESP32! Please check immediately.");
      leakReported = true;
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    if (!valveControlManual) {
      gasServo.write(0);
    }

    lcd.setCursor(0, 1);
    lcd.print("Safe            ");
    leakReported = false;
  }

  delay(500);
}
