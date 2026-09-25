//#define CALIBRATE
//#define GRID // After Calibration this mode should plot a pretty good dot
// grid on the screen

// See additional calibration comments in the code down by the "#ifdef
// CALIBRATE" Adjust these values for servo arms in position for state 1 _|
const double SERVO_LEFT_ZERO =  1650;
const double SERVO_RIGHT_SCALE = 700; // + makes rotate further left

// Adjust these values for servo arms in position for state 2 |_
const double SERVO_RIGHT_ZERO = 750;
const double SERVO_LEFT_SCALE = 650;

//#define OPTION_12_HOUR // 12 or comment out this line for 24 hour time

const double DRAW_DELAY = 3; // 3

///////////////////////////////////////////////////////////////////////////////

// Plotclock
// cc - by Johannes Heberlein 2014
// modified for glow clock - Tucker Shannon 2018
// improved - 12Me21 2018
// ESP32 (classic / mini WROOM-32) port - 2026

// v 1.08
// 适用于"ESP32 mini"这类经典 ESP32（ESP32-WROOM-32）迷你开发板
// 与 ESP32-C3 版固件逻辑完全相同，仅引脚定义不同：
//   - WiFi.h + ESP32Servo（与 C3 版一致，需自行安装 ESP32Servo）
//   - 左舵机 GPIO16 / 右舵机 GPIO17 / UV LED GPIO18 / 按钮 GPIO4
//   - 避开启动引脚 GPIO0 / 2 / 5 / 12 / 15，IO34-39 只能输入也不能用

///////////////////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <ESP32Servo.h>
#include <WiFiManager.h>
#include <time.h>

// NTP Servers
const char *ntpServer = "ntp1.aliyun.com";
const char *ntpServer2 = "ntp2.aliyun.com";
const char *ntpServer3 = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

// 经典 ESP32（30 引脚 mini 板）引脚定义
// 启动相关引脚 GPIO0/2/5/12/15 与只能输入的 GPIO34-39 全部避开
#define SERVOPINLEFT 16  // GPIO16 左舵机信号
#define SERVOPINRIGHT 17 // GPIO17 右舵机信号
#define LASER_PIN 18     // GPIO18 UV LED（低电平点亮）
#define BUTTON_PIN 4     // GPIO4  内置上拉，按下接地
const double LOWER_ARM = 35;      // servo to lower arm joint
const double UPPER_ARM_LEFT = 56; // lower arm joint to led
const double LED_ARM = 13.5;      // upper arm joint to led
const double UPPER_ARM = 45;      // lower arm joint to upper arm joint
double cosineRule(double a, double b, double c);
const double LED_ANGLE = cosineRule(UPPER_ARM_LEFT, UPPER_ARM, LED_ARM);

// Location of servos relative to origin
const double SERVO_LEFT_X = 22;
const double SERVO_LEFT_Y = -32;
const double SERVO_RIGHT_X = SERVO_LEFT_X + 25.5;
const double SERVO_RIGHT_Y = SERVO_LEFT_Y;

// lovely macros
#define radian(angle) (M_PI * 2 * angle)
#define dist(x, y) sqrt(sq(x) + sq(y))
#define angle(x, y) atan2(y, x)

// digit location/size constants
const double TIME_BOTTOM = 12;
const double TIME_WIDTH = 11;
const double TIME_HEIGHT = 18; // 16;

const double HOME_X = 55, HOME_Y = -5;
Servo servoLeft, servoRight;

double lastX = HOME_X, lastY = HOME_Y;

bool lightOn = false;
volatile bool buttonPressed = false;

#define ISR_ATTR IRAM_ATTR

void ISR_ATTR buttonISR() {
  buttonPressed = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, HIGH); // OFF (Active LOW)
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 使用内部上拉，按钮另一端接GND
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING); // 下降沿触发

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  if (!wifiManager.autoConnect("PlotClock-Setup")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
  }
  Serial.println("connected...yeey :)");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, ntpServer2,
             ntpServer3);
}

void light(bool state) {
  lightOn = (state == HIGH);
  delay(50);
  digitalWrite(LASER_PIN, !state); // Active LOW: HIGH->LOW, LOW->HIGH
}

const int LONG_PRESS_DURATION = 750;

void loop() {
  // 等待中断触发
  if (!buttonPressed) {
    delay(10); // 让出CPU给WiFi处理
    return;
  }

  // 消抖
  delay(50);
  buttonPressed = false;

  // Touch detected
  if (!servoLeft.attached())
    servoLeft.attach(SERVOPINLEFT);
  if (!servoRight.attached())
    servoRight.attach(SERVOPINRIGHT);

#ifdef CALIBRATE
  // Calibration logic remains...
  static bool half;
  servoLeft.writeMicroseconds(
      floor(SERVO_LEFT_ZERO + (half ? -M_PI / 2 : 0) * SERVO_LEFT_SCALE));
  servoRight.writeMicroseconds(
      floor(SERVO_RIGHT_ZERO + (half ? 0 : M_PI / 2) * SERVO_RIGHT_SCALE));
  light(half ? LOW : HIGH);
  half = !half;
  delay(2000);
#elif defined(GRID)
  // Grid logic remains...
  for (int i = 0; i <= 70; i += 10) {
    for (int j = 0; j <= 40; j += 10) {
      drawTo(i, j);
      light(HIGH);
      light(LOW);
    }
  }
  drawTo(HOME_X, HOME_Y);
  servoLeft.detach();
  servoRight.detach();
#else
  // Draw Time
  drawTo(HOME_X, 0);

  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  int hour = timeinfo->tm_hour;
  int minute = timeinfo->tm_min;

#ifdef OPTION_12_HOUR
  if (hour >= 12)
    hour -= 12;
  if (hour == 0)
    hour = 12;
#endif
  Serial.print(hour);
  Serial.print(":");
  Serial.println(minute);
  // Draw Hour
  if (hour / 10)
    drawDigit(3, TIME_BOTTOM, TIME_WIDTH, TIME_HEIGHT, hour / 10);
  drawDigit(3 + TIME_WIDTH + 3, TIME_BOTTOM, TIME_WIDTH, TIME_HEIGHT,
            hour % 10);

  // Draw Colon
  drawDigit((69 - TIME_WIDTH) / 2, TIME_BOTTOM, TIME_WIDTH, TIME_HEIGHT, 11);

  // Draw Minute
  drawDigit(69 - (TIME_WIDTH + 3) * 2, TIME_BOTTOM, TIME_WIDTH, TIME_HEIGHT,
            minute / 10);
  drawDigit(72 - (TIME_WIDTH + 3), TIME_BOTTOM, TIME_WIDTH, TIME_HEIGHT,
            minute % 10);

  drawTo(HOME_X, HOME_Y);
  servoLeft.detach();
  servoRight.detach();

  // De-bounce / wait for release might be good, but simple delay for now
  delay(1000);
#endif
}

#define digitMove(dx, dy) drawTo(x + width * dx, y + height * dy)
#define digitStart(dx, dy)                                                     \
  digitMove(dx, dy);                                                           \
  light(HIGH)
#define digitArc(dx, dy, rx, ry, start, last)                                  \
  drawArc(x + width * dx, y + height * dy, width * rx, height * ry,            \
          radian(start), radian(last))

// Symbol is drawn with the lower left corner at (x,y) and a size of
// (width,height).
void drawDigit(double x, double y, double width, double height, char digit) {
  // see macros for reference
  switch (digit) {
  case 0: //
    digitStart(1 / 2, 1);
    digitArc(1 / 2, 1 / 2, 1 / 2, 1 / 2, 1 / 4, -3 / 4);
    // digitStart(1,1/2);
    // digitArc(1/2,1/2, 1/2,1/2, 0, 1.02);
    break;
  case 1: //
    digitStart(1 / 4, 7 / 8);
    digitMove(1 / 2, 1);
    digitMove(1 / 2, 0);
    break;
  case 2: //
    digitStart(0, 3 / 4);
    digitArc(1 / 2, 3 / 4, 1 / 2, 1 / 4, 1 / 2, -1 / 8);
    digitArc(1, 0, 1, 1 / 2, 3 / 8, 1 / 2);
    digitMove(1, 0);
    break;
  case 3:
    digitStart(0, 3 / 4);
    digitArc(1 / 2, 3 / 4, 1 / 2, 1 / 4, 3 / 8, -1 / 4);
    digitArc(1 / 2, 1 / 4, 1 / 2, 1 / 4, 1 / 4, -3 / 8);
    break;
  case 4:
    digitStart(1, 3 / 8);
    digitMove(0, 3 / 8);
    digitMove(3 / 4, 1);
    digitMove(3 / 4, 0);
    break;
  case 5: // wayy too many damn lines
    digitStart(1, 1);
    digitMove(0, 1);
    digitMove(0, 1 / 2);
    digitMove(1 / 2, 1 / 2);
    digitArc(1 / 2, 1 / 4, 1 / 2, 1 / 4, 1 / 4, -1 / 4);
    digitMove(0, 0);
    break;
  case 6:
    digitStart(0, 1 / 4);
    digitArc(1 / 2, 1 / 4, 1 / 2, 1 / 4, 1 / 2, -1 / 2);
    digitArc(1, 1 / 2, 1, 1 / 2, 1 / 2, 1 / 4);
    break;
  case 7:
    digitStart(0, 1);
    digitMove(1, 1);
    digitMove(1 / 4, 0);
    break;
  case 8:
    digitStart(1 / 2, 1 / 2);
    digitArc(1 / 2, 3 / 4, 1 / 2, 1 / 4, -1 / 4, 3 / 4);
    digitArc(1 / 2, 1 / 4, 1 / 2, 1 / 4, 1 / 4, -3 / 4);
    break;
  case 9:
    digitStart(1, 3 / 4);
    digitArc(1 / 2, 3 / 4, 1 / 2, 1 / 4, 0, 1);
    digitMove(3 / 4, 0);
    break;
  case 10: // dot
    digitStart(0, 0);
    // digitMove(0,1);
    // digitMove(1,1);
    // digitMove(1,0);
    break;
  case 11: // colon
    digitStart(1 / 2, 3 / 4);
    light(LOW);
    digitStart(1 / 2, 1 / 4);
    break;
  case 12: // slash
    digitStart(3 / 4, 5 / 4);
    digitMove(1 / 4, -1 / 4);
    break;
  }
  light(LOW);
}

#define ARCSTEP 0.05 // 0.05 //should change depending on radius...
void drawArc(double x, double y, double rx, double ry, double pos,
             double last) {
  if (pos < last)
    for (; pos <= last; pos += ARCSTEP)
      drawTo(x + cos(pos) * rx, y + sin(pos) * ry);
  else
    for (; pos >= last; pos -= ARCSTEP)
      drawTo(x + cos(pos) * rx, y + sin(pos) * ry);
}

// didn't really change this
void drawTo(double pX, double pY) {
  double dx, dy, c;
  int i;

  // dx dy of new point
  dx = pX - lastX;
  dy = pY - lastY;
  // path length in mm, times 4 equals 4 steps per mm
  c = floor(4 * dist(dx, dy));

  if (c < 1)
    c = 1;

  // draw line point by point
  for (i = 1; i <= c; i++) {
    set_XY(lastX + (i * dx / c), lastY + (i * dy / c));
    if (lightOn)
      delay(DRAW_DELAY);
  }

  lastX = pX;
  lastY = pY;
}

// cosine rule for angle between c and a
double cosineRule(double a, double b, double c) {
  return acos((sq(a) + sq(c) - sq(b)) / (2 * a * c));
}

void set_XY(double x, double y) {
  // Calculate triangle between left servo, left arm joint, and light
  // Position of pen relative to left servo
  // rectangular
  double penX = x - SERVO_LEFT_X;
  double penY = y - SERVO_LEFT_Y;
  // polar
  double penAngle = angle(penX, penY);
  double penDist = dist(penX, penY);
  // get angle between lower arm and a line connecting the left servo and the
  // pen
  double bottomAngle = cosineRule(LOWER_ARM, UPPER_ARM_LEFT, penDist);

  servoLeft.writeMicroseconds(floor(
      SERVO_LEFT_ZERO + (bottomAngle + penAngle - M_PI) * SERVO_LEFT_SCALE));

  // calculate middle arm joint location
  double topAngle = cosineRule(UPPER_ARM_LEFT, LOWER_ARM, penDist);
  double lightAngle = penAngle - topAngle + LED_ANGLE + M_PI;
  double jointX = x - SERVO_RIGHT_X + cos(lightAngle) * LED_ARM;
  double jointY = y - SERVO_RIGHT_Y + sin(lightAngle) * LED_ARM;

  bottomAngle = cosineRule(LOWER_ARM, UPPER_ARM, dist(jointX, jointY));
  double jointAngle = angle(jointX, jointY);

  servoRight.writeMicroseconds(
      floor(SERVO_RIGHT_ZERO + (jointAngle - bottomAngle) * SERVO_RIGHT_SCALE));
}