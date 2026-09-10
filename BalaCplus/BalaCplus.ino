// BalaC balancing robot — Arduino Nesso N1 port
// Original by Kiraku Labo / M5Stack, ported by Eoin Jordan, 2026
//
// 1. Lay flat on power-on → Cal-1 (gyro bias) runs automatically
// 2. Hold upright until Cal-2 triggers → robot starts balancing
//
// KEY1 short press : re-run Cal-1
// KEY2 long press  : toggle Stand / Demo mode

#include <Arduino_Nesso_N1.h>
#include <Wire.h>

// BLE remote control — Nordic UART Service (app -> robot teleop, robot -> app telemetry)
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// ── Hardware objects ────────────────────────────────────────────────────────
NessoDisplay lcd;
NessoBattery battery;

// ── Balance constants ───────────────────────────────────────────────────────
#define N_CAL1    100
#define N_CAL2    100
#define LCDV_MID  60
#define MOTOR_I2C 0x38   // BalaC base motor driver

const float     clk      = 0.01f;
const uint32_t  interval = (uint32_t)(clk * 1000UL);
const float     cutoff   = 0.1f;
const float     moveStep = 0.2f * clk;

// ── State ───────────────────────────────────────────────────────────────────
bool    standing       = false;
bool    serialMonitor  = true;
int16_t counter        = 0;
uint32_t time0 = 0, time1 = 0;
int16_t counterOverPwr = 0, maxOvp = 20;

float power, powerR, powerL, yawPower;
float varAng, varOmg, varSpd, varDst, varIang;
float gyroYoffset, gyroZoffset, accXoffset;
float gyroXdata, gyroYdata, gyroZdata, accXdata, accZdata;
float aveAccZ = 0.0f, aveAbsOmg = 0.0f;
float yawAngle = 0.0f;
float moveTarget = 0.0f, moveRate = 0.0f;
int16_t fbBalance = 0, motorDeadband = 0, maxPwr = 0;
float mechFactR, mechFactL;
bool  spinContinuous = false;
float spinDest = 0.0f, spinTarget = 0.0f, spinFact = 1.0f, spinStep = 0.0f;
int16_t ipowerL = 0, ipowerR = 0;
int16_t motorLdir = 0, motorRdir = 0;
int16_t punchPwr, punchPwr2, punchDur;
int16_t punchCountL = 0, punchCountR = 0;
float Kang, Komg, KIang, Kyaw, Kdst, Kspd;
byte  demoMode = 0;

// ── BLE remote control (Nordic UART Service) ────────────────────────────────
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"   // app -> robot commands
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"   // robot -> app telemetry
#define CMD_TIMEOUT_MS   600                 // failsafe: cut throttle if no command arrives
const float spinRateMax = 60.0f;             // deg/s of yaw at full steer

BLECharacteristic *txChar = nullptr;
volatile bool     bleConnected = false;
volatile int16_t  bleThrottle  = 0;          // -100..100 forward / back
volatile int16_t  bleSteer     = 0;          // -100..100 left / right
volatile uint32_t lastCmdMs    = 0;

// ── Prototypes ──────────────────────────────────────────────────────────────
void resetPara(); void resetVar();  void resetMotor();
void calib1();    void calib2();    void calDelay(int n);
void getGyro();   void readGyro();
void drive();     void startDemo(); void setMode(bool inc);
void sendStatus();
void drvMotor(byte ch, int8_t sp);
void drvMotorL(int16_t pwm); void drvMotorR(int16_t pwm);
void checkButtons();
void initBLE();   void applyRemote(); void sendTelemetry();

// ── BLE plumbing ─────────────────────────────────────────────────────────────
class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { bleConnected = true; }
  void onDisconnect(BLEServer* s) override {
    bleConnected = false;
    bleThrottle = 0; bleSteer = 0;
    s->getAdvertising()->start();          // keep the robot discoverable
  }
};

static void parseCommand(const String& line) {
  if (line.length() == 0) return;
  char c = line[0];
  if (c == 'S' || c == 's') { bleThrottle = 0; bleSteer = 0; lastCmdMs = millis(); return; }
  if (c == 'M' || c == 'm') { setMode(true);                 lastCmdMs = millis(); return; }
  if (c == 'D' || c == 'd') {                                 // "D,<throttle>,<steer>"
    int p1 = line.indexOf(',');
    int p2 = (p1 >= 0) ? line.indexOf(',', p1 + 1) : -1;
    if (p1 >= 0 && p2 >= 0) {
      bleThrottle = constrain((int)line.substring(p1 + 1, p2).toInt(), -100, 100);
      bleSteer    = constrain((int)line.substring(p2 + 1).toInt(),     -100, 100);
      lastCmdMs   = millis();
    }
  }
}

class RxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override {
    String v = ch->getValue();
    int start = 0;
    for (int i = 0; i <= (int)v.length(); i++) {   // tolerate one or many newline-terminated lines
      if (i == (int)v.length() || v[i] == '\n' || v[i] == '\r') {
        if (i > start) parseCommand(v.substring(start, i));
        start = i + 1;
      }
    }
  }
};

void initBLE() {
  BLEDevice::init("BalaC-Nesso");
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  BLEService *svc = server->createService(NUS_SERVICE_UUID);
  BLECharacteristic *rxChar = svc->createCharacteristic(
      NUS_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxChar->setCallbacks(new RxCB());

  txChar = svc->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  svc->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
}

// Map the latest BLE command onto the balance controller (Stand mode only).
void applyRemote() {
  if (millis() - lastCmdMs > CMD_TIMEOUT_MS) { bleThrottle = 0; bleSteer = 0; }
  moveRate = bleThrottle * 0.01f;                        // -1..1
  if (bleSteer != 0) {
    spinContinuous = true;
    spinStep = -(bleSteer * 0.01f) * spinRateMax * clk;  // yaw rate toward steer
  } else if (spinContinuous) {
    spinContinuous = false;
    spinDest = spinTarget;                               // hold current heading
  }
}

void sendTelemetry() {
  if (!bleConnected || txChar == nullptr) return;
  char buf[48];
  int n = snprintf(buf, sizeof(buf), "T,%d,%.1f,%d\n",
                   battery.getChargeLevel(), varAng, standing ? 1 : 0);
  txChar->setValue((uint8_t*)buf, n);
  txChar->notify();
}

// ── setup() ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Display (also configures KEY1, KEY2, LED_BUILTIN, LCD_BACKLIGHT via expander)
  lcd.begin();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextSize(2);
  lcd.setTextColor(TFT_WHITE);

  // I²C for motor driver (same bus, already started by lcd.begin())
  // Wire.begin() is called internally by NessoDisplay; nothing extra needed.

  // IMU — uses Wire with SDA=10, SCL=8 (Nesso default)
  if (!IMU.begin()) {
    lcd.setTextColor(TFT_RED);
    lcd.setCursor(5, 20);
    lcd.print("IMU FAIL");
    while (1) {}
  }
  lcd.setTextColor(TFT_GREEN);
  lcd.setCursor(5, 20);
  lcd.print("IMU OK");
  delay(600);
  lcd.fillScreen(TFT_BLACK);

  initBLE();                 // advertise as "BalaC-Nesso" for the Android controller

  resetMotor();
  resetPara();
  resetVar();
  calib1();
  setMode(false);
}

// ── loop() ──────────────────────────────────────────────────────────────────
void loop() {
  checkButtons();
  getGyro();

  if (!standing) {
    aveAbsOmg = aveAbsOmg * 0.9f + abs(varOmg) * 0.1f;
    aveAccZ   = aveAccZ   * 0.9f + accZdata    * 0.1f;
    lcd.setCursor(30, 130);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.printf("%5.2f ", -aveAccZ);
    if (abs(aveAccZ) > 0.9f && aveAbsOmg < 1.5f) {
      calib2();
      if (demoMode == 1) startDemo();
      standing = true;
    }
  } else {
    if (abs(varAng) > 30.0f || counterOverPwr > maxOvp) {
      resetMotor();
      resetVar();
      standing = false;
      setMode(false);
    } else {
      if (demoMode == 0) applyRemote();   // Stand mode = BLE teleop; Demo mode = autonomous
      drive();
    }
  }

  if (++counter >= 100) {
    counter = 0;
    // Show battery %
    lcd.setCursor(65, 110);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    lcd.printf("%3d%%  ", battery.getChargeLevel());
    if (serialMonitor) sendStatus();
    sendTelemetry();
  }

  do { time1 = millis(); } while (time1 - time0 < interval);
  time0 = time1;
}

// ── IMU ─────────────────────────────────────────────────────────────────────
void readGyro() {
  float gX, gY, gZ, aX, aY, aZ;
  IMU.readGyroscope(gX, gY, gZ);     // deg/s
  IMU.readAcceleration(aX, aY, aZ);  // g
  gyroYdata = gX;
  gyroZdata = -gY;
  gyroXdata = -gZ;
  accXdata  = aZ;   // pitch tilt axis (reads ~1g when flat, ~0g when upright)
  accZdata  = aX;   // upright detection (reads ~0g when flat, ~1g when upright)
}

void getGyro() {
  readGyro();
  varOmg    = gyroYdata - gyroYoffset;
  yawAngle += (gyroZdata - gyroZoffset) * clk;
  varAng   += (varOmg + ((accXdata - accXoffset) * 57.3f - varAng) * cutoff) * clk;
}

// ── Calibration ─────────────────────────────────────────────────────────────
void calib1() {
  calDelay(30);
  digitalWrite(LED_BUILTIN, LOW);
  calDelay(80);
  lcd.fillScreen(TFT_BLACK);
  lcd.setCursor(30, LCDV_MID);
  lcd.setTextColor(TFT_YELLOW);
  lcd.print(" Cal-1 ");
  gyroYoffset = 0.0f;
  for (int i = 0; i < N_CAL1; i++) { readGyro(); gyroYoffset += gyroYdata; delay(9); }
  gyroYoffset /= (float)N_CAL1;
  lcd.fillScreen(TFT_BLACK);
  digitalWrite(LED_BUILTIN, HIGH);
}

void calib2() {
  resetVar(); resetMotor();
  digitalWrite(LED_BUILTIN, LOW);
  calDelay(80);
  lcd.setCursor(30, LCDV_MID);
  lcd.setTextColor(TFT_YELLOW);
  lcd.print(" Cal-2 ");
  accXoffset = 0.0f; gyroZoffset = 0.0f;
  for (int i = 0; i < N_CAL2; i++) {
    readGyro();
    accXoffset  += accXdata;
    gyroZoffset += gyroZdata;
    delay(9);
  }
  accXoffset  /= (float)N_CAL2;
  gyroZoffset /= (float)N_CAL2;
  lcd.fillScreen(TFT_BLACK);
  digitalWrite(LED_BUILTIN, HIGH);
}

void calDelay(int n) { for (int i = 0; i < n; i++) { getGyro(); delay(9); } }

// ── Buttons ─────────────────────────────────────────────────────────────────
void checkButtons() {
  // KEY1 short press → Cal-1
  if (digitalRead(KEY1) == LOW) {
    delay(15);
    if (digitalRead(KEY1) == LOW) {
      while (digitalRead(KEY1) == LOW) {}
      calib1();
    }
  }
  // KEY2 long press → mode switch
  static uint32_t btn2Start = 0;
  if (digitalRead(KEY2) == LOW) {
    if (btn2Start == 0) btn2Start = millis();
    else if (millis() - btn2Start > 800) { setMode(true); btn2Start = 0; }
  } else {
    btn2Start = 0;
  }
}

// ── Drive ────────────────────────────────────────────────────────────────────
void drive() {
  spinFact = (abs(moveRate) > 0.1f)
    ? constrain(-(powerR + powerL) / 10.0f, -1.0f, 1.0f)
    : 1.0f;

  if (spinContinuous)       spinTarget += spinStep * spinFact;
  else {
    if (spinTarget < spinDest) spinTarget += spinStep;
    if (spinTarget > spinDest) spinTarget -= spinStep;
  }

  moveTarget += moveStep * (moveRate + (float)fbBalance / 100.0f);
  varSpd  += power * clk;
  varDst  += Kdst  * (varSpd * clk - moveTarget);
  varIang += KIang * varAng * clk;
  power    = varIang + varDst + (Kspd * varSpd) + (Kang * varAng) + (Komg * varOmg);

  counterOverPwr = (abs(power) > 1000.0f) ? counterOverPwr + 1 : 0;
  if (counterOverPwr > maxOvp) return;

  power    = constrain(power,  -maxPwr, maxPwr);
  yawPower = (yawAngle - spinTarget) * Kyaw;
  powerR   = power - yawPower;
  powerL   = power + yawPower;

  ipowerL      = (int16_t)constrain(powerL * mechFactL, -maxPwr, maxPwr);
  int16_t mdbn = -motorDeadband, pp2n = -punchPwr2;

  if (ipowerL > 0) {
    punchCountL = (motorLdir == 1) ? constrain(++punchCountL, 0, 100) : 0;
    motorLdir = 1;
    drvMotorL((punchCountL < punchDur) ? max(ipowerL, punchPwr2) : max(ipowerL, motorDeadband));
  } else if (ipowerL < 0) {
    punchCountL = (motorLdir == -1) ? constrain(++punchCountL, 0, 100) : 0;
    motorLdir = -1;
    drvMotorL((punchCountL < punchDur) ? min(ipowerL, pp2n) : min(ipowerL, mdbn));
  } else { drvMotorL(0); motorLdir = 0; }

  ipowerR = (int16_t)constrain(powerR * mechFactR, -maxPwr, maxPwr);
  if (ipowerR > 0) {
    punchCountR = (motorRdir == 1) ? constrain(++punchCountR, 0, 100) : 0;
    motorRdir = 1;
    drvMotorR((punchCountR < punchDur) ? max(ipowerR, punchPwr2) : max(ipowerR, motorDeadband));
  } else if (ipowerR < 0) {
    punchCountR = (motorRdir == -1) ? constrain(++punchCountR, 0, 100) : 0;
    motorRdir = -1;
    drvMotorR((punchCountR < punchDur) ? min(ipowerR, pp2n) : min(ipowerR, mdbn));
  } else { drvMotorR(0); motorRdir = 0; }
}

// ── Motor driver ─────────────────────────────────────────────────────────────
void drvMotor(byte ch, int8_t sp) {
  Wire.beginTransmission(MOTOR_I2C);
  Wire.write(ch); Wire.write(sp);
  Wire.endTransmission();
}
void drvMotorL(int16_t pwm) { drvMotor(0,  (int8_t)constrain(pwm, -127, 127)); }
void drvMotorR(int16_t pwm) { drvMotor(1, -(int8_t)constrain(pwm, -127, 127)); }
void resetMotor() { drvMotorR(0); drvMotorL(0); counterOverPwr = 0; }

// ── Utility ──────────────────────────────────────────────────────────────────
void resetPara() {
  Kang = 37.0f; Komg = 0.84f; KIang = 800.0f; Kyaw = 4.0f;
  Kdst = 85.0f; Kspd = 2.7f;
  mechFactL = 0.45f; mechFactR = 0.45f;
  punchPwr = 20; punchDur = 1;
  fbBalance = -3; motorDeadband = 10; maxPwr = 120;
  punchPwr2 = max(punchPwr, motorDeadband);
}

void resetVar() {
  power = powerR = powerL = yawPower = 0.0f;
  varAng = varOmg = varSpd = varDst = varIang = 0.0f;
  moveTarget = moveRate = yawAngle = 0.0f;
  spinContinuous = false; spinDest = spinTarget = spinStep = 0.0f;
}

void setMode(bool inc) {
  if (inc) demoMode = (demoMode + 1) % 2;
  lcd.fillScreen(TFT_BLACK);
  lcd.setCursor(30, 5);
  lcd.setTextColor(TFT_WHITE);
  lcd.print(demoMode == 0 ? "Stand" : "Demo ");
}

void startDemo() { moveRate = 1.0f; spinContinuous = true; spinStep = -40.0f * clk; }

void sendStatus() {
  float aX, aY, aZ;
  IMU.readAcceleration(aX, aY, aZ);
  Serial.print("power="); Serial.print(power);
  Serial.print(" ang=");  Serial.print(varAng);
  Serial.print(" aX=");   Serial.print(aX, 2);
  Serial.print(" aY=");   Serial.print(aY, 2);
  Serial.print(" aZ=");   Serial.print(aZ, 2);
  Serial.print(" aveAccZ="); Serial.print(aveAccZ, 2);
  Serial.print(" t=");    Serial.println(millis() - time0);
}
