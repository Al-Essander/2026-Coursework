#pragma once
#include "fan.hpp"

// возвращает false если запись критически сломана
// иначе выставляет fan.status
bool validateFan(Fan& fan)
{
    const double Q_MAX   = 100000.0;
    const double P_MIN   = 1.0,   P_MAX  = 10000.0;
    const double Ny_MIN  = 0.01,  Ny_MAX = 500.0;
    const double ETA_MIN = 0.1,   ETA_MAX = 1.0;

    if (fan.flowMin  <= 0 || fan.flowMax  > Q_MAX)          return false;
    if (fan.flowMin  > fan.flowMax)                         return false;
    if (fan.pressureMin < P_MIN || fan.pressureMax > P_MAX) return false;
    if (fan.power    < Ny_MIN  || fan.power   > Ny_MAX)     return false;
    if (fan.efficiency < ETA_MIN || fan.efficiency > ETA_MAX) return false;

    // физическая проверка: Ny = Q*p / (Ню*3600)
    double Q_avg     = (fan.flowMin + fan.flowMax) / 2.0;
    double P_avg     = (fan.pressureMin + fan.pressureMax) / 2.0;
    double Ny_theory = (Q_avg / 3600.0) * P_avg / fan.efficiency; // Вт
    double Ny_actual = fan.power * 1000.0;                         // Вт
    double ratio     = Ny_actual / Ny_theory;

    // не перебиваем СКОРРЕКТИРОВАНО если оно уже стоит
    if (fan.status != "СКОРРЕКТИРОВАНО")
    {
        if (ratio < 0.1 || ratio > 10.0)
            fan.status = "НУЖНА ПРОВЕРКА";
        else
            fan.status = "КОРРЕКТНО";
    }

    return true;
}
