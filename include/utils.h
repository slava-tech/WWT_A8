// =================================================================================
// File:         include/utils.h
// Description:  Объявления общих вспомогательных функций, используемых
//               в разных частях проекта для избежания круговых зависимостей.
// =================================================================================

#ifndef UTILS_H
#define UTILS_H

#include "config.h"

// Функции для работы с профилями и "плитками"
String getProfileId(uint8_t cont);
int tileIndexById(const String& id);
const TileDef& getTile(uint8_t idx);

#endif // UTILS_H
