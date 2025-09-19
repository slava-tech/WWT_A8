// =================================================================================
// File:         include/config.h
// Description:  Главный конфигурационный файл проекта. Содержит все глобальные
//               объявления (extern), структуры, константы и подключения библиотек.
//               Это "карта" проекта, которая позволяет разным файлам (.cpp)
//               "видеть" общие переменные и функции.
// =================================================================================

#ifndef CONFIG_H
#define CONFIG_H

// --- Секция 1: Подключение библиотек ---
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <pgmspace.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <RTClib.h>

// --- Секция 1.1: Структуры данных ---

// Описание "Плитки" (режима работы)
struct TileDef {
  const char* id;
  const char* displayName;
  const char* TPOD;
  const char* TINV;
  const char* TZAD;
  const char* settingsLabel;
  float       defaultValue;
};

// Индексы для удобного доступа к плиткам
enum TileIndex : uint8_t {
  TILE_CO_1 = 0, TILE_GVP_1, TILE_CO_2, TILE_GVP_2, TILE_CUSTOM_5, TILE_CUSTOM_6
};

// Состояние ПИ-регулятора
struct PIDController {
    enum PIDPhase { PHASE_ACTIVE, PHASE_STANDBY };
    float integralSum = 0.0f;
    unsigned long lastRunTime = 0;
    unsigned long lastImpulseTime = 0;
    int impulseCounter = 0;
    int lastDirection = 0;
    unsigned long counterResetTime = 0;
    PIDPhase phase = PHASE_ACTIVE;
    unsigned long phaseChangeTime = 0;
    int standbyCycleCounter = 0;
};

// Состояние насоса
enum PumpStatus : uint8_t { S_OK, S_WORKING, S_ALARM, S_REPAIR };

// Состояния конечного автомата логики насосов
enum ContourLogicState : uint8_t { S_IDLE, S_START_DELAY, S_WAIT_FEEDBACK, S_NORMAL, S_CHANGEOVER_PAUSE, S_DRY_RUN_RECOVERY, S_ALL_PUMPS_ALARM };

struct PumpState {
    PumpStatus status = S_OK;
    unsigned long workStartTime = 0;
    unsigned long feedbackLossTime = 0;
};

// Состояние логики управления насосами для одного контура
struct ContourPumpLogic {
    ContourLogicState state = S_IDLE;
    int activePumpIndex = 0;
    unsigned long stateTimer = 0;
    PumpState pumps[2];
    int prev_mode = -1;
    int prev_dry_run_stable = -1;
    bool summer_mode_active = false;
    unsigned long dryRunAlarmStartTime = 0;
    bool dryRunAlarmPending = false;
};


// --- Секция 2: Глобальные объявления ---
// 'extern' означает, что эти переменные созданы в definitions.cpp,
// и мы просто сообщаем компилятору об их существовании.

// --- Секция 2.1: Константные данные ---
extern const TileDef TILES[6];

#define OW_VAR_COUNT 11 // Жестко задаем количество, чтобы использовать как константу
extern const char* OW_VARS[OW_VAR_COUNT]; // Используем константу для задания размера

// --- Секция 2.2: Конфигурация аппаратной части (пины, адреса) ---
#define RELAY_I2C_ADDR 0x24
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define PCF8574_INPUTS_ADDR 0x22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define BUTTON_PIN 34
#define OW_PIN 14 // Пин для 1-Wire

// --- Секция 2.3: Глобальные объекты ---
extern Preferences prefs;
extern Preferences prefsParams;
extern Preferences prefsProfiles;
extern Preferences prefsGeneral;

extern RTC_DS3231 rtc;

extern U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2;
extern PIDController pidController1;
extern PIDController pidController2;

extern ContourPumpLogic pumpLogic1;
extern ContourPumpLogic pumpLogic2;

extern WebServer server; // <-- ДОБАВИТЬ

// --- Секция 2.4: Глобальные переменные состояния ---
extern uint8_t globalPumpEnableMask;

// UI and Mode State // <-- ДОБАВИТЬ ЭТОТ БЛОК
extern bool displayOn;
extern int currentScreen;
extern unsigned long lastDisplayActivityTime;
extern bool apModeActive;
extern unsigned long apModeStartTime;

// Флаги доступности I2C устройств
extern bool isDisplayAvailable;
extern bool isRelayExpanderAvailable;
extern bool isInputExpanderAvailable;
extern bool isRtcAvailable;

// Состояние реле и таймеры импульсов
extern uint8_t relayStates;
extern unsigned long pulseEndTimes[8];

// Стабильные (отфильтрованные) состояния входов
extern int contour1_mode_stable, contour1_mode_last, contour1_mode_count;
extern int dry_run_state_stable, dry_run_state_last, dry_run_state_count;
extern int pump1_state_stable, pump1_state_last, pump1_state_count;
extern int pump2_state_stable, pump2_state_last, pump2_state_count;
extern int contour2_mode_stable, contour2_mode_last, contour2_mode_count;
extern int dry_run_state_2_stable, dry_run_state_2_last, dry_run_state_2_count;
extern int pump3_state_stable, pump3_state_last, pump3_state_count;
extern int pump4_state_stable, pump4_state_last, pump4_state_count;

// Переменные для отслеживания предыдущего состояния режима
extern bool wasInAutoModeContour1;
extern bool wasInAutoModeContour2;

// --- Секция 2.5: Константы --- // <-- ДОБАВИТЬ ЭТОТ БЛОК
extern const unsigned long displayTimeout;
extern const unsigned long apTimeout;
extern const char* AP_SSID;
extern const char* AP_PASS;
extern IPAddress apIP;
extern IPAddress apGW;
extern IPAddress apMASK;

#endif // CONFIG_H

