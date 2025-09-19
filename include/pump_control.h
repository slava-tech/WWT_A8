

// =================================================================================
// File:         include/pump_control.h
// Description:  Объявления функций для логики управления насосами.
// =================================================================================

#ifndef PUMP_CONTROL_H
#define PUMP_CONTROL_H

#include "config.h"

// Главная функция, запускающая логику для одного из контуров
void runPumpLogic(int contourNum);

// Вспомогательная функция для получения статуса насоса в виде строки
const char* getPumpStatusString(PumpStatus status);

#endif // PUMP_CONTROL_H