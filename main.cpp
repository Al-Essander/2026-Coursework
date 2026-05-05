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
#include <map>               // multimap по Q, сорт по ключу
#include <unordered_map>    // хэш таблица по типу

#include <fstream>          // для csv
#include <sstream>          // string в число и для csv

#include <cctype>           // клинер символов
#include <algorithm>        // сортировка

#include <functional>       // для блума
#include <cmath>

#include <cstdio>           // для экспорта

#include <random>           // тест
#include <chrono>           // замерка

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using namespace std::chrono;

// 1. цветаа

namespace clr {
    const char* RST  = "\033[0m";  // ориг. консоль
    const char* RED  = "\033[31m";
    const char* GRN  = "\033[32m";
    const char* YLW  = "\033[33m";
    const char* BLU  = "\033[34m";
    const char* MAG  = "\033[35m";
    const char* CYN  = "\033[36m";
    const char* BOLD = "\033[1m";
    const char* DIM  = "\033[2m";
}

// константы (исправлено: убрано unsigned перед const double)
const double R_TYPICAL_LOW  = 1.5;   // нижн r
const double R_TYPICAL_HIGH = 10.0;  // верхн r
const double R_TYPICAL_MID  = 5.0;   // середина

const double R_HARD_LOW     = 0.5;   // физ недопустимость r
const double R_HARD_HIGH    = 100.0;

const double R_SOFT_LOW     = 1.0;   // норм допустимость r
const double R_SOFT_HIGH    = 30.0;

const double POWER_MIN_KW   = 0.01;  // предикат допустимости P_adm
const double POWER_MAX_KW   = 500.0;

const unsigned int BLOOM_M  = 4096;  // размер битового массива
const unsigned int BLOOM_K  = 3;     // хэш функции блум фильтра, можно менять

void setupConsole() // сетап
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

// 2. представление Fan

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

// Fan = (Q_min, Q_max, P_min, P_max, N_y, D)
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
    ErrorType errorType; // переменная для ошибки
    bool corrected; // коррекция любая
    bool effImputed; // коррекция кпд
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

// 3: PIPELINE — этапы f_clean, f_parse, f_normalize, f_validate

// f_clean: удаление BOM, пробелов, \r, унификация

string removeSpaces(const string& s) {
    string res;
    for (unsigned int i = 0; i < s.size(); i++) { // обход по символу
        unsigned char c = (unsigned char)s[i];    // в байт

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue; // пропуск
        }
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xA0) {
            i++;
            continue; // пропуск неразрывного пробела UTF-8
        }
        res += s[i]; // если не пробел - оставляем
    }
    return res;
}

string stripBOM(const string& s)
{
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF
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
    for (unsigned int i = 0; i < s.size(); i++) {
        char c = s[i];
        if (isdigit((unsigned char)c) || c == '.' || c == ',' || c == '-') res += c;
    }
    for (unsigned int i = 0; i < res.size(); i++)
        if (res[i] == ',') res[i] = '.';
    return res;
}

bool safeStod(const string& s, double& out) {
    if (s.empty()) return false;
    try { out = stod(s); return true; }
    catch (...) { return false; }
}

bool safeStou(const string& s, unsigned int& out) {
    if (s.empty()) return false;
    try {
        long v = stol(s);
        if (v < 0) return false;
        out = static_cast<unsigned int>(v);
        return true;
    } catch (...) { return false; }
}

bool isEmptyField(const string& s) {
    string t = removeSpaces(s);
    if (t.empty()) return true;

    static const vector<string> markers = {
        "-", "\xe2\x80\x94", "?", "??", "???",   // em-dash и знаки вопроса
        "N/A", "n/a", "NA", "n/A"                // англоязычные маркеры
    };

    for (const auto& marker : markers)
        if (t == marker) return true;

    return false;
}

// f_parse: распознавание диапазонов, извлечение единиц

bool parseRange(const string& input, double& minVal, double& maxVal, bool& corrected)
{
    if (isEmptyField(input)) return false;

    string sMin, sMax;
    bool found = false;

    // Определяем все возможные разделители диапазона
    struct Delimiter {
        string pattern;      // сам разделитель
        size_t length;       // его длина в байтах
    };

    static const Delimiter delimiters[] = {
        {"\xE2\x80\x93", 3},  // en-dash (UTF-8)
        {"\xE2\x80\x94", 3},  // em-dash (UTF-8)
        {" - ", 3}             // пробел-дефис-пробел
    };

    // Пробуем каждый разделитель
    for (const auto& delim : delimiters) {
        size_t pos = input.find(delim.pattern);
        if (pos != string::npos) {
            sMin = input.substr(0, pos);
            sMax = input.substr(pos + delim.length);
            found = true;
            break;
        }
    }

    // Особый случай: ASCII '-' с цифрой слева (без пробелов, например "3240-21800")
    if (!found) {
        for (size_t i = 1; i < input.size(); i++) {
            if (input[i] == '-' && isdigit(static_cast<unsigned char>(input[i - 1]))) {
                sMin = input.substr(0, i);
                sMax = input.substr(i + 1);
                found = true;
                break;
            }
        }
    }

    // Если разделитель не найден — значит одно число
    if (!found) {
        double v;
        if (!safeStod(cleanNumber(removeSpaces(input)), v)) return false;
        minVal = v;
        maxVal = v;
        return true;
    }

    // Очищаем и конвертируем обе части
    sMin = removeSpaces(sMin);
    sMax = removeSpaces(sMax);

    double a, b;
    if (!safeStod(cleanNumber(sMin), a)) return false;
    if (!safeStod(cleanNumber(sMax), b)) return false;

    // Инвариант: гарантируем min <= max (предикаты pi1 и pi2)
    if (a > b) {
        swap(a, b);
        corrected = true;
    }
    minVal = a;
    maxVal = b;
    return true;
}

// f_normalize: приведение единиц, разрешение неоднозначности

double normalizePower(const string& s, double Q_min, double Q_max,
                      double P_min, double P_max, bool& /*corrected*/, ErrorType& err)
{
    if (isEmptyField(s)) { err = EMPTY_FIELD; return 0; }

    string lower = s;
    for (auto& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    const bool hasKW = lower.find("kw") != string::npos || lower.find("квт") != string::npos;
    const bool hasW  = !hasKW && (lower.find("w") != string::npos || lower.find("вт") != string::npos);

    double value;
    if (!safeStod(cleanNumber(removeSpaces(s)), value)) { err = PARSE_ERROR; return 0; }

    if (hasKW) return value;       // уже в кВт
    if (hasW)  return value / 1000.0; // ватты -> кВт

    // Без суффикса: предикат допустимости P_adm
    const double asW  = value / 1000.0; // трактовка "Вт"
    const double asKW = value;          // трактовка "кВт"

    const bool okW  = (asW  >= POWER_MIN_KW && asW  <= POWER_MAX_KW);
    const bool okKW = (asKW >= POWER_MIN_KW && asKW <= POWER_MAX_KW);

    if (okW && !okKW) return asW;
    if (okKW && !okW) return asKW;

    if (okW && okKW) {
        // Оба варианта допустимы: выбираем по r, ближайшему к R_TYPICAL_MID
        const double Q_mid = (Q_min + Q_max) / 2.0;
        const double P_mid = (P_min + P_max) / 2.0;
        const double N_hydr = (Q_mid * P_mid) / 3600.0; // Вт

        if (N_hydr > 0) {
            const double r_W  = (asW  * 1000.0) / N_hydr;
            const double r_KW = (asKW * 1000.0) / N_hydr;
            return (fabs(r_W - R_TYPICAL_MID) <= fabs(r_KW - R_TYPICAL_MID)) ? asW : asKW;
        }
        return asW; // fallback
    }

    err = INVALID_POWER;
    return asW;
}

// f_validate: проверка предикатов, расчёт r, формирование статуса

void validateFan(Fan& f)
{
    if (f.errorType != NONE) { f.status = "ERROR DETECTED"; return; }

    // π3: Q > 0, π4: P > 0
    if (f.flowMin <= 0 || f.flowMax <= 0 || f.pressureMin <= 0 || f.pressureMax <= 0) {
        f.errorType = INVALID_PHYSICAL_VALUE;
        f.status = "ERROR DETECTED";
        return;
    }

    // π5: N_y > 0
    if (f.power <= 0) {
        f.errorType = INVALID_POWER;
        f.status = "ERROR DETECTED";
        return;
    }

    // Гидравлическая мощность (теоретический минимум)
    const double Q_mid = (f.flowMin + f.flowMax) / 2.0;
    const double P_mid = (f.pressureMin + f.pressureMax) / 2.0;
    const double N_hydr_W = (Q_mid * P_mid) / 3600.0;

    if (N_hydr_W <= 0) {
        f.errorType = INVALID_STRUCTURE;
        f.status = "ERROR DETECTED";
        return;
    }

    // Коэффициент согласованности: r = N_y / N_hydr
    const double Ny_W = f.power * 1000.0;
    const double r = Ny_W / N_hydr_W;

    // Восстановление η (метаданные)
    f.efficiency = (f.flowMin * f.pressureMin) / (Ny_W * 3600.0);
    f.effImputed = true;

    // Жёсткие границы
    if (r < R_HARD_LOW || r > R_HARD_HIGH) {
        f.errorType = INVALID_EFFICIENCY;
        f.status = "ERROR DETECTED";
        return;
    }

    // Мягкие границы
    if (r >= R_SOFT_LOW && r <= R_SOFT_HIGH)
        f.status = f.corrected ? "CORRECTED" : "CORRECT";
    else
        f.status = "NEEDS CHECK";
}

// 4. CSV ввод/вывод

string errorMessage(ErrorType e)
{
    static const unordered_map<ErrorType, string> messages = {
        {NONE,                  "E0: OK"},
        {PARSE_ERROR,           "E1: Parse error"},
        {INVALID_RANGE,         "E2: Invalid range"},
        {INVALID_PHYSICAL_VALUE,"E3: Physical violation"},
        {INVALID_EFFICIENCY,    "E4: Efficiency OOR"},
        {INVALID_POWER,         "E5: Invalid power"},
        {INVALID_STRUCTURE,     "E6: Structure error"},
        {EMPTY_FIELD,           "E7: Empty field"}
    };
    auto it = messages.find(e);
    return it != messages.end() ? it->second : "E0: OK";
}

string colorStatus(const string& s)
{
    static const unordered_map<string, string> colors = {
        {"CORRECT",     string(clr::GRN)},
        {"CORRECTED",   string(clr::YLW)},
        {"NEEDS CHECK", string(clr::YLW)}
    };
    auto it = colors.find(s);
    string color = it != colors.end() ? it->second : string(clr::RED);
    return color + s + clr::RST;
}

vector<string> splitCSV(const string& line, char sep)
{
    vector<string> out;
    istringstream ss(line);
    string token;
    while (getline(ss, token, sep)) {
        if (!token.empty() && token.back() == '\r') token.pop_back();
        out.push_back(token);
    }
    return out;
}

bool loadCSV(const string& path, vector<RawFan>& out)
{
    ifstream in(path);
    if (!in.is_open()) {
        cout << clr::RED << "  [P1] Файл не найден: " << path << clr::RST << "\n";
        return false;
    }

    string line;
    bool firstLine = true;
    char sep = ';';

    while (getline(in, line)) {
        if (firstLine) {
            line = stripBOM(line);
            firstLine = false;
            if (line.find(';') == string::npos && line.find(',') != string::npos) sep = ',';

            // [P2] Детектор повторной прогонки
            if (line.find("Q_min") != string::npos && line.find("Q_max") != string::npos) {
                cout << clr::RED << "  [P2] Файл уже нормализован (шапка содержит Q_min/Q_max).\n"
                     << "       Повторная обработка не предусмотрена — схема выходного\n"
                     << "       файла отличается от входной. Используйте исходный CSV."
                     << clr::RST << "\n";
                return false;
            }
            continue;
        }
        if (line.empty()) continue;

        vector<string> cols = splitCSV(line, sep);
        if (cols.size() < 8) continue;

        RawFan r;
        r.id       = cols[0];
        r.type     = cols[1];
        r.model    = cols[2];
        r.size     = cols[3];
        r.diameter = cols[4];
        r.flow     = cols[5];
        r.pressure = cols[6];
        r.power    = cols[7];
        r.noise    = cols.size() > 8 ? cols[8] : "";
        r.price    = cols.size() > 9 ? cols[9] : "";
        out.push_back(r);
    }
    return true;
}

bool saveCSV(const string& path, const vector<Fan>& fans)
{
    ofstream out(path, ios::binary);
    if (!out.is_open()) return false;

    out << "\xEF\xBB\xBF"; // UTF-8 BOM
    out << "id;type;model;size;diameter;Q_min;Q_max;P_min;P_max;N_y_kW;eta;price;status;corrected;eff_imputed;error\n";

    for (const auto& f : fans) {
        out << f.id << ';' << f.type << ';' << f.model << ';' << f.size << ';' << f.diameter << ';';
        out.precision(2); out << fixed;
        out << f.flowMin << ';' << f.flowMax << ';' << f.pressureMin << ';' << f.pressureMax << ';';
        out.precision(4);
        out << f.power << ';' << f.efficiency << ';';
        out.unsetf(ios::fixed);
        out << f.price << ';' << f.status << ';'
            << (f.corrected   ? "1" : "0") << ';'
            << (f.effImputed  ? "1" : "0") << ';'
            << errorMessage(f.errorType) << '\n';
    }
    return true;
}

// 5. ИНДЕКСНЫЕ СТРУКТУРЫ (Bloom, хеш-таблица, multimap)

struct BloomFilter
{
    vector<bool> bits;
    BloomFilter() : bits(BLOOM_M, false) {}

    unsigned int hashI(const string& s, unsigned int i) const {
        size_t h1 = std::hash<string>{}(s);
        size_t h2 = std::hash<string>{}(s + "_salt");
        return static_cast<unsigned int>((h1 + i * h2) % BLOOM_M);
    }

    void add(const string& s) {
        for (unsigned int i = 0; i < BLOOM_K; ++i)
            bits[hashI(s, i)] = true;
    }

    bool maybeContains(const string& s) const {
        for (unsigned int i = 0; i < BLOOM_K; ++i)
            if (!bits[hashI(s, i)]) return false;
        return true;
    }
};

struct Index
{
    unordered_map<string, vector<unsigned int>> byType; // хеш-бакет
    BloomFilter bloomTypes;
    multimap<double, unsigned int> byQmin; // сбалансированное дерево
};

void buildIndex(const vector<Fan>& fans, Index& idx)
{
    idx.byType.clear();
    idx.byQmin.clear();
    idx.bloomTypes = BloomFilter();

    for (unsigned int i = 0; i < fans.size(); ++i) {
        const Fan& f = fans[i];
        if (f.status == "ERROR DETECTED") continue;
        idx.byType[f.type].push_back(i);
        idx.bloomTypes.add(f.type);
        idx.byQmin.insert({f.flowMin, i});
    }
}

// 6. ПОИСК (линейный + индексный)

inline bool matches(const Fan& f, const Query& q) {
    return f.status != "ERROR DETECTED"
        && (q.includeYellow || f.status != "NEEDS CHECK")
        && (q.type.empty() || f.type == q.type)
        && q.Q >= f.flowMin && q.Q <= f.flowMax
        && q.P >= f.pressureMin && q.P <= f.pressureMax;
}

void linearSearch(const vector<Fan>& fans, const Query& q, vector<unsigned int>& out) {
    out.clear();
    for (unsigned int i = 0; i < fans.size(); ++i)
        if (matches(fans[i], q))
            out.push_back(i);
}

void indexedSearch(const vector<Fan>& fans, const Index& idx,
                   const Query& q, vector<unsigned int>& out) {
    out.clear();
    if (!q.type.empty()) {
        if (!idx.bloomTypes.maybeContains(q.type)) return;
        auto it = idx.byType.find(q.type);
        if (it == idx.byType.end()) return;
        for (unsigned int id : it->second)
            if (matches(fans[id], q))
                out.push_back(id);
    } else {
        auto end = idx.byQmin.upper_bound(q.Q);
        for (auto it = idx.byQmin.begin(); it != end; ++it)
            if (matches(fans[it->second], q))
                out.push_back(it->second);
    }
}

// 7. МЕНЮ — Загрузка, Поиск, Бенчмарк, Верификация

void printResults(const vector<Fan>& fans, const vector<unsigned int>& res, unsigned int show)
{
    cout << "  Найдено: " << clr::BOLD << res.size() << clr::RST << " записей\n";
    if (res.empty()) return;

    unsigned int n = min(show, static_cast<unsigned int>(res.size()));
    cout << "\n  " << clr::DIM << "ID    Тип    Модель                  Q [м3/ч]            P [Па]          N_y kW   Статус" << clr::RST << "\n";
    for (unsigned int i = 0; i < n; ++i) {
        const Fan& f = fans[res[i]];
        printf("  %-5u %-6s %-23s %6.0f..%-8.0f %5.0f..%-6.0f %7.2f  ",
               f.id, f.type.c_str(), f.model.c_str(),
               f.flowMin, f.flowMax, f.pressureMin, f.pressureMax, f.power);
        cout << colorStatus(f.status) << "\n";
    }
    if (res.size() > n) cout << "  " << clr::DIM << "... ещё " << (res.size() - n) << " записей" << clr::RST << "\n";
}

void menuLoadAndNormalize(vector<Fan>& fans, Index& idx, bool& dataReady)
{
    cout << "\n  Путь к CSV (Enter = ventsearch_massive_sorted.csv): ";
    string path; getline(cin, path); path = trim(path);
    if (path.empty()) path = "ventsearch_massive_sorted.csv";

    vector<RawFan> raw;
    auto t0 = high_resolution_clock::now();
    if (!loadCSV(path, raw)) return;
    auto t1 = high_resolution_clock::now();

    cout << "  Загружено: " << raw.size() << " записей за "
         << duration_cast<milliseconds>(t1 - t0).count() << " мс\n";

    fans.clear(); fans.reserve(raw.size());
    unsigned int cnt[4] = {0, 0, 0, 0};

    auto t2 = high_resolution_clock::now();
    for (unsigned int i = 0; i < raw.size(); ++i) {
        Fan f{};
        f.type   = raw[i].type;
        f.model  = raw[i].model;
        f.size   = raw[i].size;
        f.errorType = NONE;
        f.corrected = false;
        f.effImputed = false;

        unsigned int v;
        if (safeStou(removeSpaces(raw[i].id), v)) f.id = v;
        if (safeStou(removeSpaces(raw[i].diameter), v) && v < 65535) f.diameter = static_cast<unsigned short>(v);
        if (safeStou(removeSpaces(raw[i].price), v)) f.price = v;

        bool r1 = parseRange(raw[i].flow, f.flowMin, f.flowMax, f.corrected);
        bool r2 = parseRange(raw[i].pressure, f.pressureMin, f.pressureMax, f.corrected);
        if (!r1 || !r2) f.errorType = INVALID_RANGE;

        if (f.errorType == NONE)
            f.power = normalizePower(raw[i].power, f.flowMin, f.flowMax,
                                     f.pressureMin, f.pressureMax, f.corrected, f.errorType);

        validateFan(f);
        fans.push_back(f);

        if      (f.status == "CORRECT")     cnt[0]++;
        else if (f.status == "CORRECTED")   cnt[1]++;
        else if (f.status == "NEEDS CHECK") cnt[2]++;
        else                                cnt[3]++;
    }
    auto t3 = high_resolution_clock::now();

    cout << clr::BOLD << "\n  Нормализация: " << duration_cast<milliseconds>(t3 - t2).count() << " мс" << clr::RST << "\n";
    cout << "  " << clr::GRN << "CORRECT     : " << cnt[0] << clr::RST << "\n";
    cout << "  " << clr::YLW << "CORRECTED   : " << cnt[1] << clr::RST << "\n";
    cout << "  " << clr::YLW << "NEEDS CHECK : " << cnt[2] << clr::RST << "\n";
    cout << "  " << clr::RED << "ERROR       : " << cnt[3] << clr::RST << "\n";

    string outPath = path;
    size_t dot = outPath.rfind('.');
    if (dot != string::npos) outPath.insert(dot, "_normalized");
    else outPath += "_normalized.csv";
    if (saveCSV(outPath, fans)) cout << "  Сохранено: " << outPath << "\n";

    auto t4 = high_resolution_clock::now();
    buildIndex(fans, idx);
    auto t5 = high_resolution_clock::now();
    cout << "  Индексы: " << duration_cast<milliseconds>(t5 - t4).count() << " мс\n";

    // Оценка памяти
    size_t memF = fans.size() * sizeof(Fan);
    size_t memB = BLOOM_M / 8;
    size_t memH = 0;
    for (const auto& kv : idx.byType) memH += kv.second.size() * sizeof(unsigned int);
    size_t memT = idx.byQmin.size() * (sizeof(double) + sizeof(unsigned int) + 48);
    size_t memAll = memF + memB + memH + memT;

    cout << clr::DIM << "\n  Память:\n";
    printf("    Fan[]       : %7.1f КБ (%zu записей)\n", memF/1024.0, fans.size());
    printf("    Bloom       : %7.1f КБ (m=%u, k=%u)\n", memB/1024.0, BLOOM_M, BLOOM_K);
    printf("    Хеш-таблица : %7.1f КБ (%zu типов)\n", memH/1024.0, idx.byType.size());
    printf("    Multimap    : %7.1f КБ (%zu узлов)\n", memT/1024.0, idx.byQmin.size());
    printf("    Итого       : %7.1f КБ (%.2f МБ)\n", memAll/1024.0, memAll/1048576.0);
    cout << clr::RST;

    dataReady = true;
    pressEnter();
}

void menuSearch(const vector<Fan>& fans, const Index& idx)
{
    cout << "\n  " << clr::BOLD << "=== Поиск ===" << clr::RST << "\n";
    cout << "  Тип вентилятора (пусто = любой): ";
    string typeIn; getline(cin, typeIn); typeIn = trim(typeIn);
    cout << "  Расход Q (м3/ч): "; string qIn; getline(cin, qIn);
    cout << "  Давление P (Па): "; string pIn; getline(cin, pIn);
    cout << "  Включать NEEDS CHECK? (y/n): "; string yIn; getline(cin, yIn);

    Query q; q.type = typeIn;
    if (!safeStod(cleanNumber(qIn), q.Q) || !safeStod(cleanNumber(pIn), q.P)) {
        cout << clr::RED << "  [P3] Ошибка ввода" << clr::RST << "\n"; pressEnter(); return;
    }
    if (q.Q <= 0 || q.P <= 0) {
        cout << clr::RED << "  [P3] Q и P должны быть > 0" << clr::RST << "\n"; pressEnter(); return;
    }
    q.includeYellow = (!yIn.empty() && (yIn[0] == 'y' || yIn[0] == 'Y'));

    vector<unsigned int> resLin, resIdx;
    auto t1 = high_resolution_clock::now();
    linearSearch(fans, q, resLin);
    auto t2 = high_resolution_clock::now();
    indexedSearch(fans, idx, q, resIdx);
    auto t3 = high_resolution_clock::now();
    auto usLin = duration_cast<microseconds>(t2 - t1).count();
    auto usIdx = duration_cast<microseconds>(t3 - t2).count();

    cout << "\n  " << clr::RED << "Линейный: " << usLin << " мкс" << clr::RST << "\n";
    printResults(fans, resLin, 5);
    cout << "\n  " << clr::BLU << "Индексный: " << usIdx << " мкс" << clr::RST << "\n";
    printResults(fans, resIdx, 5);

    if (resLin.size() != resIdx.size())
        cout << "\n  " << clr::RED << "[!] РАСХОЖДЕНИЕ результатов" << clr::RST << "\n";

    if (usIdx > 0) printf("\n  Ускорение: %.1fx\n", (double)usLin / (double)usIdx);
    pressEnter();
}

void runScenario(const vector<Fan>& fans, const Index& idx,
                 const vector<Query>& queries, const string& name)
{
    long long totalLin = 0, totalIdx = 0;
    unsigned long long matchedLin = 0, matchedIdx = 0;
    vector<unsigned int> r;
    for (const auto& q : queries) {
        auto t1 = high_resolution_clock::now();
        linearSearch(fans, q, r); auto t2 = high_resolution_clock::now();
        matchedLin += r.size(); totalLin += duration_cast<nanoseconds>(t2 - t1).count();
        auto t3 = high_resolution_clock::now();
        indexedSearch(fans, idx, q, r); auto t4 = high_resolution_clock::now();
        matchedIdx += r.size(); totalIdx += duration_cast<nanoseconds>(t4 - t3).count();
    }
    double avgLin = (double)totalLin / queries.size() / 1000.0;
    double avgIdx = (double)totalIdx / queries.size() / 1000.0;

    cout << "\n  " << clr::BOLD << name << clr::RST << " (" << queries.size() << " запросов)\n";
    printf("    %sЛинейный:%s   %7.2f мкс/запрос\n", clr::RED, clr::RST, avgLin);
    printf("    %sИндексный:%s  %7.2f мкс/запрос\n", clr::BLU, clr::RST, avgIdx);
    if (avgIdx > 0) {
        double spd = avgLin / avgIdx;
        if (spd >= 1.0) printf("    %sУскорение: %.1fx%s\n", clr::GRN, spd, clr::RST);
        else printf("    %sЗамедление: %.1fx%s\n", clr::RED, 1.0/spd, clr::RST);
    }
    if (matchedLin != matchedIdx)
        cout << "    " << clr::RED << "[!] РАСХОЖДЕНИЕ" << clr::RST << "\n";
    else
        cout << "    Корректность: совпадает " << clr::GRN << "✓" << clr::RST << "\n";
}

void menuBenchmark(const vector<Fan>& fans, const Index& idx)
{
    cout << "\n  " << clr::BOLD << "=== Бенчмарк ===" << clr::RST << "\n";
    vector<string> types;
    for (const auto& kv : idx.byType) types.push_back(kv.first);
    if (types.empty()) { cout << "  Нет данных\n"; pressEnter(); return; }

    const unsigned int N = 1000;
    mt19937 rng(42);
    uniform_int_distribution<unsigned int> typeDist(0, (unsigned int)types.size() - 1);
    uniform_real_distribution<double> qDist(500.0, 30000.0), pDist(50.0, 2000.0);

    cout << "  Каталог: " << fans.size() << " записей, " << types.size() << " типов\n";

    // Сценарий А
    vector<Query> sA;
    for (unsigned int i = 0; i < N; ++i) {
        Query q;
        q.type = types[typeDist(rng)];
        q.Q = qDist(rng); q.P = pDist(rng);
        q.includeYellow = true;
        sA.push_back(q);
    }
    runScenario(fans, idx, sA, "Сценарий А: с типом (хеш-бакет + Bloom)");

    // Сценарий Б
    vector<Query> sB;
    uniform_real_distribution<double> qLow(500.0, 3000.0);
    for (unsigned int i = 0; i < N; ++i) {
        Query q;
        q.type = "";
        q.Q = qLow(rng); q.P = pDist(rng);
        q.includeYellow = true;
        sB.push_back(q);
    }
    runScenario(fans, idx, sB, "Сценарий Б: без типа (дерево по Q_min)");

    // Сценарий В
    vector<Query> sC;
    for (unsigned int i = 0; i < N; ++i) {
        Query q;
        q.type = "XYZ_NOT_EXISTS";
        q.Q = qDist(rng); q.P = pDist(rng);
        q.includeYellow = true;
        sC.push_back(q);
    }
    runScenario(fans, idx, sC, "Сценарий В: несуществующий тип (Bloom)");

    pressEnter();
}

// 8. ВЕРИФИКАЦИЯ МАТЕМАТИЧЕСКОЙ МОДЕЛИ

void menuVerify(const vector<Fan>& fans, const Index& idx)
{
    cout << "\n  " << clr::BOLD << "=== Верификация модели ===" << clr::RST << "\n";

    // 1. Bloom P_fp: теория vs эмпирика
    cout << "\n  " << clr::CYN << "1. Bloom-фильтр: вероятность ложноположительного" << clr::RST << "\n";
    unsigned int n_types = (unsigned int)idx.byType.size();
    double p_theory = pow(1.0 - exp(-(double)BLOOM_K * n_types / BLOOM_M), BLOOM_K);
    printf("    Теория: P_fp = (1 - e^(-kn/m))^k = (1 - e^(-%u·%u/%u))^%u = %.2e\n",
           BLOOM_K, n_types, BLOOM_M, BLOOM_K, p_theory);

    const unsigned int N_TEST = 100000;
    unsigned int fp_count = 0;
    mt19937 rng(123);
    for (unsigned int i = 0; i < N_TEST; ++i) {
        string test = "TEST_" + to_string(rng());
        if (idx.bloomTypes.maybeContains(test)) fp_count++;
    }
    printf("    Эмпирика: %u ложноположительных из %u проверок = %.2e\n",
           fp_count, N_TEST, (double)fp_count / N_TEST);
    if (fp_count == 0)
        cout << "    " << clr::GRN << "Гипотеза подтверждена: P_fp приблизительно 0 на данной выборке ✓" << clr::RST << "\n";
    else
        cout << "    " << clr::YLW << "Обнаружены ложноположительные — увеличить m?" << clr::RST << "\n";

    // 2. Распределение r
    cout << "\n  " << clr::CYN << "2. Распределение коэффициента согласованности r" << clr::RST << "\n";
    unsigned int r_below1 = 0, r_1_15 = 0, r_15_10 = 0, r_10_30 = 0, r_above30 = 0, r_err = 0;
    for (const auto& f : fans) {
        if (f.status == "ERROR DETECTED") { r_err++; continue; }
        const double Q_mid = (f.flowMin + f.flowMax) / 2.0;
        const double P_mid = (f.pressureMin + f.pressureMax) / 2.0;
        const double N_hydr = (Q_mid * P_mid) / 3600.0;
        if (N_hydr <= 0) { r_err++; continue; }
        const double r = (f.power * 1000.0) / N_hydr;
        if (r < 1.0) r_below1++;
        else if (r < 1.5) r_1_15++;
        else if (r <= 10.0) r_15_10++;
        else if (r <= 30.0) r_10_30++;
        else r_above30++;
    }
    unsigned int total = (unsigned int)fans.size();
    printf("    r < 1.0      : %6u (%5.1f%%)  — заниженная мощность\n", r_below1, 100.0*r_below1/total);
    printf("    r in [1, 1.5) : %6u (%5.1f%%)  — высокий КПД\n", r_1_15, 100.0*r_1_15/total);
    printf("    r in [1.5, 10]: %6u (%5.1f%%)  — типичный диапазон\n", r_15_10, 100.0*r_15_10/total);
    printf("    r in (10, 30] : %6u (%5.1f%%)  — требует проверки\n", r_10_30, 100.0*r_10_30/total);
    printf("    r > 30       : %6u (%5.1f%%)  — критическое\n", r_above30, 100.0*r_above30/total);
    printf("    ERROR        : %6u (%5.1f%%)\n", r_err, 100.0*r_err/total);

    // 3. Покрытие предикатов
    cout << "\n  " << clr::CYN << "3. Покрытие предикатов п1–п5 (до/после нормализации)" << clr::RST << "\n";
    unsigned int pi1_fix = 0, pi2_fix = 0;
    for (const auto& f : fans)
        if (f.corrected) { pi1_fix++; pi2_fix++; }

    unsigned int pi3_fail = 0, pi4_fail = 0, pi5_fail = 0;
    for (const auto& f : fans) {
        if (f.flowMin <= 0 || f.flowMax <= 0) pi3_fail++;
        if (f.pressureMin <= 0 || f.pressureMax <= 0) pi4_fail++;
        if (f.power <= 0) pi5_fail++;
    }
    printf("    п1 (Q_min <= Q_max): %u записей исправлены swap\n", pi1_fix);
    printf("    п2 (P_min <= P_max): %u записей исправлены swap\n", pi2_fix);
    printf("    п3 (Q > 0):         %u нарушений после нормализации\n", pi3_fail);
    printf("    п4 (P > 0):         %u нарушений после нормализации\n", pi4_fail);
    printf("    п5 (N_y > 0):       %u нарушений после нормализации\n", pi5_fail);

    pressEnter();
}

void printErrorCodes()
{
    cout << "\n  " << clr::BOLD << "Коды ошибок данных:" << clr::RST << "\n";
    cout << "  E0  OK                  Ошибок нет\n";
    cout << "  E1  Parse error         Не удалось распознать число\n";
    cout << "  E2  Invalid range       Невалидный диапазон\n";
    cout << "  E3  Physical violation  Q <= 0 или P <= 0\n";
    cout << "  E4  Efficiency OOR      r вне допустимого [" << R_HARD_LOW << ", " << R_HARD_HIGH << "]\n";
    cout << "  E5  Invalid power       Мощность невалидна\n";
    cout << "  E6  Structure error     Деление на ноль\n";
    cout << "  E7  Empty field         Пустое поле\n";
    cout << "\n  " << clr::BOLD << "Коды ошибок программы:" << clr::RST << "\n";
    cout << "  P1  File not found      Файл не найден\n";
    cout << "  P2  Already normalized  Файл уже обработан\n";
    cout << "  P3  Invalid input       Некорректный ввод\n";
    cout << "  P4  No data loaded      Данные не загружены\n";
    pressEnter();
}

// 9. MAIN

int main()
{
    setupConsole();

    vector<Fan> fans;
    Index idx;
    bool dataReady = false;

    while (true) {
        clearScreen();
        cout << "\n";
        cout << "  " << clr::BLU << clr::BOLD;
        cout << "  Каталог вентиляторов\n";
        cout << "  Курсовая работа, Андреев А.А., 2026\n";
        cout << clr::RST << "\n\n";

        cout << "  " << clr::CYN << "1." << clr::RST << " Загрузить и нормализовать CSV\n";
        cout << "  " << clr::CYN << "2." << clr::RST << " Поиск\n";
        cout << "  " << clr::CYN << "3." << clr::RST << " Бенчмарк (линейный vs индексный)\n";
        cout << "  " << clr::CYN << "4." << clr::RST << " Верификация математической модели\n";
        cout << "  " << clr::CYN << "5." << clr::RST << " Справка: коды ошибок\n";
        cout << "  " << clr::CYN << "6." << clr::RST << " Выход\n\n";

        cout << "  Состояние: " << (dataReady
            ? (string(clr::GRN) + "загружено " + to_string(fans.size()) + " записей" + clr::RST)
            : (string(clr::DIM) + "данные не загружены" + clr::RST)) << "\n";
        cout << "  Выбор: ";

        string choice;
        if (!getline(cin, choice)) break;
        choice = trim(choice);
        if (choice.empty()) continue;

        if (choice == "1") { menuLoadAndNormalize(fans, idx, dataReady); }
        else if (choice == "2") {
            if (!dataReady) { cout << clr::RED << "  [P4] Сначала загрузите данные" << clr::RST << "\n"; pressEnter(); continue; }
            menuSearch(fans, idx);
        }
        else if (choice == "3") {
            if (!dataReady) { cout << clr::RED << "  [P4] Сначала загрузите данные" << clr::RST << "\n"; pressEnter(); continue; }
            menuBenchmark(fans, idx);
        }
        else if (choice == "4") {
            if (!dataReady) { cout << clr::RED << "  [P4] Сначала загрузите данные" << clr::RST << "\n"; pressEnter(); continue; }
            menuVerify(fans, idx);
        }
        else if (choice == "5") { printErrorCodes(); }
        else if (choice == "6") break;
        else { cout << clr::RED << "  [P3] Неизвестный пункт" << clr::RST << "\n"; pressEnter(); }
    }

    return 0;
}
