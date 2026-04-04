#include <iostream>
#include <string>




#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// ─────────────────────────────────────────────
// Настройка консоли (твоя идея, просто дополнена)
// ─────────────────────────────────────────────
void setupConsole()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

// ─────────────────────────────────────────────
// Структура записи вентилятора
// Убрал flowRate — его не было в остальном коде
// ─────────────────────────────────────────────
struct Fan
{
    unsigned int id;
    double flowMin,     flowMax;        // Q, м³/ч
    double pressureMin, pressureMax;    // p, Па
    double power;                       // Ny, кВт
    double efficiency;                  // η, КПД (0..1)
    string status;                      // КОРРЕКТНО / СКОРРЕКТИРОВАНО / НУЖНА ПРОВЕРКА
};

// ─────────────────────────────────────────────
// Вспомогательная: убираем пробелы из строки
// '\xC2' — первый байт мусорного UTF-8 артефакта
// ─────────────────────────────────────────────
static string removeSpaces(const string& s)
{
    string result;
    for (char c : s)
        if (c != ' ' && c != '\t' && (unsigned char)c != 0xC2)
            result += c;
    return result;
}

// ─────────────────────────────────────────────
// Парсинг строки вида "100-500" или "100–500"
// Исправлено: добавлен try/catch, обработка UTF-8 тире,
//             swap при инверсии min > max
// ─────────────────────────────────────────────
bool parseRange(const string& input, double& minVal, double& maxVal)
{
    string sMin, sMax;

    // Сначала ищем UTF-8 тире – (E2 80 93, 3 байта)
    size_t pos = input.find("\xE2\x80\x93");
    if (pos != string::npos)
    {
        sMin = removeSpaces(input.substr(0, pos));
        sMax = removeSpaces(input.substr(pos + 3));
    }
    else
    {
        // Обычный ASCII дефис
        size_t dash = input.find('-');
        if (dash == string::npos) return false;
        sMin = removeSpaces(input.substr(0, dash));
        sMax = removeSpaces(input.substr(dash + 1));
    }

    // stod бросает исключение если строка не число — ловим
    try
    {
        minVal = stod(sMin);
        maxVal = stod(sMax);
    }
    catch (...) { return false; }

    // "52–22" → переставляем, статус позже выставит validateFan
    if (minVal > maxVal)
        swap(minVal, maxVal);

    return true;
}

// ─────────────────────────────────────────────
// Валидация и проставление статуса
// Исправлено: убран несуществующий fan.flowRate,
//             добавлена физическая проверка по формуле
// ─────────────────────────────────────────────
bool validateFan(Fan& fan)
{
    // Физические диапазоны для промышленных вентиляторов
    const double Q_MAX   = 100000.0;   // м³/ч
    const double P_MIN   = 1.0,   P_MAX = 10000.0;  // Па
    const double Ny_MIN  = 0.01,  Ny_MAX = 500.0;   // кВт
    const double ETA_MIN = 0.1,   ETA_MAX = 1.0;    // безразмерный

    if (fan.flowMin  <= 0 || fan.flowMin  > Q_MAX)          return false;
    if (fan.flowMax  <= 0 || fan.flowMax  > Q_MAX)          return false;
    if (fan.flowMin  > fan.flowMax)                         return false;
    if (fan.pressureMin < P_MIN || fan.pressureMin > P_MAX) return false;
    if (fan.pressureMax < P_MIN || fan.pressureMax > P_MAX) return false;
    if (fan.power    < Ny_MIN  || fan.power    > Ny_MAX)    return false;
    if (fan.efficiency < ETA_MIN || fan.efficiency > ETA_MAX) return false;

    // Физическая согласованность: Ny = Q * p / (η * 3600)
    // Q в м³/ч → делим на 3600 чтобы получить м³/с
    double Q_avg  = (fan.flowMin + fan.flowMax) / 2.0;
    double P_avg  = (fan.pressureMin + fan.pressureMax) / 2.0;
    double Ny_theory = (Q_avg / 3600.0) * P_avg / fan.efficiency; // Вт
    double Ny_actual = fan.power * 1000.0;                         // кВт → Вт

    double ratio = Ny_actual / Ny_theory;

    // Отклонение больше порядка — подозрительно, но не критично
    if (ratio < 0.1 || ratio > 10.0)
        fan.status = "НУЖНА ПРОВЕРКА";
    else
        fan.status = "КОРРЕКТНО";

    return true;
}

// ─────────────────────────────────────────────
// Меню — твоё, только добавлен setupConsole()
// ─────────────────────────────────────────────
unsigned short int menu()
{
    cout << "fansearcher ver.0.1" << endl;
    cout << "1. тест parseRange" << endl;
    cout << "0. выход" << endl;
    cout << "choice: ";

    unsigned short int choice;
    cin >> choice;
    return choice;
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main()
{
    setupConsole();

    while (true)
    {
        unsigned short int choice = menu();
        switch (choice)
        {
            case 0:
                return 0;   // выход из программы, а не просто break из switch

            case 1:
            {
                // Тест parseRange на разных форматах грязных данных
                string tests[] = { "100-500", "52-22", "100–500", "1 140-5 000" };
                for (const string& t : tests)
                {
                    double lo, hi;
                    if (parseRange(t, lo, hi))
                        cout << "\"" << t << "\" → [" << lo << ", " << hi << "]" << endl;
                    else
                        cout << "\"" << t << "\" → не распознано" << endl;
                }
                break;
            }

            default:
                cout << "unknown command..." << endl;
                break;
        }
    }
}
