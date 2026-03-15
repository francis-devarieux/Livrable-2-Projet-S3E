#include <Arduino.h>
#include <Wire.h>
#include "SdFat.h"
#include <RTClib.h>
#include <rgb_lcd.h>
#include <ChainableLED.h>
#include <SoftwareSerial.h>
// --- MODIFICATION : Bibliothèque GPS allégée ---
#include <TinyGPS.h> 
#include "DHT.h"
#include <avr/interrupt.h>
#include <math.h>

// --- CONFIGURATION ---
const int SD_CS = 4;
const int DHT_PIN = 5;
const int LIGHT_PIN = A2;
const unsigned int MAX_SZ = 4096;

// --- ETATS ---
enum Mode { STD, ECO, CONF, MAINT };
Mode mode = STD;

SdFat sd; File logF; RTC_DS1307 rtc; rgb_lcd lcd;
ChainableLED leds(6, 7, 1); DHT dht(DHT_PIN, DHT11);
SoftwareSerial ss(A0, A1); 
TinyGPS gps; // <-- Utilisation de TinyGPS classique

// --- GESTION DES BOUTONS PAR INTERRUPTION ---
volatile bool flagBtnVert = false; 
volatile bool flagBtnRouge = false;

unsigned long b_v_start = 0, b_r_start = 0;
bool b_v_long = false, b_r_long = false;

void isrBtnVert() { 
  flagBtnVert = true; 
}
void isrBtnRouge() { 
  flagBtnRouge = true; 
}

// --- GESTION DES ERREURS ---
enum ErrorCode { ERR_NONE = 0, ERR_RTC, ERR_GPS, ERR_SENSOR_HT_ACCESS, ERR_SENSOR_L_ACCESS, ERR_SENSOR_INCOH, ERR_SD_FULL, ERR_SD_RW };
volatile bool ledUpdateFlag = false;
volatile bool phaseIsSecondColor = false;
volatile uint16_t tickMs = 0;
volatile uint16_t durA = 500;
ErrorCode currentErr = ERR_NONE;

bool rtcOK = false, sdOK = false, needNormalRefresh = false;
unsigned long lastGpsByteMs = 0, fixedMsgUntil = 0, lastRtcCheck = 0, lastSdCheck = 0, lastLightCheck = 0;
const unsigned long GPS_TIMEOUT_MS = 5000, RTC_CHECK_MS = 1000, SD_CHECK_MS = 2000, LIGHT_CHECK_MS = 1000;
unsigned long dl = 0, dd = 0;

// --- NOUVEAU : CHRONOMETRE POUR LE MODE CONFIGURATION ---
unsigned long confStartTime = 0; 
const unsigned long TIMEOUT_CONF_MS = 1800000L; // 30 minutes (30 * 60 * 1000 ms)

uint8_t sdFailCount = 0, lightFailCount = 0, lightOkCount = 0;
float lastTemp = NAN, lastHum = NAN; int lastLight = 0;
const uint8_t LIGHT_FAIL_THRESHOLD = 3, LIGHT_OK_THRESHOLD = 3;
bool lightSensorErrorLatched = false;

// --- COULEURS ---
const uint8_t RR = 255, RG = 0, RB = 0, BR = 0, BG = 0, BB = 255;
const uint8_t YR = 255, YG = 255, YB = 0, GR = 0, GG = 255, GB = 0, WR = 255, WG = 255, WB = 255;

static inline bool fixedMsgActive() { 
  return millis() < fixedMsgUntil; 
}

// INTERRUPTION TIMER 1 ms
ISR(TIMER1_COMPA_vect) {
  tickMs++;
  if (tickMs >= 1000) {
    tickMs = 0;
  }
  bool newPhase = (tickMs >= durA);
  if (newPhase != phaseIsSecondColor) { 
    phaseIsSecondColor = newPhase; 
    ledUpdateFlag = true; 
  }
}

void timer1_init_1ms() {
  cli(); 
  TCCR1A = 0; 
  TCCR1B = 0; 
  TCNT1 = 0;
  TCCR1B |= (1 << WGM12); 
  OCR1A = 249;
  TCCR1B |= (1 << CS11) | (1 << CS10); 
  TIMSK1 |= (1 << OCIE1A); 
  sei();
}

// OUTILS
void ledSet(uint8_t r, uint8_t g, uint8_t b) { 
  leds.setColorRGB(0, r, g, b); 
}

void setBlinkPatternForError(ErrorCode e) { 
  if (e == ERR_SENSOR_INCOH || e == ERR_SD_RW) {
    durA = 333; 
  } else {
    durA = 500; 
  }
}

bool i2cDevicePresent(uint8_t address) { 
  Wire.beginTransmission(address); 
  return (Wire.endTransmission() == 0); 
}

bool rtcPresent() { 
  return i2cDevicePresent(0x68); 
}

bool sdHealthCheck() { 
  return sd.begin(SD_CS); 
}

// LUMINOSITE AVEC PULL-UP
bool lightAcquisitionOK(int &valueOut) {
  const int N = 12; 
  int minV = 1023;
  int maxV = 0; 
  long sum = 0;
  
  for (int i = 0; i < N; i++) {
    int v = analogRead(LIGHT_PIN);
    if (v < minV) { minV = v; }
    if (v > maxV) { maxV = v; }
    sum += v; 
    delay(2);
  }
  valueOut = sum / N;

  // Test de débranchement
  pinMode(LIGHT_PIN, INPUT_PULLUP);
  delay(2);
  int testPullUp = analogRead(LIGHT_PIN);
  pinMode(LIGHT_PIN, INPUT);

  if (testPullUp > 1020) {
    return false; // Déclenche l'erreur
  }
  return true;
}

bool rtcTimeReadable(DateTime &dt) {
  if (!rtcPresent()) {
    return false; 
  }
  dt = rtc.now();
  if (dt.year() < 2020 || dt.year() > 2099) {
    return false; 
  }
  return true;
}

void setLcdColorForError(ErrorCode e) {
  switch (e) {
    case ERR_RTC: lcd.setRGB(0, 0, 255); break;
    case ERR_GPS: lcd.setRGB(255, 255, 0); break;
    case ERR_SENSOR_HT_ACCESS: case ERR_SENSOR_L_ACCESS: case ERR_SENSOR_INCOH: lcd.setRGB(0, 255, 0); break;
    case ERR_SD_FULL: case ERR_SD_RW: lcd.setRGB(255, 255, 255); break;
    default:
      if (mode == STD) { lcd.setRGB(0, 255, 0); } 
      else if (mode == ECO) { lcd.setRGB(0, 0, 255); } 
      else if (mode == CONF) { lcd.setRGB(255, 255, 0); } 
      else { lcd.setRGB(255, 86, 0); }
      break;
  }
}

void applyErrorLedNow(ErrorCode e, bool secondColor) {
  switch (e) {
    case ERR_RTC: if (!secondColor) { ledSet(RR, RG, RB); } else { ledSet(BR, BG, BB); } break;
    case ERR_GPS: if (!secondColor) { ledSet(RR, RG, RB); } else { ledSet(YR, YG, YB); } break;
    case ERR_SENSOR_HT_ACCESS: case ERR_SENSOR_L_ACCESS: case ERR_SENSOR_INCOH: if (!secondColor) { ledSet(RR, RG, RB); } else { ledSet(GR, GG, GB); } break;
    case ERR_SD_FULL: case ERR_SD_RW: if (!secondColor) { ledSet(RR, RG, RB); } else { ledSet(WR, WG, WB); } break;
    default: break;
  }
}

void showErrorLCD(ErrorCode e) {
  lcd.clear(); 
  setLcdColorForError(e); 
  lcd.setCursor(0, 0); 
  lcd.print(F("ERREUR")); 
  lcd.setCursor(0, 1);
  switch (e) {
    case ERR_RTC: lcd.print(F("Acces RTC")); break;
    case ERR_GPS: lcd.print(F("Acces GPS")); break;
    case ERR_SENSOR_HT_ACCESS: lcd.print(F("Capteur H/T")); break;
    case ERR_SENSOR_L_ACCESS: lcd.print(F("Capteur L")); break;
    case ERR_SENSOR_INCOH: lcd.print(F("Capt incoher")); break;
    case ERR_SD_FULL: lcd.print(F("SD pleine")); break;
    case ERR_SD_RW: lcd.print(F("SD acces/ecr")); break;
    default: break;
  }
}

void showFixedLCD() {
  lcd.clear();
  if (mode == STD) { lcd.setRGB(0, 255, 0); } 
  else if (mode == ECO) { lcd.setRGB(0, 0, 255); } 
  else if (mode == CONF) { lcd.setRGB(255, 255, 0); } 
  else { lcd.setRGB(255, 86, 0); }
  
  lcd.setCursor(0, 0); 
  lcd.print(F("PROBLEME")); 
  lcd.setCursor(0, 1); 
  lcd.print(F("REGLE"));
  fixedMsgUntil = millis() + 2000UL; 
  needNormalRefresh = true;
}

ErrorCode choosePriority(bool sdRw, bool sdFull, bool rtcFail, bool gpsFail, bool htFail, bool lFail, bool sensIncoh) {
  if (sdRw) { return ERR_SD_RW; } 
  if (sdFull) { return ERR_SD_FULL; } 
  if (rtcFail) { return ERR_RTC; }
  if (gpsFail) { return ERR_GPS; } 
  if (htFail) { return ERR_SENSOR_HT_ACCESS; } 
  if (lFail) { return ERR_SENSOR_L_ACCESS; }
  if (sensIncoh) { return ERR_SENSOR_INCOH; } 
  return ERR_NONE;
}

void updateError(ErrorCode newErr) { 
  currentErr = newErr; 
  setBlinkPatternForError(newErr); 
}

void restoreModeLed() {
  if (mode == STD) { ledSet(0, 255, 0); } 
  else if (mode == ECO) { ledSet(0, 0, 255); } 
  else if (mode == CONF) { ledSet(255, 255, 0); } 
  else { ledSet(255, 86, 0); }
}

void setM(Mode m, uint8_t r, uint8_t g, uint8_t b) {
  mode = m; 
  if (currentErr == ERR_NONE && !fixedMsgActive()) { 
    ledSet(r, g, b); 
    lcd.setRGB(r, g, b); 
  } 
  needNormalRefresh = true;
}

// --- MODIFICATION : ECRAN ALLEGE (AFFICHAGE DES CAPTEURS EN MAINTENANCE UNIQUEMENT) ---
void drawNormalScreen(float t, float h, int lightValue, bool lightOK, bool rtcReadable) {
  lcd.clear();
  if (mode == STD) { lcd.setRGB(0, 255, 0); } 
  else if (mode == ECO) { lcd.setRGB(0, 0, 255); } 
  else if (mode == CONF) { lcd.setRGB(255, 255, 0); } 
  else { lcd.setRGB(255, 86, 0); }
  
  lcd.print(F("M:"));
  
  // En mode STD ou ECO, on n'affiche plus que l'heure pour gagner de la place
  if (mode == STD || mode == ECO) {
    lcd.print(mode == STD ? F("STD") : F("ECO"));
    lcd.setCursor(0, 1);
    
    if (rtcReadable) { 
      DateTime n = rtc.now(); 
      if (n.hour() < 10) lcd.print('0'); 
      lcd.print(n.hour()); 
      lcd.print(':'); 
      if (n.minute() < 10) lcd.print('0'); 
      lcd.print(n.minute()); 
    } else {
      lcd.print(F("NO RTC")); 
    }
  } 
  // En mode MAINT, on affiche les valeurs techniques
  else if (mode == MAINT) { 
    lcd.print(F("MAINT"));
    static int p = 0; 
    lcd.setCursor(0, 1);
    
    if (p == 0) { 
      lcd.print((int)t); lcd.print(F("C ")); lcd.print((int)h); lcd.print(F("%H")); 
    }
    else if (p == 1) { 
      float lat, lon; unsigned long age;
      gps.f_get_position(&lat, &lon, &age);
      if (lat != TinyGPS::GPS_INVALID_F_ANGLE) { 
        lcd.print(lat, 1); lcd.print(F(" ")); lcd.print(lon, 1); 
      } else {
        lcd.print(F("NO GPS")); 
      }
    }
    else if (p == 2) { 
      if (!lightOK) { lcd.print(F("NO LIGHT")); } else { lcd.print(F("LUM:")); lcd.print(lightValue); } 
    }
    p = (p + 1) % 3;
  } 
  else { // Mode CONF
    lcd.print(F("CONF"));
    lcd.setCursor(0, 1); 
    lcd.print(F("ACTIF")); 
  }
}

void setup() {
  Wire.begin(); 
  ss.begin(9600); 
  lcd.begin(16, 2); 
  dht.begin();
  
  pinMode(2, INPUT_PULLUP); 
  pinMode(3, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(2), isrBtnVert, CHANGE);
  attachInterrupt(digitalPinToInterrupt(3), isrBtnRouge, CHANGE);

  timer1_init_1ms();
  rtcOK = rtcPresent() && rtc.begin(); 
  sdOK = sd.begin(SD_CS);
  setM(STD, 0, 255, 0); 
  restoreModeLed(); 
  needNormalRefresh = true;
}

void loop() {
  // --- NOUVEAU : TIMEOUT DU MODE CONFIGURATION ---
  if (mode == CONF && (millis() - confStartTime >= TIMEOUT_CONF_MS)) {
    setM(STD, 0, 255, 0); // Retour forcé en mode Standard (Vert) au bout de 30min
  }

  while (ss.available() > 0) { 
    lastGpsByteMs = millis(); 
    gps.encode(ss.read()); 
  }

  // --- TRAITEMENT DES BOUTONS DEPUIS LES INTERRUPTIONS ---
  if (flagBtnVert) {
    if (digitalRead(2) == LOW) { 
      if (b_v_start == 0) { b_v_start = millis(); }
    } else { 
      b_v_start = 0; 
      b_v_long = false; 
    }
    flagBtnVert = false; 
  }

  if (flagBtnRouge) {
    if (digitalRead(3) == LOW) { 
      if (b_r_start == 0) { b_r_start = millis(); }
    } else { 
      if (b_r_start > 0 && millis() - b_r_start < 2800) {
        setM(CONF, 255, 255, 0); 
        confStartTime = millis(); // <--- Lancement du chrono de 30 min ici !
      }
      b_r_start = 0; 
      b_r_long = false; 
    }
    flagBtnRouge = false; 
  }

  // --- VALIDATION DES TEMPS LONGS ---
  if (b_v_start > 0 && millis() - b_v_start >= 2800 && !b_v_long) {
    if (mode == STD) { setM(ECO, 0, 0, 255); }
    b_v_long = true;
  }
  
  if (b_r_start > 0 && millis() - b_r_start >= 2800 && !b_r_long) {
    if (mode != STD) { setM(STD, 0, 255, 0); } 
    else { setM(MAINT, 255, 86, 0); }
    b_r_long = true;
  }

  float t = dht.readTemperature(); 
  float h = dht.readHumidity();
  if (!isnan(t)) { lastTemp = t; }
  if (!isnan(h)) { lastHum = h; }
  
  bool htFail = isnan(t) || isnan(h); 
  bool sensIncoh = false;
  if (!htFail) { 
    if (h < 0.0f || h > 100.0f) { sensIncoh = true; }
    if (t < -40.0f || t > 80.0f) { sensIncoh = true; }
  }

  int lightValue = lastLight; 
  if (lightSensorErrorLatched) { lightValue = -1; }
  
  bool lightReadingNowOK = true;
  if (millis() - lastLightCheck >= LIGHT_CHECK_MS) {
    lightReadingNowOK = lightAcquisitionOK(lightValue);
    if (lightReadingNowOK) { 
      lastLight = lightValue; 
      if (lightOkCount < 255) { lightOkCount++; }
      lightFailCount = 0; 
      if (lightOkCount >= LIGHT_OK_THRESHOLD) { lightSensorErrorLatched = false; }
    } else { 
      if (lightFailCount < 255) { lightFailCount++; }
      lightOkCount = 0; 
      if (lightFailCount >= LIGHT_FAIL_THRESHOLD) { lightSensorErrorLatched = true; }
    }
    lastLightCheck = millis();
  } else {
    lightValue = lastLight;
  }

  bool lightOK = !lightSensorErrorLatched; 
  bool lightFail = lightSensorErrorLatched;
  bool rtcReadable = false;
  
  if (millis() - lastRtcCheck >= RTC_CHECK_MS) { 
    rtcOK = rtcPresent(); 
    if (rtcOK) { rtcOK = rtc.begin(); }
    lastRtcCheck = millis(); 
  }
  
  DateTime dt; 
  rtcReadable = rtcOK && rtcTimeReadable(dt);
  
  if (millis() - lastSdCheck >= SD_CHECK_MS) { 
    sdOK = sdHealthCheck(); 
    lastSdCheck = millis(); 
  }
  
  // Extraction des données GPS allégées
  float lat, lon; unsigned long age;
  gps.f_get_position(&lat, &lon, &age);
  bool gpsValid = (lat != TinyGPS::GPS_INVALID_F_ANGLE);

  bool sdRw = !sdOK; 
  bool sdFull = false;
  bool gpsFail = (millis() - lastGpsByteMs > GPS_TIMEOUT_MS);
  
  // --- BYPASS DE L'HORLOGE CASSÉE ---
  bool rtcFail = false; 

  ErrorCode newErr = choosePriority(sdRw, sdFull, rtcFail, gpsFail, htFail, lightFail, sensIncoh);

  if (newErr != currentErr) { 
    ErrorCode oldErr = currentErr; 
    updateError(newErr); 
    if (newErr != ERR_NONE) { showErrorLCD(newErr); } 
    else if (oldErr != ERR_NONE) { showFixedLCD(); }
  }
  
  if (currentErr != ERR_NONE) {
    bool doUpdate = false; 
    bool second = false;
    cli(); 
    if (ledUpdateFlag) { ledUpdateFlag = false; doUpdate = true; } 
    second = phaseIsSecondColor; 
    sei();
    
    static ErrorCode lastAppliedErr = ERR_NONE;
    if (doUpdate || lastAppliedErr != currentErr) { 
      applyErrorLedNow(currentErr, second); 
      lastAppliedErr = currentErr; 
    }
  } else { 
    if (!fixedMsgActive()) { restoreModeLed(); }
  }

  if (!fixedMsgActive() && needNormalRefresh && currentErr == ERR_NONE) { 
    drawNormalScreen(lastTemp, lastHum, lastLight, lightOK, rtcReadable); 
    restoreModeLed(); 
    dd = millis(); 
    needNormalRefresh = false; 
  }
  
  if (currentErr == ERR_NONE && !fixedMsgActive() && (millis() - dd >= 3000)) { 
    drawNormalScreen(lastTemp, lastHum, lastLight, lightOK, rtcReadable); 
    dd = millis(); 
  }

  // --- SAUVEGARDE SUR CARTE SD ---
  if (millis() - dl >= (mode == ECO ? 1200000UL : 600000UL) && mode != MAINT) {
    if (sdOK) {
      if (sd.exists("L.CSV")) {
        File f = sd.open("L.CSV", FILE_READ);
        if (f) { 
          if (f.size() >= MAX_SZ) { 
            f.close(); 
            if (!sd.rename("L.CSV", "O.CSV")) { sdFailCount++; } else { sdFailCount = 0; }
          } else { f.close(); }
        } else { sdFailCount++; }
      }
      logF = sd.open("L.CSV", FILE_WRITE);
      if (logF) {
        if (logF.size() == 0) {
          logF.println(F("DateHeure;Latitude/Longitude;Temperature_C;Humidite_%;Luminosite"));
        }
        if (rtcReadable) { logF.print(rtc.now().timestamp()); } else { logF.print(F("NO_RTC")); }
        logF.print(';');
        
        if (gpsValid) { logF.print(lat, 6); } else { logF.print(F("NO_GPS")); }
        logF.print(';');
        
        if (!isnan(lastTemp)) { logF.print(lastTemp, 2); } else { logF.print(F("NaN")); }
        logF.print(';');
        
        if (!isnan(lastHum)) { logF.print(lastHum, 2); } else { logF.print(F("NaN")); }
        logF.print(';');
        
        if (lightOK) { logF.println(lastLight); } else { logF.println(F("NO_LIGHT")); }
        
        logF.close(); 
        sdFailCount = 0;
      } else { sdFailCount++; }
      
      if (sdFailCount >= 2) { sdOK = false; }
    }
    dl = millis();
  }
}
