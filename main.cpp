// каталог промышленных вентиляторов
// курсовая работа по предмету Структуры и алгоритмы компьютерной обработки данных.
// Тема курсовой: "Адаптивная многокритериальная фильтрация в структурированных каталогах с неоднородными параметрами"


// Андреев Александр Алексеевич, РЭУ им. Г.В. Плеханова, 2026 год



// Математическая модель программы: Fan = ( Q_min, Q_max, P_min, P_max, N_y, D)
// пространство итоговое f = f_validate and f_normalize and f_parse and f_clean
// предикаты Fan = п1 and п2 and п3 and п4 and п5

// п1 = Q_min <= Q_max
// п2 = P_min <= P_max
// п3 = Q_min >0 and Q_max > 0
// п4 = P_min >0 and P_max > 0
// п5 = N_y >0


// N_гидр = (Q_средн * P_средн)/3600
// Q_средн = (Q_min+Q_max) / 2
// P_средн = (P_min+P_max) / 2
// r(Fan) = N_y / N_гидр

// g: D_clean -> {CORRECT, CORRECTED, NEEDS_CHECK, ERROR}


// фильтр блума 1970г.
// Ложноположительный ответ = P_fp ~ (1-e^(-kn/m))^k
// Кирш-Митценмахер = pos_i = (h1+i*h2)mod(m)



#include <iostream>
#include <string>
#include <vector>
#include <map> //multimap по Q, сорт по ключу
#include <unordered_map> //хэш таблица по типу

#include <fstream> // для csv
#include <sstream> // string в число и для csv

#include <cctype> //клинер символов
#include <algorithm> //сортировка

#include <functional> //для блума
#include <cmath>

#include <cstdio> //для экспорта


#include <random> //тест
#include <chrono> //замерка


#ifdef _WIN32
#include <windows.h>
#endif



using namespace std;
using namespace std::chrono;





//1. цветаа

namespace clr {
    const char* RST  = "\033[0m"; // ориг. консоль
    const char* RED  = "\033[31m";
    const char* GRN  = "\033[32m";
    const char* YLW  = "\033[33m";
    const char* BLU  = "\033[34m";
    const char* MAG  = "\033[35m";
    const char* CYN  = "\033[36m";
    const char* BOLD = "\033[1m";
    const char* DIM  = "\033[2m";
}




unsigned const double R_TYPICAL_LOW  = 1.5;   // нижн r
unsigned const double R_TYPICAL_HIGH = 10.0;  // верхн r
unsigned const double R_TYPICAL_MID  = 5.0;   // середина

unsigned const double R_HARD_LOW     = 0.5;   // физ недопустимость r
unsigned const double R_HARD_HIGH    = 100.0;

unsigned const double R_SOFT_LOW     = 1.0;   // норм допустипость r
unsigned const double R_SOFT_HIGH    = 30.0;

unsigned const double POWER_MIN_KW   = 0.01;  // предикат допустимости P_adm
unsigned const double POWER_MAX_KW   = 500.0;


const unsigned int BLOOM_M  = 4096;  // размер битового массива
const unsigned int BLOOM_K  = 3;     // хэш функции блум фильтра, можно менять



void setupConsole() //сетап
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | 0x0004);
#endif
}



void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    (void)system("clear");
#endif
}

void pressEnter()
{
    cout << "\n" << clr::DIM << " нажмите enter для продолжения." << clr::RST;
    cin.ignore(1000, '\n');
    // на всякий 1000 в игнор чтобы вообще убедиться
}






//2. представление Fan



// Коды ошибок данных (E0–E7)
enum ErrorType
{
    NONE,                    // E0: OK
    PARSE_ERROR,             // E1: не удалось распознать число
    INVALID_RANGE,           // E2: невалидный диапазон
    INVALID_PHYSICAL_VALUE,  // E3: Q <= 0 или P <= 0
    INVALID_EFFICIENCY,      // E4: КПД вне (0, 1]
    INVALID_POWER,           // E5: мощность невалидна
    INVALID_STRUCTURE,       // E6: деление на ноль
    EMPTY_FIELD              // E7: пустое поле
};


//Fan = (Q_min, Q_max, P_min, P_max, N_y, D)
struct Fan
{
    unsigned int id;
    string type;       // типы ВО ВЦ ВКОП ВР Ц и тд
    string model;
    string size;       // типоразмер
    unsigned short diameter;  // диаметр D колеса в мм.

    double flowMin;    // Q_min м3ч
    double flowMax;    // Q_max м3ч

    double pressureMin; // P_min, Па
    double pressureMax; // P_max, Па

    double power;      // N_y электричество кВт

    double efficiency; // ню (длинная n) кпд
    unsigned int price;

    string status;     // g: D_clean -> {CORRECT, CORRECTED, NEEDS_CHECK, ERROR}
    ErrorType errorType; //переменная для ошибки
    bool corrected; // коррекция любая
    bool effImputed; //коррекция кпд
};




struct RawFan {
    string id, type, model, size, diameter, flow, pressure, power, noise, price;
};

struct Query {
    string type;       // пусто = любой тип
    double Q;          // точка производительность м3ч
    double P;          // точка давление Па
    bool includeYellow; // при поиске проверяем даже исправленные
};









//3: PIPELINE — этапы f_clean, f_parse, f_normalize, f_validate


// f_clean: удаление BOM, пробелов, \r, унификация


string removeSpaces(const string& s) {
    string res;
    for (unsigned int i = 0; i<s.size(); i++) { //обход по символу
        unsigned char c = (unsigned char)s[i]; //в байт

        if (c == '' || c=='\t' || c=='\r' || c=='\n') {
                continue; //пропуск
        }
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xA0) { //неразрывные пробелы из ютф
                i++;
                continue; //пропуск
        }
        res += s[i]; //если не пробел - оставляем

    }
    return res;
}



string stripBOM(const string& s)
{
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF //длинная строка +  если первые байты это экселевский мусор
        && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        return s.substr(3);
    return s;
}

string trim(const string& s)
{
    unsigned int a = 0, b = (unsigned int)s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

string cleanNumber(string s)
{
    string res;
    for (unsigned int i = 0; i < s.size(); i++)
    {
        char c = s[i];
        if (isdigit((unsigned char)c) || c == '.' || c == ',' || c == '-') res += c;
    }
    for (unsigned int i = 0; i < res.size(); i++)
        if (res[i] == ',') res[i] = '.';
    return res;
}













