// =================================================================================
// File:         include/web_server.h
// Description:  Объявления функций для веб-сервера и работы с NVS.
// =================================================================================

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config.h"

// Настройка маршрутов и запуск веб-сервера
void setupWebServer();

// Отладочная функция для вывода NVS в Serial
void dumpNvsToSerial();

// Инициализация Веб-интерфейса
void initializeWebInterface();

#endif // WEB_SERVER_H


