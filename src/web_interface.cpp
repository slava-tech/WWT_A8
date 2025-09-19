// =================================================================================
// File:         src/web_interface.cpp
// Description:  Реализация всех функций веб-интерфейса. Содержит полный код
//               веб-страницы (HTML/CSS/JS) и все обработчики API-запросов.
// =================================================================================

#include "web_interface.h"
#include "sensors.h" 
#include "hardware.h" 
#include "web_server.h"
#include "utils.h"

// --- Секция 12: HTML, CSS, JavaScript для веб-интерфейса ---
// Здесь находится полный код вашей оригинальной веб-страницы.
const char index_html[] PROGMEM = R"====(
// ... HTML код ...
)====";

// --- Прототипы всех обработчиков ---
void handleRoot();
void handleNotFound();
void handleOwScan();
void handleOwStatus();
void handleOwBind();
void handleVarsStatus();
void handleSystemStatus();
void handleContourProfileGET();
void handleContourProfilePOST();
void handleMainStatus();
void handleRelayPulse();
void handleParamSave();
void handleSettingsLoad();
void handleSettingsSave();
void handleTimeSet();

// --- Реализация обработчиков ---

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleNotFound(){
  server.send(404, "text/plain", "Not found");
}

// ... (остальные обработчики handleOwScan, handleOwStatus и т.д.)

// --- Инициализация Веб-интерфейса ---

void initializeWebInterface() {
    // Регистрируем все обработчики, которые ожидает ваш JS
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/main/status", HTTP_GET, handleMainStatus);
    server.on("/api/relay/pulse", HTTP_POST, handleRelayPulse);
    server.on("/api/contour/param", HTTP_POST, handleParamSave);
    server.on("/api/settings/load", HTTP_GET, handleSettingsLoad);
    server.on("/api/settings/save", HTTP_POST, handleSettingsSave);
    server.on("/api/time/set", HTTP_POST, handleTimeSet);
    server.on("/api/ow/scan", HTTP_POST, handleOwScan);
    server.on("/api/ow/status", HTTP_GET, handleOwStatus);
    server.on("/api/ow/bind", HTTP_POST, handleOwBind);
    server.on("/api/vars/status", HTTP_GET, handleVarsStatus);
    server.on("/api/system/status", HTTP_GET, handleSystemStatus);
    server.on("/api/contour/profile", HTTP_GET, handleContourProfileGET);
    server.on("/api/contour/profile", HTTP_POST, handleContourProfilePOST);

    server.onNotFound(handleNotFound);
}
