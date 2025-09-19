// =================================================================================
// File:         src/definitions.cpp
// Description:  Определение всех глобальных переменных и констант, 
//               объявленных в config.h.
// =================================================================================

#include "config.h"
#include "definitions.h"

// --- Секция 1: Определения константных массивов (ВАШ СУЩЕСТВУЮЩИЙ КОД) ---

const TileDef TILES[6] = {
  { "CO_1",     "СО_1",       "T11", "T21", "Tr",  "Коеф. зміщеня графіку", 1.00f },
  { "GVP_1",    "ГВП_1",      "T31", "T41", "Tr3", "Завдання ГВП",          55.0f },
  { "CO_2",     "СО_2",       "T12", "T22", "Tr2", "Коеф. зміщеня графіку", 1.00f },
  { "GVP_2",    "ГВП_2",      "T32", "T42", "Tr4", "Завдання ГВП",          55.0f },
  { "CUSTOM_5", "Кнопка 5",   "",    "",    "",    "", 0.0f },
  { "CUSTOM_6", "Кнопка 6",   "",    "",    "",    "", 0.0f }
};

const char* OW_VARS[OW_VAR_COUNT] = {
  "Tn","T1","T2","T11","T12","T21","T22","T31","T41","T32","T42"
};

// Проверка времени компиляции, чтобы убедиться, что константа соответствует размеру
static_assert(sizeof(OW_VARS)/sizeof(OW_VARS[0]) == OW_VAR_COUNT, "OW_VAR_COUNT mismatch!");


// --- Секция 2: Определение глобальных объектов и переменных (НОВЫЙ КОД) ---

// Глобальные объекты для работы с NVS (памятью)
Preferences prefs;
Preferences prefsParams;
Preferences prefsProfiles;
Preferences prefsGeneral;

// Глобальные объекты устройств
RTC_DS3231 rtc;
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Глобальные объекты для логики
PIDController pidController1;
PIDController pidController2;

ContourPumpLogic pumpLogic1;
ContourPumpLogic pumpLogic2;

WebServer server(80); // <-- ДОБАВИТЬ

// --- Глобальные переменные состояния ---
uint8_t globalPumpEnableMask = 0b1111;

// UI and Mode State // <-- ДОБАВИТЬ ЭТОТ БЛОК
bool displayOn = false;
int currentScreen = 1;
unsigned long lastDisplayActivityTime = 0;
bool apModeActive = false;
unsigned long apModeStartTime = 0;

// Флаги доступности I2C устройств
bool isDisplayAvailable = false;
bool isRelayExpanderAvailable = false;
bool isInputExpanderAvailable = false;
bool isRtcAvailable = false;

// Состояние реле и таймеры импульсов
uint8_t relayStates = 0xFF; // Изначально все реле выключены (логика инверсная)
unsigned long pulseEndTimes[8] = {0};

// Стабильные (отфильтрованные) состояния входов
int contour1_mode_stable = 0, contour1_mode_last = 0, contour1_mode_count = 0;
int dry_run_state_stable = 1, dry_run_state_last = 1, dry_run_state_count = 0;
int pump1_state_stable = 0, pump1_state_last = 0, pump1_state_count = 0;
int pump2_state_stable = 0, pump2_state_last = 0, pump2_state_count = 0;
int contour2_mode_stable = 0, contour2_mode_last = 0, contour2_mode_count = 0;
int dry_run_state_2_stable = 1, dry_run_state_2_last = 1, dry_run_state_2_count = 0;
int pump3_state_stable = 0, pump3_state_last = 0, pump3_state_count = 0;
int pump4_state_stable = 0, pump4_state_last = 0, pump4_state_count = 0;

// Переменные для отслеживания предыдущего состояния режима
bool wasInAutoModeContour1 = false;
bool wasInAutoModeContour2 = false;

// --- Константы --- // <-- ДОБАВИТЬ ЭТОТ БЛОК
const unsigned long displayTimeout = 300000; // 5 минут
const unsigned long apTimeout = 300000;      // 5 минут
const char* AP_SSID = "WWT-A8";
const char* AP_PASS = "12345678";
IPAddress apIP(192, 168, 4, 1);
IPAddress apGW(192, 168, 4, 1);
IPAddress apMASK(255, 255, 255, 0);