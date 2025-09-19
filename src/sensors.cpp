

// =================================================================================
// File:         src/sensors.cpp
// Description:  Реализация функций для работы с датчиками 1-Wire (DS18B20)
//               и дискретными входами (PCF8574).
// =================================================================================

#include "sensors.h"

// --- Локальные объекты и переменные для этого модуля ---
static OneWire oneWire(OW_PIN);
static DallasTemperature ds18(&oneWire);

// Массив для хранения состояний всех датчиков
struct SensorState {
    float temperature = DEVICE_DISCONNECTED_C;
    unsigned long lastUpdateTime = 0;
    bool is_alarm = true;
};
static SensorState sensorStates[OW_VAR_COUNT];


// --- Реализация функций ---

void initializeSensors() {
    ds18.begin();
    ds18.setWaitForConversion(false); // Неблокирующий режим
    ds18.requestTemperatures(); // Первый запрос
}

void updateAllSensorReadings() {
    static unsigned long lastRequestTime = 0;
    const unsigned long REQUEST_INTERVAL = 2000;

    if (millis() - lastRequestTime > REQUEST_INTERVAL) {
        lastRequestTime = millis();

        uint8_t addr[8];
        oneWire.reset_search();

        // Сначала сбрасываем флаги тревоги для всех переменных
        for (size_t i = 0; i < OW_VAR_COUNT; ++i) {
            sensorStates[i].is_alarm = true;
        }

        // Ищем все устройства на шине
        while (oneWire.search(addr)) {
            if (OneWire::crc8(addr, 7) != addr[7]) continue;
            if (addr[0] != 0x28) continue; // Только DS18B20

            String romStr = owAddrToString(addr);
            String varName = nvsFindVarByRom(romStr);

            if (varName.length() > 0) {
                float tempC = ds18.getTempC(addr);
                
                for (size_t i = 0; i < OW_VAR_COUNT; ++i) {
                    if (varName.equals(OW_VARS[i])) {
                        if (tempC != DEVICE_DISCONNECTED_C && tempC > -55.0f) {
                            sensorStates[i].temperature = tempC;
                            sensorStates[i].is_alarm = false;
                        } else {
                            sensorStates[i].temperature = DEVICE_DISCONNECTED_C;
                            sensorStates[i].is_alarm = true;
                        }
                        sensorStates[i].lastUpdateTime = millis();
                        break;
                    }
                }
            }
        }
        ds18.requestTemperatures(); // Запрашиваем следующее измерение
    }
}

// Глобальная функция для получения температуры по имени переменной
// (может быть вызвана из любого другого файла)
float getTempByVar(const char* varName, bool& isAlarm) {
    for (size_t i = 0; i < OW_VAR_COUNT; ++i) {
        if (strcmp(varName, OW_VARS[i]) == 0) {
            isAlarm = sensorStates[i].is_alarm;
            return sensorStates[i].temperature;
        }
    }
    isAlarm = true;
    return DEVICE_DISCONNECTED_C;
}


void readDigitalInputs() {
    if (!isInputExpanderAvailable) return;

    Wire.requestFrom(PCF8574_INPUTS_ADDR, (uint8_t)1);
    if (Wire.available()) {
        byte inputs = Wire.read();
        
        auto filter = [](int& stable, int& last, int& count, bool current) {
            if (current == last) {
                if (count < 5) count++;
            } else {
                count = 0;
            }
            if (count >= 5) stable = current;
            last = current;
        };

        filter(contour1_mode_stable, contour1_mode_last, contour1_mode_count, bitRead(inputs, 0));
        filter(dry_run_state_stable, dry_run_state_last, dry_run_state_count, bitRead(inputs, 1));
        filter(pump1_state_stable, pump1_state_last, pump1_state_count, bitRead(inputs, 2));
        filter(pump2_state_stable, pump2_state_last, pump2_state_count, bitRead(inputs, 3));
        filter(contour2_mode_stable, contour2_mode_last, contour2_mode_count, bitRead(inputs, 4));
        filter(dry_run_state_2_stable, dry_run_state_2_last, dry_run_state_2_count, bitRead(inputs, 5));
        filter(pump3_state_stable, pump3_state_last, pump3_state_count, bitRead(inputs, 6));
        filter(pump4_state_stable, pump4_state_last, pump4_state_count, bitRead(inputs, 7));
    }
}


// --- Вспомогательные функции для работы с NVS и адресами 1-Wire ---

String owAddrToString(const uint8_t addr[8]) {
  char buf[24];
  sprintf(buf, "%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
  return String(buf);
}

bool owStringToAddr(const String& romStr, uint8_t addr[8]) {
    return sscanf(romStr.c_str(), "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", 
                  &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5], &addr[6], &addr[7]) == 8;
}

String nvsFindVarByRom(const String& rom) {
  prefs.begin("owmap", true);
  for (size_t i=0; i < OW_VAR_COUNT; i++) {
    String cur = prefs.getString(OW_VARS[i], String());
    if (cur.length() && cur == rom) {
      prefs.end();
      return String(OW_VARS[i]);
    }
  }
  prefs.end();
  return String();
}

bool nvsClearVar(const String& varName) {
  if (!owIsKnownVar(varName)) return false;
  prefs.begin("owmap", false);
  bool res = prefs.remove(varName.c_str());
  prefs.end();
  return res;
}

bool nvsBindVarToRom(const String& varName, const String& rom, String* clearedVarOut, String* replacedRomOut, String* errMsg) {
  if (!owIsKnownVar(varName)) { if (errMsg) *errMsg="unknown var"; return false; }
  uint8_t addr[8];
  if (!owStringToAddr(rom, addr)) { if (errMsg) *errMsg="bad rom format"; return false; }
  if (addr[0] != 0x28) { if (errMsg) *errMsg="not DS18B20 family"; return false; }

  prefs.begin("owmap", false);
  String occupiedBy = nvsFindVarByRom(rom);
  if (occupiedBy.length() && occupiedBy != varName) {
    prefs.remove(occupiedBy.c_str());
    if (clearedVarOut) *clearedVarOut = occupiedBy;
  }
  prefs.putString(varName.c_str(), rom);
  prefs.end();
  return true;
}

bool owIsKnownVar(const String& v) {
  for (size_t i=0; i < OW_VAR_COUNT; i++) {
    if (v == OW_VARS[i]) return true;
  }
  return false;
}

