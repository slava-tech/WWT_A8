// =================================================================================
// File:         src/main.cpp
// Description:  Главный файл проекта. Содержит функции setup() и loop()
//               и организует вызов всех остальных модулей.
// =================================================================================

#include "config.h"
#include "hardware.h"
#include "sensors.h"
#include "pid_control.h"
#include "pump_control.h"
#include "web_server.h"
#include <esp_task_wdt.h>

// --- Переменные для таймеров основного цикла ---
unsigned long lastInputReadTime = 0;
unsigned long lastPIDRunTime = 0;
unsigned long lastPumpLogicRunTime = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastTempRequestTime = 0;

const long inputReadInterval = 1000;
const long pidRunInterval = 1000;
const long pumpLogicRunInterval = 1000;
const long displayUpdateInterval = 500;
const long tempRequestInterval = 2000;

// --- Основные функции setup() и loop() ---

void setup() {
    Serial.begin(115200);
    delay(200);

    // Инициализация сторожевого таймера
    esp_task_wdt_init(60, true); // 60 секунд, перезагрузка при срабатывании
    esp_task_wdt_add(NULL);

    // Инициализация всего оборудования (пины, I2C, дисплей и т.д.)
    initializeHardware();

    // Загрузка настроек из энергонезависимой памяти (NVS)
    loadNvsSettings();

    // Инициализация шины 1-Wire и датчиков
    initializeSensors();

    // Настройка Wi-Fi и всех обработчиков веб-сервера
    setupWebServer();
    initializeWebInterface();

    // Для отладки: выводим все загруженные настройки в Serial
    dumpNvsToSerial();
}

void loop() {
    // Сбрасываем сторожевой таймер в начале каждого цикла
    esp_task_wdt_reset();

    // Обработка нажатий физической кнопки
    handleButton();
    
    // Управление режимом точки доступа Wi-Fi и обработка клиентов веб-сервера
    handleWifiAndServer();

    // Проверка статуса I2C устройств и попытка восстановления связи при сбое
    manageI2CDevices();

    unsigned long currentTime = millis();

    // Обновляем показания датчиков с заданным интервалом
    if (currentTime - lastTempRequestTime > tempRequestInterval) {
        lastTempRequestTime = currentTime;
        updateAllSensorReadings();
    }

    // Читаем состояние дискретных входов
    if (currentTime - lastInputReadTime >= inputReadInterval) {
        lastInputReadTime = currentTime;
        readDigitalInputs();
    }

    // Запускаем логику ПИ-регуляторов для обоих контуров
    if (currentTime - lastPIDRunTime >= pidRunInterval) {
        lastPIDRunTime = currentTime;
        runPIDLogic(1);
        runPIDLogic(2);
    }

    // Запускаем логику управления насосами для обоих контуров
    if (currentTime - lastPumpLogicRunTime >= pumpLogicRunInterval) {
        lastPumpLogicRunTime = currentTime;
        runPumpLogic(1);
        runPumpLogic(2);
    }

    // Обновляем информацию на OLED дисплее
    if (currentTime - lastDisplayUpdateTime > displayUpdateInterval) {
        lastDisplayUpdateTime = currentTime;
        updateDisplay();
    }

    // Проверяем, не пора ли выключить дисплей по таймауту
    checkDisplayTimeout();

    // Проверяем и завершаем активные импульсы на реле
    checkRelayPulses();
}

