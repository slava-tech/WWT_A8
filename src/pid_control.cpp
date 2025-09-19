// =================================================================================
// File:         src/pid_control.cpp
// Description:  Реализация логики ПИ-регулятора и функции расчета уставки.
// =================================================================================

#include "pid_control.h"
#include "hardware.h"
#include "sensors.h"
#include "web_server.h"
#include "utils.h"

// --- Основная функция логики ПИ-регулятора ---

void runPIDLogic(int contourNum) {
    PIDController& pid = (contourNum == 1) ? pidController1 : pidController2;
    int relay_plus = (contourNum == 1) ? 2 : 6;
    int relay_minus = (contourNum == 1) ? 1 : 5;

    unsigned long currentTime = millis();
    if (currentTime - pid.lastRunTime < 1000) { // Запускаем не чаще раза в секунду
        return;
    }
    pid.lastRunTime = currentTime;

    bool isComfort, isSummer;
    float comfortReduction;
    float setpoint = calculateSetpoint(contourNum, isComfort, comfortReduction);

    String profileId = getProfileId(contourNum);
    int tileIdx = tileIndexById(profileId);
    if (tileIdx < 0) return;
    const TileDef& tile = getTile(tileIdx);

    bool tpod_alarm;
    float tpod = getTempByVar(tile.TPOD, tpod_alarm);

    if (isnan(setpoint) || tpod_alarm) {
        pid.integralSum = 0; // Сбрасываем интеграл, если нет уставки или датчик в аварии
        pid.lastDirection = 0;
        return;
    }

    float error = setpoint - tpod;

    prefsGeneral.begin("general", true);
    String pi_prefix = (contourNum == 1) ? "pi1_" : "pi2_";
    float Kp = prefsGeneral.getFloat((pi_prefix + "Kp").c_str(), 0.1f);
    float Ki = prefsGeneral.getFloat((pi_prefix + "Ki").c_str(), 0.01f);
    float Ti = prefsGeneral.getFloat((pi_prefix + "Ti").c_str(), 10.0f);
    prefsGeneral.end();

    pid.integralSum += error;
    // Ограничение интегральной суммы (anti-windup)
    if (pid.integralSum > 100.0f) pid.integralSum = 100.0f;
    if (pid.integralSum < -100.0f) pid.integralSum = -100.0f;

    float output = Kp * error + Ki * pid.integralSum;

    int direction = 0;
    if (output > 0.5) direction = 1; // Открыть
    else if (output < -0.5) direction = -1; // Закрыть

    if (direction != 0 && (currentTime - pid.lastImpulseTime) > (unsigned long)(Ti * 1000)) {
        int pulse_duration = 1000; // Длительность импульса 1 секунда
        triggerRelayPulse(direction == 1 ? relay_plus : relay_minus, pulse_duration);
        pid.lastImpulseTime = currentTime;
        pid.lastDirection = direction;
    } else {
        pid.lastDirection = 0;
    }
}


// --- Функция расчета уставки (целевой температуры) ---

float calculateSetpoint(int contourNum, bool& isComfortActive, float& comfortReduction) {
    isComfortActive = false;
    comfortReduction = 0.0f;

    String profileId = getProfileId(contourNum);
    int tileIdx = tileIndexById(profileId);
    if (tileIdx < 0) return NAN;
    
    const TileDef& tile = getTile(tileIdx);
    bool tn_alarm, tpod_alarm;
    float tn = getTempByVar("Tn", tn_alarm);
    float tpod = getTempByVar(tile.TPOD, tpod_alarm);

    float baseSetpoint = NAN;

    if (profileId.startsWith("GVP")) {
        if (!tpod_alarm) {
            prefsParams.begin("params", true);
            baseSetpoint = prefsParams.getFloat(tile.TZAD, tile.defaultValue);
            prefsParams.end();
        }
    } else if (profileId.startsWith("CO")) {
        prefsParams.begin("params", true);
        float coefficient = prefsParams.getFloat(tile.TZAD, tile.defaultValue);
        prefsParams.end();

        prefsGeneral.begin("general", true);
        String jsonPoints = prefsGeneral.getString("curvePoints", "[]");
        float summerCutoffTemp = prefsGeneral.getFloat("summerCutoff", 20.0f);
        prefsGeneral.end();

        if (!tn_alarm && tn >= summerCutoffTemp) {
            return NAN; // Летний режим
        }

        if (!tn_alarm) {
            // Расчет по кривой
            StaticJsonDocument<256> doc;
            deserializeJson(doc, jsonPoints);
            JsonArray array = doc.as<JsonArray>();
            if (array.size() == 5) {
                float x[5], y[5];
                for(int i=0; i<5; i++) { x[i] = array[i]["x"]; y[i] = array[i]["y"]; }
                
                // Сортировка точек на случай, если они в NVS хранятся не по порядку
                for(int i=0; i<4; i++) {
                    for(int j=0; j<4-i; j++) {
                        if(x[j] > x[j+1]) {
                            float tempX = x[j]; x[j] = x[j+1]; x[j+1] = tempX;
                            float tempY = y[j]; y[j] = y[j+1]; y[j+1] = tempY;
                        }
                    }
                }
                
                // Простая линейная интерполяция
                if (tn <= x[0]) baseSetpoint = y[0];
                else if (tn >= x[4]) baseSetpoint = y[4];
                else {
                    for (int i = 0; i < 4; i++) {
                        if (tn >= x[i] && tn <= x[i+1]) {
                            baseSetpoint = y[i] + (tn - x[i]) * (y[i+1] - y[i]) / (x[i+1] - x[i]);
                            break;
                        }
                    }
                }
                if (!isnan(baseSetpoint)) {
                    baseSetpoint *= coefficient;
                }
            }
        }
    }

    // Логика комфортного режима (если RTC доступен)
    if (isRtcAvailable && !isnan(baseSetpoint)) {
        prefsGeneral.begin("general", true);
        bool timeWasSet = prefsGeneral.getBool("timeWasSet", false);
        String comfortKey = (contourNum == 1) ? "comfort1" : "comfort2";
        String comfortJsonStr = prefsGeneral.getString(comfortKey.c_str(), "{}");
        prefsGeneral.end();

        if (timeWasSet) {
            StaticJsonDocument<512> doc;
            deserializeJson(doc, comfortJsonStr);

            if (doc["enabled"] | false) {
                DateTime now = rtc.now();
                int currentDow = now.dayOfTheWeek();
                int currentMinutes = now.hour() * 60 + now.minute();
                
                JsonArray days = doc["days"];
                bool todayIsActive = false;
                for(int day : days) { if (day == currentDow) { todayIsActive = true; break; } }

                if (todayIsActive) {
                    JsonArray intervals = doc["intervals"];
                    for(JsonObject interval : intervals) {
                        String startStr = interval["start"] | "00:00";
                        String endStr = interval["end"] | "00:00";
                        int startMins = (startStr.substring(0,2).toInt() * 60) + startStr.substring(3,5).toInt();
                        int endMins = (endStr.substring(0,2).toInt() * 60) + endStr.substring(3,5).toInt();
                        if (endMins == 0 && startMins > 0) endMins = 1440;

                        if (currentMinutes >= startMins && currentMinutes < endMins) {
                            isComfortActive = true;
                            comfortReduction = interval["reduct"] | 0.0f;
                            return baseSetpoint + comfortReduction;
                        }
                    }
                }
            }
        }
    }
    
    return baseSetpoint;
}

