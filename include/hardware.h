#ifndef HARDWARE_H
#define HARDWARE_H

#include "config.h" // <-- ДОБАВЬТЕ ЭТУ СТРОКУ

// Объявления (прототипы) функций, которые мы реализуем в hardware.cpp
// Теперь main.cpp будет знать, что эти функции существуют

void initializeHardware();
void loadNvsSettings();
void manageI2CDevices();
void handleButton();
void handleWifiAndServer();
void checkDisplayTimeout();
void checkRelayPulses();
void updateDisplay();
void updateRelays();
void triggerRelayPulse(int relayIndex, unsigned long duration);
void setRelay(int relayIndex, bool on);


#endif // HARDWARE_H
