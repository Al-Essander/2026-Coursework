#pragma once
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include "fan.hpp"
#include "parser.hpp"
using namespace std;

// Разбивает строку CSV.
// ВАЖНО: Проверь, какой разделитель в твоем файле.
// Если Excel русскоязычный, там может быть ';'. Если ',' - оставь ','.
static vector<string> splitCSV(const string& line, char sep = ',')
{
    vector<string> fields;
    string cur;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];
        if (c == '"')
        {
            inQuotes = !inQuotes;
        }
        else if (c == sep && !inQuotes)
        {
            fields.push_back(normalizeField(cur));
            cur.clear();
        }
        else
        {
            cur += c;
        }
    }
    fields.push_back(normalizeField(cur));
    return fields;
}

vector<Fan> loadCSV(const string& path)
{
    vector<Fan> result;
    ifstream file(path);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << path << endl;
        return result;
    }

    string line;
    bool firstLine = true;

    while (getline(file, line))
    {
        // Пропуск пустых строк
        if (line.empty() || line == "\r") continue;

        if (firstLine) {
            firstLine = false;
            continue; // Пропускаем заголовок
        }

        vector<string> f = splitCSV(line);

        // Проверяем, достаточно ли колонок.
        // В твоем Excel 10 колонок (индексы 0-9).
        // 0: Номер, 1: Тип, 2: Модель, 3: Типоразмер, 4: Диаметр,
        // 5: Производительность, 6: Давление, 7: Мощность(Вт), 8: Шум, 9: Цена
        if (f.size() < 10) {
            // cerr << "Warning: Skipping incomplete line: " << line << endl;
            continue;
        }

        Fan fan;
        fan.status = "КОРРЕКТНО";

        // 1. ID
        try {
            fan.id = (unsigned int)stoul(f[0]);
        } catch (...) {
            continue;
        }

        // 2. Текстовые поля
        fan.type = f[1];
        fan.model = f[2];

        // 3. Парсинг Производительности (Колонка 5)
        if (!parseRangeFromString(f[5], fan.flowMin, fan.flowMax)) {
            fan.status = "ОШИБКА (Производительность)";
            // Можно добавить в результат, чтобы увидеть ошибку, или пропустить
            result.push_back(fan);
            continue;
        }

        // 4. Парсинг Давления (Колонка 6)
        if (!parseRangeFromString(f[6], fan.pressureMin, fan.pressureMax)) {
            fan.status = "ОШИБКА (Давление)";
            result.push_back(fan);
            continue;
        }

        // 5. Мощность (Колонка 7). В таблице она в Ваттах!
        try {
            string powerStr = cleanNumber(f[7]);
            double powerWatts = stod(powerStr);
            fan.powerKw = wattsToKw(powerWatts);
        } catch (...) {
            fan.status = "ОШИБКА (Мощность)";
            result.push_back(fan);
            continue;
        }

        // 6. Шум (Колонка 8)
        try {
            fan.noiseLevel = stod(cleanNumber(f[8]));
        } catch (...) {
            fan.noiseLevel = 0.0; // Или статус ошибки
        }

        // 7. Цена (Колонка 9)
        try {
            // В цене могут быть пробелы "18 500", cleanNumber уберет их
            fan.price = stod(cleanNumber(f[9]));
        } catch (...) {
            fan.price = 0.0;
        }

        // 8. Валидация (Физическая проверка)
        // Так как КПД нет в таблице, мы не можем сделать полную проверку Ny = Q*P / eta.
        // Но мы можем проверить логику диапазонов и разумность значений.

        if (fan.flowMin <= 0 || fan.flowMax <= 0) {
             fan.status = "ОШИБКА (Отрицательный поток)";
        }

        // Пример простой эвристики: если мощность слишком велика для такого потока/давления
        // Это можно усложнить позже, добавив расчет КПД "задним числом"

        result.push_back(fan);
    }

    return result;
}

void saveCSV(const string& path, const vector<Fan>& fans)
{
    ofstream file(path);
    // Заголовок для нового чистого файла
    file << "id,type,model,flowMin,flowMax,pressureMin,pressureMax,powerKw,noiseLevel,price,status\n";

    for (const Fan& fan : fans)
    {
        file << fan.id          << ","
             << fan.type        << ","
             << fan.model       << ","
             << fan.flowMin     << ","
             << fan.flowMax     << ","
             << fan.pressureMin << ","
             << fan.pressureMax << ","
             << fan.powerKw     << ","
             << fan.noiseLevel  << ","
             << fan.price       << ","
             << fan.status      << "\n";
    }
}
