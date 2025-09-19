// =================================================================================
// File:         include/sensors.h
// Description:  Объявления (прототипы) функций для работы с датчиками 1-Wire
//               и дискретными входами. Это "оглавление" для модуля sensors.
// =================================================================================

#ifndef SENSORS_H
#define SENSORS_H

#include "config.h" // Нужен для доступа к глобальным структурам и переменным

// Инициализация шины 1-Wire
void initializeSensors();

// Обновление всех показаний с датчиков DS18B20
void updateAllSensorReadings();

// Чтение и фильтрация состояния дискретных входов с PCF8574
void readDigitalInputs();

// --- Вспомогательные функции для работы с NVS и адресами 1-Wire ---

String owAddrToString(const uint8_t addr[8]);
bool owStringToAddr(const String& romStr, uint8_t addr[8]);
String nvsFindVarByRom(const String& rom);
bool nvsClearVar(const String& varName);
bool nvsBindVarToRom(const String& varName, const String& rom, String* clearedVarOut=nullptr, String* replacedRomOut=nullptr, String* errMsg=nullptr);
bool owIsKnownVar(const String& v);

// Объявление функции для получения температуры
float getTempByVar(const char* varName, bool& isAlarm);

#endif // SENSORS_H


