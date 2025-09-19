// =================================================================================
// File:         src/hardware.cpp
// Description:  Реализация функций для работы с аппаратной частью.
//               Управление реле, чтение входов, работа с дисплеем и RTC.
// =================================================================================

#include "hardware.h"
#include "sensors.h"
#include "pid_control.h"
#include "web_server.h"
#include "pump_control.h"
#include "utils.h"


// Вспомогательные переменные, нужные только этому файлу
uint8_t displayErrorCounter = 0;
uint8_t relayErrorCounter = 0;
uint8_t inputErrorCounter = 0;
uint8_t rtcErrorCounter = 0;
const uint8_t I2C_ERROR_THRESHOLD = 5;

unsigned long lastI2CRecoveryAttempt = 0;
const unsigned long I2C_RECOVERY_INTERVAL = 15000; // 15 секунд

unsigned long apModeStartTime = 0;

// Для кнопки
int buttonState = LOW;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long buttonPressStartTime = 0;
bool longPressTriggered = false;

// Прототипы функций, которые используются только внутри этого файла
void drawI2CFaultScreen(const char* line1, const char* line2, const char* line3);
void drawContour1Screen();
void drawContour2Screen();
void drawSystemScreen();
void drawWifiAPScreen();


void initializeHardware() {
    pinMode(BUTTON_PIN, INPUT_PULLDOWN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    Serial.println("Scanning I2C bus...");
    Wire.beginTransmission(PCF8574_INPUTS_ADDR);
    if (Wire.endTransmission() == 0) { isInputExpanderAvailable = true; Serial.println("Input Expander (0x22) ... ONLINE"); } else { Serial.println("Input Expander (0x22) ... OFFLINE"); }
    Wire.beginTransmission(RELAY_I2C_ADDR);
    if (Wire.endTransmission() == 0) { isRelayExpanderAvailable = true; Serial.println("Relay Expander (0x24) ... ONLINE"); } else { Serial.println("Relay Expander (0x24) ... OFFLINE"); }
    Wire.beginTransmission(OLED_ADDR);
    if (Wire.endTransmission() == 0) { isDisplayAvailable = true; Serial.println("OLED Display (0x3C) ... ONLINE"); } else { Serial.println("OLED Display (0x3C) ... OFFLINE"); }
    if (rtc.begin()) { isRtcAvailable = true; Serial.println("RTC DS3231 ... ONLINE"); } else { Serial.println("RTC DS3231 ... OFFLINE"); }

    if (isDisplayAvailable) {
        u8g2.begin();
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_6x10_tf);
            u8g2.drawStr(0, 12, "WWT-A8 Controller");
            u8g2.drawStr(0, 26, "System starting...");
        } while (u8g2.nextPage());
        delay(2000);
        displayOn = true;
        currentScreen = 1;
        lastDisplayActivityTime = millis();
        updateDisplay();
    } else {
        delay(2000);
    }

    updateRelays(); // Устанавливаем начальное состояние реле
}

void loadNvsSettings() {
    prefs.begin("owmap", false);
    prefsProfiles.begin("profiles", false);
    prefsParams.begin("params", false);
    prefsGeneral.begin("general", false);
    globalPumpEnableMask = prefsGeneral.getUChar("pumpEnableMask", 0b1111);
}

void manageI2CDevices() {
    if (isDisplayAvailable && displayErrorCounter >= I2C_ERROR_THRESHOLD) isDisplayAvailable = false;
    if (isRelayExpanderAvailable && relayErrorCounter >= I2C_ERROR_THRESHOLD) isRelayExpanderAvailable = false;
    if (isInputExpanderAvailable && inputErrorCounter >= I2C_ERROR_THRESHOLD) isInputExpanderAvailable = false;
    if (isRtcAvailable && rtcErrorCounter >= I2C_ERROR_THRESHOLD) isRtcAvailable = false;
    
    if ((!isDisplayAvailable || !isRelayExpanderAvailable || !isInputExpanderAvailable || !isRtcAvailable) && (millis() - lastI2CRecoveryAttempt > I2C_RECOVERY_INTERVAL)) {
        lastI2CRecoveryAttempt = millis();
        if (!isDisplayAvailable) {
            Wire.beginTransmission(OLED_ADDR);
            if (Wire.endTransmission() == 0) { isDisplayAvailable = true; displayErrorCounter = 0; u8g2.begin(); }
        }
        if (!isRelayExpanderAvailable) {
            Wire.beginTransmission(RELAY_I2C_ADDR);
            if (Wire.endTransmission() == 0) {
                isRelayExpanderAvailable = true; relayErrorCounter = 0;
                pidController1.integralSum = 0; pidController2.integralSum = 0;
                pumpLogic1.state = S_IDLE; pumpLogic2.state = S_IDLE;
            }
        }
        if (!isInputExpanderAvailable) {
            Wire.beginTransmission(PCF8574_INPUTS_ADDR);
            if (Wire.endTransmission() == 0) { isInputExpanderAvailable = true; inputErrorCounter = 0; }
        }
        if (!isRtcAvailable) {
            if (rtc.begin()) { isRtcAvailable = true; rtcErrorCounter = 0; }
        }
    }
}

void handleButton() {
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == HIGH) {
                buttonPressStartTime = millis();
                longPressTriggered = false;
            } else { // Кнопку відпустили
                if (!longPressTriggered) { // Це коротке натискання
                    lastDisplayActivityTime = millis();
                    if (isDisplayAvailable) {
                        if (!displayOn) {
                            displayOn = true;
                            u8g2.setPowerSave(0);
                            currentScreen = 1;
                        } else {
                            currentScreen++;
                            if (currentScreen > 3) {
                                currentScreen = 1;
                            }
                        }
                    }
                }
            }
        }
    }
    lastButtonState = reading;
    if (buttonState == HIGH && !longPressTriggered && (millis() - buttonPressStartTime > 5000)) {
        longPressTriggered = true;
        if (!apModeActive) {
            apModeActive = true;
            WiFi.softAPConfig(apIP, apGW, apMASK);
            WiFi.softAP(AP_SSID, AP_PASS);
            server.begin();
            if (isDisplayAvailable) {
                displayOn = true;
                u8g2.setPowerSave(0);
            }
            currentScreen = 4;
            apModeStartTime = millis();
            lastDisplayActivityTime = millis();
        }
    }
}

void handleWifiAndServer() {
    if (apModeActive) {
        if (millis() - apModeStartTime > apTimeout) {
            apModeActive = false;
            currentScreen = 1;
            lastDisplayActivityTime = millis();
            WiFi.softAPdisconnect(true);
            server.stop();
        }
        server.handleClient();
    }
}

void checkDisplayTimeout() {
    if (isDisplayAvailable && displayOn && millis() - lastDisplayActivityTime > displayTimeout) {
        displayOn = false;
        u8g2.setPowerSave(1);
    }
}

void updateRelays() {
    if (!isRelayExpanderAvailable) return;
    Wire.beginTransmission(RELAY_I2C_ADDR);
    Wire.write(relayStates);
    if (Wire.endTransmission() != 0) {
        if (relayErrorCounter < 255) relayErrorCounter++;
    } else {
        relayErrorCounter = 0;
    }
}

void triggerRelayPulse(int relayIndex, unsigned long duration) {
    if (relayIndex < 0 || relayIndex > 7) return;
    int partnerIndex = relayIndex ^ 1;
    if ((pulseEndTimes[partnerIndex] > 0 && (long)(millis() - pulseEndTimes[partnerIndex]) < 0) || (pulseEndTimes[relayIndex] > 0 && (long)(millis() - pulseEndTimes[relayIndex]) < 0)) {
        return;
    }
    bitClear(relayStates, relayIndex);
    updateRelays();
    pulseEndTimes[relayIndex] = millis() + duration;
}

void checkRelayPulses() {
    bool relaysChanged = false;
    for (int i = 0; i < 8; i++) {
        if (pulseEndTimes[i] != 0 && (long)(millis() - pulseEndTimes[i]) >= 0) {
            bitSet(relayStates, i);
            pulseEndTimes[i] = 0;
            relaysChanged = true;
        }
    }
    static uint8_t lastRelayStates = 0xFF;
    if (relayStates != lastRelayStates || relaysChanged) {
        updateRelays();
        lastRelayStates = relayStates;
    }
}

// --- Функции отрисовки для OLED ---
// (Они остаются здесь, т.к. тесно связаны с железом - дисплеем)

const char* getPumpStatusStringOLED(PumpStatus status) {
    switch (status) {
        case S_WORKING: return "WORK";
        case S_ALARM:   return "ALARM";
        case S_REPAIR:  return "REPAIR";
        case S_OK:      
        default:        return "OK";
    }
}

void drawI2CFaultScreen(const char* line1, const char* line2, const char* line3) {
    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(0, 12, "SYSTEM ALARM");
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(0, 32, line1);
        u8g2.drawStr(0, 48, line2);
        u8g2.drawStr(0, 62, line3);
    } while (u8g2.nextPage());
}

void drawContour1Screen() {
    char buffer[32];
    u8g2.firstPage();
    do {
        String profileId = getProfileId(1);
        int tileIdx = tileIndexById(profileId);
        if(tileIdx < 0) tileIdx = TILE_CUSTOM_6;
        
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(0, 12, profileId.substring(0, 5).c_str());

        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(110, 10, "1/3");

        const TileDef& tile = getTile(tileIdx);
        bool tpod_al, tinv_al;
        float tpod = getTempByVar(tile.TPOD, tpod_al);
        float tinv = getTempByVar(tile.TINV, tinv_al);
        
        bool isComfort = false;
        float comfortReduction = 0.0f;
        float tzad = calculateSetpoint(1, isComfort, comfortReduction);

        sprintf(buffer, "Tsup: %s%s", tpod_al ? "AL" : String(tpod, 1).c_str(), tpod_al ? "" : " C");
        u8g2.drawStr(0, 24, buffer);
        sprintf(buffer, "Tret: %s%s", tinv_al ? "AL" : String(tinv, 1).c_str(), tinv_al ? "" : " C");
        u8g2.drawStr(0, 34, buffer);

        if (pumpLogic1.summer_mode_active) {
            sprintf(buffer, "Tset: SUMMER");
        } else if (isComfort) {
            sprintf(buffer, "Tset: %sC(%.1f)", isnan(tzad) ? "--.-" : String(tzad, 1).c_str(), comfortReduction);
        } else {
            sprintf(buffer, "Tset: %s C", isnan(tzad) ? "--.-" : String(tzad, 1).c_str());
        }
        u8g2.drawStr(0, 44, buffer);
        
        const char* p1_status = getPumpStatusStringOLED(pumpLogic1.pumps[0].status);
        const char* p2_status = getPumpStatusStringOLED(pumpLogic1.pumps[1].status);
        if (pumpLogic1.state == S_ALL_PUMPS_ALARM) {
             sprintf(buffer, "PUMPS: ALARM");
        } else {
             sprintf(buffer, "P1:%s  P2:%s", p1_status, p2_status);
        }
        u8g2.drawStr(0, 54, buffer);
        
        sprintf(buffer, "DryRun: %s", dry_run_state_stable ? "OK" : "ALARM");
        u8g2.drawStr(0, 64, buffer);

    } while (u8g2.nextPage());
}


void drawContour2Screen() {
    char buffer[32];
    u8g2.firstPage();
    do {
        String profileId = getProfileId(2);
        int tileIdx = tileIndexById(profileId);
        if(tileIdx < 0) tileIdx = TILE_CUSTOM_6;

        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(0, 12, profileId.substring(0, 5).c_str());

        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(110, 10, "2/3");

        const TileDef& tile = getTile(tileIdx);
        bool tpod_al, tinv_al;
        float tpod = getTempByVar(tile.TPOD, tpod_al);
        float tinv = getTempByVar(tile.TINV, tinv_al);
        
        bool isComfort = false;
        float comfortReduction = 0.0f;
        float tzad = calculateSetpoint(2, isComfort, comfortReduction);

        sprintf(buffer, "Tsup: %s%s", tpod_al ? "AL" : String(tpod, 1).c_str(), tpod_al ? "" : " C");
        u8g2.drawStr(0, 24, buffer);
        sprintf(buffer, "Tret: %s%s", tinv_al ? "AL" : String(tinv, 1).c_str(), tinv_al ? "" : " C");
        u8g2.drawStr(0, 34, buffer);
        
        if (pumpLogic2.summer_mode_active) {
            sprintf(buffer, "Tset: SUMMER");
        } else if (isComfort) {
            sprintf(buffer, "Tset: %sC(%.1f)", isnan(tzad) ? "--.-" : String(tzad, 1).c_str(), comfortReduction);
        } else {
            sprintf(buffer, "Tset: %s C", isnan(tzad) ? "--.-" : String(tzad, 1).c_str());
        }
        u8g2.drawStr(0, 44, buffer);

        const char* p1_status = getPumpStatusStringOLED(pumpLogic2.pumps[0].status);
        const char* p2_status = getPumpStatusStringOLED(pumpLogic2.pumps[1].status);
        if (pumpLogic2.state == S_ALL_PUMPS_ALARM) {
             sprintf(buffer, "PUMPS: ALARM");
        } else {
             sprintf(buffer, "P1:%s  P2:%s", p1_status, p2_status);
        }
        u8g2.drawStr(0, 54, buffer);

        sprintf(buffer, "DryRun: %s", dry_run_state_2_stable ? "OK" : "ALARM");
        u8g2.drawStr(0, 64, buffer);
        
    } while (u8g2.nextPage());
}

void drawSystemScreen() {
    char buffer[32];
    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(0, 12, "System");

        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(110, 10, "3/3");
        
        if (isRtcAvailable) {
            DateTime now = rtc.now();
            const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            sprintf(buffer, "Time: %02d:%02d (%s)", now.hour(), now.minute(), days[now.dayOfTheWeek()]);
            u8g2.drawStr(0, 26, buffer);
        } else {
            u8g2.drawStr(0, 26, "TIME: OFFLINE");
        }
        
        bool tn_al, t1_al, t2_al;
        float tn = getTempByVar("Tn", tn_al);
        float t1 = getTempByVar("T1", t1_al);
        float t2 = getTempByVar("T2", t2_al);
        
        sprintf(buffer, "T out: %s%s", tn_al ? "AL" : String(tn, 1).c_str(), tn_al ? "" : " C");
        u8g2.drawStr(0, 40, buffer);
        
        sprintf(buffer, "T net: %s / %s C", t1_al ? "AL" : String(t1, 1).c_str(), t2_al ? "AL" : String(t2, 1).c_str());
        u8g2.drawStr(0, 52, buffer);
        
        if (!dry_run_state_stable || !dry_run_state_2_stable) {
            u8g2.drawStr(0, 64, "ALARM: DRY RUN");
        }

    } while (u8g2.nextPage());
}

void drawWifiAPScreen() {
    char buffer[32];
    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.drawStr(0, 12, "Wi-Fi AP");
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(0, 24, "SETUP MODE");
        sprintf(buffer, "SSID: %s", AP_SSID);
        u8g2.drawStr(0, 40, buffer);
        sprintf(buffer, "IP: %s", apIP.toString().c_str());
        u8g2.drawStr(0, 52, buffer);
        sprintf(buffer, "Clients: %d", WiFi.softAPgetStationNum());
        u8g2.drawStr(0, 64, buffer);
    } while (u8g2.nextPage());
}

void updateDisplay() {
    if (!isDisplayAvailable || !displayOn) return;

    Wire.beginTransmission(OLED_ADDR);
    if (Wire.endTransmission() != 0) {
        if (displayErrorCounter < 255) displayErrorCounter++;
        return;
    } else {
        displayErrorCounter = 0;
    }
    
    if (!isRelayExpanderAvailable) {
        drawI2CFaultScreen("RELAY BOARD FAULT", "(I2C)", "CONTROL STOPPED");
        return;
    }
    if (!isInputExpanderAvailable) {
        drawI2CFaultScreen("INPUT BOARD FAULT", "(I2C)", "BLIND MODE ACTIVE");
        return;
    }
    
    switch(currentScreen) {
        case 1: drawContour1Screen(); break;
        case 2: drawContour2Screen(); break;
        case 3: drawSystemScreen(); break;
        case 4: drawWifiAPScreen(); break;
        default: break;
    }
}
