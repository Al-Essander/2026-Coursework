#pragma once
#include <string>
#include <algorithm>
#include "cleaner.hpp"
using namespace std;

// Парсит диапазон из одной строки, например "100 - 500" или "52-22"
// Возвращает false, если не удалось найти два числа
bool parseRangeFromString(const string& raw, double& minVal, double& maxVal)
{
    string s = normalizeField(raw); // Убираем BOM, \r, заменяем тире на '-'

    // Ищем разделитель '-'
    size_t dashPos = s.find('-');
    if (dashPos == string::npos) {
        // Если дефиса нет, возможно, это одно число? Или ошибка.
        // Попробуем считать как одно число, где min == max
        try {
            minVal = stod(cleanNumber(s));
            maxVal = minVal;
            return true;
        } catch (...) {
            return false;
        }
    }

    string sMinStr = s.substr(0, dashPos);
    string sMaxStr = s.substr(dashPos + 1);

    string sMinClean = cleanNumber(sMinStr);
    string sMaxClean = cleanNumber(sMaxStr);

    if (sMinClean.empty() || sMaxClean.empty()) return false;

    try {
        minVal = stod(sMinClean);
        maxVal = stod(sMaxClean);
    } catch (...) {
        return false;
    }

    // Исправляем, если мин больше макс (как в примере 52-22)
    if (minVal > maxVal) {
        swap(minVal, maxVal);
    }

    return true;
}

// Конвертация мощности из Вт в кВт
double wattsToKw(double watts) {
    return watts / 1000.0;
}
