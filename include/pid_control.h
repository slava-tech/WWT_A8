

// =================================================================================
// File:         include/pid_control.h
// Description:  Объявления функций для ПИ-регулятора.
// =================================================================================

#ifndef PID_CONTROL_H
#define PID_CONTROL_H

#include "config.h"

// Запуск итерации ПИ-регулятора для указанного контура (1 или 2)
void runPIDLogic(int contourNum);

// Рассчитывает целевую температуру (уставку) для контура
float calculateSetpoint(int contourNum, bool& isComfortActive, float& comfortReduction);



#endif // PID_CONTROL_H


