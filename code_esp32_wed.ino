#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Stepper.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// -------------------- WiFi & WebServer --------------------
const char* ssid = "VIET ANH";
const char* password = "hoanganh";
WebServer server(80);

// -------------------- Định nghĩa chân phần cứng --------------------
// Servo và Buzzer
#define SERVO_PIN       5
#define BUZZER_PIN      18

// Động cơ bước (Stepper)
#define STEPPER_PIN1    12
#define STEPPER_PIN2    13
#define STEPPER_PIN3    14
#define STEPPER_PIN4    15
const int stepsPerRevolution = 200; // số bước cho 1 vòng (tùy theo model)

// Cảm biến DHT11
#define DHT_PIN         19
#define DHT_TYPE        DHT11

// LCD I2C
#define LCD_ADDRESS     0x27
#define LCD_COLS        16
#define LCD_ROWS        2

// Đèn LED (5 đèn – ứng với màu của kẹo)
const int ledPins[5] = {25, 26, 27, 32, 33};

// Các nút bấm (giả sử khi nhấn mức điện áp là LOW)
#define BTN_MENU        34   // Chuyển chế độ
#define BTN_START_STOP  35   // Thực hiện hành động
#define BTN_UP          36   // Tăng giá trị
#define BTN_DOWN        39   // Giảm giá trị
#define BTN_RESET       23   // Reset (INPUT_PULLUP)

// -------------------- Khai báo đối tượng --------------------
Servo myServo;
Stepper stepperMotor(stepsPerRevolution, STEPPER_PIN1, STEPPER_PIN2, STEPPER_PIN3, STEPPER_PIN4);
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// -------------------- Các chế độ hoạt động --------------------
enum Mode { SENSOR, SERVO, STEPPER, LED, BUZZER };
Mode currentMode = SENSOR;

// Các biến tham số cho từng chế độ
int servoAngle = 90;        // Góc servo (0 - 180)
int stepperSteps = 50;      // Số bước động cơ bước sẽ di chuyển
int ledIndex = 0;           // Chọn LED (0 -> 4)
bool ledStates[5] = { false, false, false, false, false };

// -------------------- Debounce nút bấm --------------------
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;

// Cập nhật LCD định kỳ
unsigned long lastLCDUpdate = 0;
const unsigned long lcdUpdateInterval = 500;

// -------------------- Hàm cập nhật LCD --------------------
void updateLCD() {
  lcd.clear();
  switch(currentMode) {
    case SENSOR: {
       float temp = dht.readTemperature();
       float hum = dht.readHumidity();
       lcd.setCursor(0, 0);
       lcd.print("SENSOR MODE");
       lcd.setCursor(0, 1);
       lcd.print("T:");
       lcd.print(temp);
       lcd.print(" H:");
       lcd.print(hum);
       break;
    }
    case SERVO: {
       lcd.setCursor(0, 0);
       lcd.print("SERVO MODE");
       lcd.setCursor(0, 1);
       lcd.print("Angle:");
       lcd.print(servoAngle);
       break;
    }
    case STEPPER: {
       lcd.setCursor(0, 0);
       lcd.print("STEPPER MODE");
       lcd.setCursor(0, 1);
       lcd.print("Steps:");
       lcd.print(stepperSteps);
       break;
    }
    case LED: {
       lcd.setCursor(0, 0);
       lcd.print("LED MODE");
       lcd.setCursor(0, 1);
       lcd.print("LED:");
       lcd.print(ledIndex + 1);
       break;
    }
    case BUZZER: {
       lcd.setCursor(0, 0);
       lcd.print("BUZZER MODE");
       lcd.setCursor(0, 1);
       lcd.print("Press Start");
       break;
    }
  }
}

// -------------------- Hàm xử lý nút bấm --------------------
void checkButtons() {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPress < debounceDelay) {
    return;
  }
  
  // Nút MENU: chuyển chế độ
  if (digitalRead(BTN_MENU) == LOW) {
    currentMode = (Mode)((currentMode + 1) % 5);
    Serial.print("Chuyển chế độ: ");
    Serial.println(currentMode);
    tone(BUZZER_PIN, 1000, 100); // báo hiệu chuyển mode
    lastButtonPress = currentTime;
    updateLCD();
  }
  
  // Nút START/STOP: thực hiện hành động theo chế độ
  if (digitalRead(BTN_START_STOP) == LOW) {
    switch(currentMode) {
      case SERVO:
        myServo.write(servoAngle);
        Serial.print("Servo di chuyển đến: ");
        Serial.println(servoAngle);
        tone(BUZZER_PIN, 1000, 100);
        break;
      case STEPPER:
        Serial.print("Stepper di chuyển ");
        Serial.print(stepperSteps);
        Serial.println(" bước");
        stepperMotor.step(stepperSteps); // di chuyển số bước chỉ định
        tone(BUZZER_PIN, 1000, 100);
        break;
      case LED:
        // Toggle trạng thái LED được chọn
        ledStates[ledIndex] = !ledStates[ledIndex];
        digitalWrite(ledPins[ledIndex], ledStates[ledIndex] ? HIGH : LOW);
        Serial.print("LED ");
        Serial.print(ledIndex + 1);
        Serial.print(" ");
        Serial.println(ledStates[ledIndex] ? "BẬT" : "TẮT");
        tone(BUZZER_PIN, 1000, 100);
        break;
      case BUZZER:
        Serial.println("Buzzer phát tiếng");
        tone(BUZZER_PIN, 1000, 500);
        break;
      case SENSOR:
        // Ở SENSOR mode không có hành động cụ thể khi nhấn nút start/stop
        break;
    }
    lastButtonPress = currentTime;
    updateLCD();
  }
  
  // Nút UP: tăng giá trị tham số
  if (digitalRead(BTN_UP) == LOW) {
    switch(currentMode) {
      case SERVO:
        servoAngle = min(servoAngle + 5, 180);
        break;
      case STEPPER:
        stepperSteps += 10;
        break;
      case LED:
        ledIndex = (ledIndex + 1) % 5;
        break;
      default:
        break;
    }
    Serial.println("Tăng giá trị");
    lastButtonPress = currentTime;
    updateLCD();
  }
  
  // Nút DOWN: giảm giá trị tham số
  if (digitalRead(BTN_DOWN) == LOW) {
    switch(currentMode) {
      case SERVO:
        servoAngle = max(servoAngle - 5, 0);
        break;
      case STEPPER:
        stepperSteps = max(stepperSteps - 10, 0);
        break;
      case LED:
        ledIndex = (ledIndex - 1 + 5) % 5;
        break;
      default:
        break;
    }
    Serial.println("Giảm giá trị");
    lastButtonPress = currentTime;
    updateLCD();
  }
  
  // Nút RESET: đặt lại giá trị của chế độ hiện tại
  if (digitalRead(BTN_RESET) == LOW) {
    switch(currentMode) {
      case SERVO:
        servoAngle = 90;
        break;
      case STEPPER:
        stepperSteps = 50;
        break;
      case LED:
        ledIndex = 0;
        for (int i = 0; i < 5; i++) {
          ledStates[i] = false;
          digitalWrite(ledPins[i], LOW);
        }
        break;
      default:
        break;
    }
    Serial.println("Reset giá trị");
    lastButtonPress = currentTime;
    updateLCD();
  }
}

// -------------------- Hàm xử lý WebServer --------------------
// Hàm trả về trạng thái hệ thống dưới dạng JSON
void handleStatus() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  String json = "{";
  json += "\"mode\":\"";
  switch(currentMode) {
    case SENSOR: json += "SENSOR"; break;
    case SERVO: json += "SERVO"; break;
    case STEPPER: json += "STEPPER"; break;
    case LED: json += "LED"; break;
    case BUZZER: json += "BUZZER"; break;
  }
  json += "\",";
  json += "\"temperature\":";
  json += temp;
  json += ",";
  json += "\"humidity\":";
  json += hum;
  json += ",";
  json += "\"servoAngle\":";
  json += servoAngle;
  json += ",";
  json += "\"stepperSteps\":";
  json += stepperSteps;
  json += ",";
  json += "\"ledIndex\":";
  json += ledIndex;
  json += ",";
  json += "\"ledStates\":[";
  for (int i = 0; i < 5; i++) {
    json += ledStates[i] ? "true" : "false";
    if(i < 4) json += ",";
  }
  json += "]";
  json += "}";
  server.send(200, "application/json", json);
}

// -------------------- Hàm setup --------------------
void setup() {
  Serial.begin(115200);

  // Khởi tạo WiFi và WebServer
  WiFi.begin(ssid, password);
  Serial.print("Kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());
  
  server.on("/status", handleStatus);
  server.begin();
  
  // Khởi tạo cảm biến DHT11
  dht.begin();
  
  // Khởi tạo LCD I2C
  lcd.init();
  lcd.backlight();
  
  // Gắn servo và đặt góc ban đầu
  myServo.attach(SERVO_PIN);
  myServo.write(servoAngle);
  
  // Thiết lập tốc độ cho động cơ bước
  stepperMotor.setSpeed(60);  // 60 vòng/phút
  
  // Khởi tạo chân LED
  for (int i = 0; i < 5; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  
  // Khởi tạo chân nút bấm
  // Với các chân 34,35,36,39 (input-only) cần mạch kéo ngoài (external pull-up)
  pinMode(BTN_MENU, INPUT);
  pinMode(BTN_START_STOP, INPUT);
  pinMode(BTN_UP, INPUT);
  pinMode(BTN_DOWN, INPUT);
  pinMode(BTN_RESET, INPUT_PULLUP); // Sử dụng pull-up nội bộ cho nút reset
  
  // Hiển thị trạng thái ban đầu lên LCD
  updateLCD();
}

// -------------------- Hàm loop --------------------
void loop() {
  checkButtons();
  server.handleClient();
  
  // Cập nhật LCD định kỳ
  if (millis() - lastLCDUpdate > lcdUpdateInterval) {
    updateLCD();
    lastLCDUpdate = millis();
  }
  
  // Delay nhỏ để tránh quét nút quá nhanh
  delay(50);
}
