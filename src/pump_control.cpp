// =================================================================================
// File:         src/pump_control.cpp
// Description:  Реализация конечного автомата для управления насосами.
//               Обрабатывает ротацию, аварии, сухой ход и обратную связь.
// =================================================================================

#include "pump_control.h"
#include "hardware.h"
#include "sensors.h"

// --- Вспомогательные константы (таймауты) ---
const unsigned long PUMP_START_DELAY = 5000;       // 5 секунд задержки перед стартом
const unsigned long PUMP_FEEDBACK_TIMEOUT = 10000; // 10 секунд на ожидание обратной связи
const unsigned long PUMP_CHANGEOVER_PAUSE = 3000;  // 3 секунды паузы при смене насоса
const unsigned long DRY_RUN_RECOVERY_TIME = 600000; // 10 минут на восстановление после сухого хода
const unsigned long DRY_RUN_ALARM_DELAY = 15000;   // 15 секунд задержки до срабатывания тревоги по сухому ходу

// --- Главная функция логики ---

void runPumpLogic(int contourNum) {
    // 1. Определяем, с каким контуром работаем, и получаем ссылки на его переменные
    ContourPumpLogic& logic = (contourNum == 1) ? pumpLogic1 : pumpLogic2;
    int mode_stable = (contourNum == 1) ? contour1_mode_stable : contour2_mode_stable;
    int dry_run_stable = (contourNum == 1) ? dry_run_state_stable : dry_run_state_2_stable;
    int p1_feedback = (contourNum == 1) ? pump1_state_stable : pump3_state_stable;
    int p2_feedback = (contourNum == 1) ? pump2_state_stable : pump4_state_stable;
    int p1_relay = (contourNum == 1) ? 3 : 7;
    int p2_relay = (contourNum == 1) ? 4 : 0;
    uint8_t p1_enable_bit = (contourNum == 1) ? 0 : 2;
    uint8_t p2_enable_bit = (contourNum == 1) ? 1 : 3;

    unsigned long currentTime = millis();
    int feedbacks[] = {p1_feedback, p2_feedback};
    int relays[] = {p1_relay, p2_relay};
    uint8_t enable_bits[] = {p1_enable_bit, p2_enable_bit};

    // 2. Проверка на летний режим (из старого проекта)
    bool tn_alarm;
    float tn = getTempByVar("Tn", tn_alarm);
    float summerCutoff = prefsGeneral.getFloat("summerCutoff", 20.0f);
    logic.summer_mode_active = !tn_alarm && (tn > summerCutoff);

    // 3. Логика сброса аварий
    if (logic.pumps[0].status == S_ALARM && (globalPumpEnableMask & (1 << enable_bits[0]))) logic.pumps[0].status = S_OK;
    if (logic.pumps[1].status == S_ALARM && (globalPumpEnableMask & (1 << enable_bits[1]))) logic.pumps[1].status = S_OK;

    // 4. Главный конечный автомат (state machine)
    switch (logic.state) {
        case S_IDLE:
            setRelay(p1_relay, false);
            setRelay(p2_relay, false);
            if (mode_stable == 1 && !logic.summer_mode_active) {
                logic.state = S_START_DELAY;
                logic.stateTimer = currentTime;
            }
            break;

        case S_START_DELAY:
            if (mode_stable == 0 || logic.summer_mode_active) {
                logic.state = S_IDLE;
            } else if (currentTime - logic.stateTimer >= PUMP_START_DELAY) {
                int nextPump = -1;
                if (logic.pumps[logic.activePumpIndex].status == S_OK) {
                    nextPump = logic.activePumpIndex;
                } else {
                    int otherPump = 1 - logic.activePumpIndex;
                    if (logic.pumps[otherPump].status == S_OK) nextPump = otherPump;
                }

                if (nextPump != -1) {
                    logic.activePumpIndex = nextPump;
                    setRelay(relays[logic.activePumpIndex], true);
                    logic.pumps[logic.activePumpIndex].workStartTime = currentTime;
                    logic.state = S_WAIT_FEEDBACK;
                    logic.stateTimer = currentTime;
                } else {
                    logic.state = S_ALL_PUMPS_ALARM;
                }
            }
            break;

        case S_WAIT_FEEDBACK:
            if (mode_stable == 0 || logic.summer_mode_active) {
                logic.state = S_IDLE;
            } else if (feedbacks[logic.activePumpIndex] == 1) {
                logic.pumps[logic.activePumpIndex].status = S_WORKING;
                logic.state = S_NORMAL;
            } else if (currentTime - logic.stateTimer >= PUMP_FEEDBACK_TIMEOUT) {
                logic.pumps[logic.activePumpIndex].status = S_ALARM;
                setRelay(relays[logic.activePumpIndex], false);
                logic.state = S_CHANGEOVER_PAUSE;
                logic.stateTimer = currentTime;
            }
            break;

        case S_NORMAL:
            if (mode_stable == 0 || logic.summer_mode_active) {
                logic.state = S_IDLE;
            } else if (feedbacks[logic.activePumpIndex] == 0) {
                logic.pumps[logic.activePumpIndex].feedbackLossTime = currentTime;
                logic.pumps[logic.activePumpIndex].status = S_ALARM;
                setRelay(relays[logic.activePumpIndex], false);
                logic.state = S_CHANGEOVER_PAUSE;
                logic.stateTimer = currentTime;
            }
            break;

        case S_CHANGEOVER_PAUSE:
            if (currentTime - logic.stateTimer >= PUMP_CHANGEOVER_PAUSE) {
                logic.activePumpIndex = 1 - logic.activePumpIndex; // Меняем насос
                logic.state = S_START_DELAY; // Начинаем цикл старта для другого насоса
                logic.stateTimer = currentTime;
            }
            break;

        case S_ALL_PUMPS_ALARM:
            setRelay(p1_relay, false);
            setRelay(p2_relay, false);
            // Выход из этого состояния - только ручное вмешательство (сброс через веб)
            break;
        
        // Логика сухого хода (не часть основного автомата, а параллельная проверка)
    }

    // 5. Отдельная логика обработки сухого хода
    if (dry_run_stable == 0) { // Сухой ход активен
        if (!logic.dryRunAlarmPending) {
            logic.dryRunAlarmPending = true;
            logic.dryRunAlarmStartTime = currentTime;
        }
        if (currentTime - logic.dryRunAlarmStartTime >= DRY_RUN_ALARM_DELAY) {
            // Фактически останавливаем все, если тревога подтвердилась
            setRelay(p1_relay, false);
            setRelay(p2_relay, false);
        }
    } else { // Сухой ход неактивен
        logic.dryRunAlarmPending = false;
    }
}


// --- Вспомогательная функция для получения статуса ---

const char* getPumpStatusString(PumpStatus status) {
    switch (status) {
        case S_WORKING: return "S_WORKING";
        case S_ALARM:   return "S_ALARM";
        case S_REPAIR:  return "S_REPAIR";
        case S_OK:      
        default:        return "S_OK";
    }
}

