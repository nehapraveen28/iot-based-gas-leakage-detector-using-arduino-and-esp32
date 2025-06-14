#define BLYNK_TEMPLATE_ID "your_blynk_template_id"
#define BLYNK_TEMPLATE_NAME "your_blynk_template_name"
#define BLYNK_AUTH_TOKEN "your_blynk_auth_code"

#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <ESP_Mail_Client.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>

// ------------------- WiFi Credentials -------------------
const char* ssid = "hotspot_name";
const char* pass = "hotspot_password";

// ------------------- Blynk Auth Token -------------------
char auth[] = "your_blynk_auth_token";
 
// ------------------- Email SMTP Credentials -------------------
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "sender_email"
#define AUTHOR_PASSWORD "author_password"
#define RECIPIENT_EMAIL "receiver_email"

// ------------------- Telegram Bot Credentials -------------------
const char* telegramToken = "your_telegram_token";
const char* chatID = "your_telegram_chatid";

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
