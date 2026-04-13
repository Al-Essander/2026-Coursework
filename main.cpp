#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// -------------------- console setup --------------------
void setupConsole()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

// -------------------- error types --------------------
enum ErrorType
{
    NONE,
    PARSE_ERROR,
    INVALID_RANGE,
    INVALID_PHYSICAL_VALUE,
    INVALID_EFFICIENCY,
    INVALID_POWER,
    INVALID_STRUCTURE,
    EMPTY_FIELD
};

// -------------------- fan structure --------------------
struct Fan
{
    unsigned int id;

    string type;
    string model;
    string size;

    unsigned short diameter;

    double flowMin;
    double flowMax;
    double pressureMin;
    double pressureMax;

    double power;        // kW
    double efficiency;   // 0..1
    unsigned int price;

    string status;
    ErrorType errorType;

    bool corrected;
    bool effImputed;     // КПД восстановлен по формуле
};

// -------------------- raw input --------------------
struct RawFan
{
    string id;
    string type;
    string model;
    string size;
    string diameter;
    string flow;
    string pressure;
    string power;
    string noise;
    string price;
};

// -------------------- helpers --------------------
string removeSpaces(const string& s)
{
    string res;
    for (unsigned int i = 0; i < s.size(); i++)
    {
        unsigned char c = (unsigned char)s[i];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            continue;

        // неразрывный пробел U+00A0 в UTF-8: 0xC2 0xA0
        if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xA0)
        {
            i++;
            continue;
        }

        res += s[i];
    }

    return res;
}

string stripBOM(const string& s)
{
    // UTF-8 BOM: EF BB BF
    if (s.size() >= 3 &&
        (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB &&
        (unsigned char)s[2] == 0xBF)
    {
        return s.substr(3);
    }

    return s;
}

string cleanNumber(string s)
{
    string res;

    for (unsigned int i = 0; i < s.size(); i++)
    {
        char c = s[i];

        if (isdigit((unsigned char)c) || c == '.' || c == ',' || c == '-')
            res += c;
    }

    for (unsigned int i = 0; i < res.size(); i++)
        if (res[i] == ',')
            res[i] = '.';

    return res;
}

bool safeStod(const string& s, double& out)
{
    try
    {
        if (s.empty()) return false;
        out = stod(s);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool safeStou(const string& s, unsigned int& out)
{
    try
    {
        if (s.empty()) return false;
        long v = stol(s);
        if (v < 0) return false;
        out = (unsigned int)v;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool isEmptyField(const string& s)
{
    string t = removeSpaces(s);

    if (t.empty()) return true;
    if (t == "-" || t == "—" || t == "?" || t == "??" || t == "???") return true;
    if (t == "N/A" || t == "n/a" || t == "NA") return true;

    return false;
}

// -------------------- range parsing --------------------
bool parseRange(const string& input, double& minVal, double& maxVal, bool& corrected)
{
    if (isEmptyField(input)) return false;

    string sMin;
    string sMax;
    bool found = false;

    // 1) en-dash U+2013 (E2 80 93)
    size_t pos = input.find("\xE2\x80\x93");
    if (pos != string::npos)
    {
        sMin = input.substr(0, pos);
        sMax = input.substr(pos + 3);
        found = true;
    }

    // 2) em-dash U+2014 (E2 80 94)
    if (!found)
    {
        pos = input.find("\xE2\x80\x94");
        if (pos != string::npos)
        {
            sMin = input.substr(0, pos);
            sMax = input.substr(pos + 3);
            found = true;
        }
    }

    // 3) ASCII '-' — но осторожно, чтобы не схватить ведущий минус
    if (!found)
    {
        // ищем '-', окружённое цифрами или пробелами после первого символа
        for (unsigned int i = 1; i < input.size(); i++)
        {
            if (input[i] == '-')
            {
                sMin = input.substr(0, i);
                sMax = input.substr(i + 1);
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        // одиночное число — диапазон вырожден в точку
        double v;
        if (!safeStod(cleanNumber(removeSpaces(input)), v)) return false;
        minVal = v;
        maxVal = v;
        return true;
    }

    sMin = removeSpaces(sMin);
    sMax = removeSpaces(sMax);

    double a;
    double b;

    if (!safeStod(cleanNumber(sMin), a)) return false;
    if (!safeStod(cleanNumber(sMax), b)) return false;

    if (a > b)
    {
        swap(a, b);
        corrected = true;
    }

    minVal = a;
    maxVal = b;

    return true;
}

// -------------------- power normalization --------------------
// Каталог хранит мощность в ваттах без суффикса.
// Если суффикс есть — уважаем его; иначе предикат допустимости [0.01..500] кВт.
double normalizePower(string s, bool& corrected, ErrorType& err)
{
    if (isEmptyField(s))
    {
        err = EMPTY_FIELD;
        return 0;
    }

    string lower = s;
    for (unsigned int i = 0; i < lower.size(); i++)
        lower[i] = (char)tolower((unsigned char)lower[i]);

    bool hasKW = (lower.find("kw") != string::npos || lower.find("квт") != string::npos);
    bool hasW = (!hasKW) && (lower.find("w") != string::npos || lower.find("вт") != string::npos);

    s = removeSpaces(s);

    double value;
    if (!safeStod(cleanNumber(s), value))
    {
        err = PARSE_ERROR;
        return 0;
    }

    if (hasKW) return value;
    if (hasW)  return value / 1000.0;

    // Без суффикса: это каталожный кейс — почти всегда ватты.
    // Проверяем оба варианта через предикат допустимости.
    const double MIN_KW = 0.01;
    const double MAX_KW = 500.0;

    double asKW = value;
    double asW = value / 1000.0;

    bool okKW = (asKW >= MIN_KW && asKW <= MAX_KW);
    bool okW = (asW >= MIN_KW && asW <= MAX_KW);

    if (okW && !okKW)
    {
        // 22000 → 22 кВт. Это нормальная интерпретация каталога (Вт без суффикса),
        // не коррекция данных.
        return asW;
    }

    if (okKW && !okW)
    {
        // 5 → 5 кВт
        return asKW;
    }

    if (okKW && okW)
    {
        // оба валидны — по умолчанию каталога: ватты
        return asW;
    }

    err = INVALID_POWER;
    return asW;
}

// -------------------- validation --------------------
void validateFan(Fan& f)
{
    if (f.errorType != NONE)
    {
        f.status = "ERROR DETECTED";
        return;
    }

    // π_3, π_4: положительность Q, P
    if (f.flowMin <= 0 || f.flowMax <= 0 || f.pressureMin <= 0 || f.pressureMax <= 0)
    {
        f.errorType = INVALID_PHYSICAL_VALUE;
        f.status = "ERROR DETECTED";
        return;
    }

    // π_5: положительность мощности
    if (f.power <= 0)
    {
        f.errorType = INVALID_POWER;
        f.status = "ERROR DETECTED";
        return;
    }

    double Q_mid = (f.flowMin + f.flowMax) / 2.0;
    double P_mid = (f.pressureMin + f.pressureMax) / 2.0;

    // Эталонный КПД (типичное значение для вентиляторов)
    const double ETA_REF = 0.65;

    // Расчётная мощность по формуле в эталонной точке
    double Ny_W = f.power * 1000.0;
    double Ny_theory_W = (Q_mid * P_mid) / (ETA_REF * 3600.0);

    if (Ny_theory_W <= 0)
    {
        f.errorType = INVALID_STRUCTURE;
        f.status = "ERROR DETECTED";
        return;
    }

    // Коэффициент согласованности
    double ratio = Ny_W / Ny_theory_W;

    // Восстанавливаем эффективный η через диапазон рабочей точки.
    // Берём минимальную точку (Q_min, P_min) — даёт верхнюю оценку η.
    double eta_at_min = (f.flowMin * f.pressureMin) / (Ny_W * 3600.0);
    double eta_at_max = (f.flowMax * f.pressureMax) / (Ny_W * 3600.0);

    // Записываем оценку (может выйти за (0, 1] — это сигнал, не ошибка)
    f.efficiency = eta_at_min;
    f.effImputed = true;

    // π_6 в мягкой форме: хотя бы при одной точке диапазона η ∈ (0, 1]
    bool eta_plausible = false;

    if (eta_at_min > 0 && eta_at_min <= 1.0) eta_plausible = true;
    if (eta_at_max > 0 && eta_at_max <= 1.0) eta_plausible = true;

    // Δ = расхождение от эталона
    double delta_pct = ratio > 1.0 ? (ratio - 1.0) * 100.0 : (1.0 - ratio) * 100.0;

    if (!eta_plausible && delta_pct > 1000.0)
    {
        // Жёсткая физическая невозможность ни в одной точке диапазона
        f.errorType = INVALID_EFFICIENCY;
        f.status = "ERROR DETECTED";
        return;
    }

    // Δ < 100% и предикат π_6 хотя бы локально → согласовано
    if (delta_pct < 100.0 && eta_plausible)
    {
        f.status = f.corrected ? "CORRECTED" : "CORRECT";
    }
    else if (delta_pct < 500.0)
    {
        f.status = "NEEDS CHECK";
    }
    else
    {
        f.status = "NEEDS CHECK"; // не отбрасываем, но помечаем
    }
}

// -------------------- error message --------------------
string errorMessage(ErrorType e)
{
    switch (e)
    {
    case PARSE_ERROR:            return "Parse error in numeric value";
    case INVALID_RANGE:          return "Invalid range structure";
    case INVALID_PHYSICAL_VALUE: return "Physical constraints violated";
    case INVALID_EFFICIENCY:     return "Efficiency out of range";
    case INVALID_POWER:          return "Power value invalid or ambiguous";
    case INVALID_STRUCTURE:      return "Invalid data structure or division error";
    case EMPTY_FIELD:            return "Empty or unparseable field";
    default:                     return "No error";
    }
}

// -------------------- CSV split --------------------
vector<string> splitCSV(const string& line, char sep)
{
    vector<string> out;
    string cur;

    for (unsigned int i = 0; i < line.size(); i++)
    {
        char c = line[i];

        if (c == sep)
        {
            out.push_back(cur);
            cur.clear();
        }
        else if (c == '\r')
        {
            // съедаем
        }
        else
        {
            cur += c;
        }
    }

    out.push_back(cur);
    return out;
}

// -------------------- CSV load --------------------
bool loadCSV(const string& path, vector<RawFan>& out)
{
    ifstream in(path);
    if (!in.is_open())
    {
        cerr << "Cannot open input: " << path << "\n";
        return false;
    }

    string line;
    bool firstLine = true;
    char sep = ';';

    while (getline(in, line))
    {
        if (firstLine)
        {
            line = stripBOM(line);
            firstLine = false;

            // авто-детект разделителя по шапке
            if (line.find(';') == string::npos && line.find(',') != string::npos)
                sep = ',';

            continue;
        }

        if (line.empty()) continue;

        vector<string> cols = splitCSV(line, sep);

        // ожидаем минимум 8 колонок
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

// -------------------- CSV save --------------------
bool saveCSV(const string& path, const vector<Fan>& fans)
{
    ofstream out(path, ios::binary);
    if (!out.is_open())
    {
        cerr << "Cannot open output: " << path << "\n";
        return false;
    }

    // UTF-8 BOM — без неё Excel на Windows читает кириллицу как мусор
    out << "\xEF\xBB\xBF";

    out << "id;type;model;size;diameter;Q_min;Q_max;P_min;P_max;N_y_kW;eta;price;status;corrected;eff_imputed;error\r\n";

    for (unsigned int i = 0; i < fans.size(); i++)
    {
        const Fan& f = fans[i];

        out << f.id << ';';
        out << f.type << ';';
        out << f.model << ';';
        out << f.size << ';';
        out << f.diameter << ';';

        out.precision(2);
        out << fixed;

        out << f.flowMin << ';' << f.flowMax << ';';
        out << f.pressureMin << ';' << f.pressureMax << ';';

        out.precision(4);
        out << f.power << ';';
        out << f.efficiency << ';';

        out.unsetf(ios::fixed);

        out << f.price << ';';
        out << f.status << ';';
        out << (f.corrected ? "1" : "0") << ';';
        out << (f.effImputed ? "1" : "0") << ';';
        out << errorMessage(f.errorType) << "\r\n";
    }

    return true;
}

// -------------------- multi-criteria filter --------------------
struct Query
{
    double Q_min;
    double Q_max;
    double P_min;
    double P_max;
    string type;       // пусто = любой
    bool requireGreen; // только CORRECT
};

void filter(const vector<Fan>& fans, const Query& q, vector<unsigned int>& matches)
{
    matches.clear();

    for (unsigned int i = 0; i < fans.size(); i++)
    {
        const Fan& f = fans[i];

        if (f.status == "ERROR DETECTED") continue;
        if (q.requireGreen && f.status != "CORRECT") continue;

        if (!q.type.empty() && f.type != q.type) continue;

        // диапазон Q пересекается с запросом
        if (f.flowMax < q.Q_min || f.flowMin > q.Q_max) continue;
        if (f.pressureMax < q.P_min || f.pressureMin > q.P_max) continue;

        matches.push_back(i);
    }
}

// -------------------- main --------------------
int main(int argc, char** argv)
{
    setupConsole();

    string inPath = "ventsearch_massive_sorted.csv";
    string outPath = "ventsearch_normalized.csv";

    if (argc >= 2) inPath = argv[1];
    if (argc >= 3) outPath = argv[2];

    cout << "Loading: " << inPath << "\n";

    vector<RawFan> raw;
    if (!loadCSV(inPath, raw)) return 1;

    cout << "Loaded raw records: " << raw.size() << "\n";

    vector<Fan> fans;
    fans.reserve(raw.size());

    unsigned int cntCorrect = 0;
    unsigned int cntCorrected = 0;
    unsigned int cntCheck = 0;
    unsigned int cntError = 0;

    for (unsigned int i = 0; i < raw.size(); i++)
    {
        Fan f;
        f.id = 0;
        f.type = raw[i].type;
        f.model = raw[i].model;
        f.size = raw[i].size;
        f.diameter = 0;
        f.flowMin = 0; f.flowMax = 0;
        f.pressureMin = 0; f.pressureMax = 0;
        f.power = 0;
        f.efficiency = 0;
        f.price = 0;
        f.status = "";
        f.errorType = NONE;
        f.corrected = false;
        f.effImputed = false;

        // id
        unsigned int idVal;
        if (safeStou(removeSpaces(raw[i].id), idVal)) f.id = idVal;

        // diameter
        unsigned int dVal;
        if (safeStou(removeSpaces(raw[i].diameter), dVal) && dVal < 65535)
            f.diameter = (unsigned short)dVal;

        // price (цена в рублях, может быть с пробелами)
        unsigned int pVal;
        if (safeStou(removeSpaces(raw[i].price), pVal)) f.price = pVal;

        // flow / pressure
        bool r1 = parseRange(raw[i].flow, f.flowMin, f.flowMax, f.corrected);
        bool r2 = parseRange(raw[i].pressure, f.pressureMin, f.pressureMax, f.corrected);

        if (!r1 || !r2)
        {
            f.errorType = INVALID_RANGE;
        }

        // power
        if (f.errorType == NONE)
        {
            f.power = normalizePower(raw[i].power, f.corrected, f.errorType);
        }

        validateFan(f);

        fans.push_back(f);

        if (f.status == "CORRECT")        cntCorrect++;
        else if (f.status == "CORRECTED") cntCorrected++;
        else if (f.status == "NEEDS CHECK") cntCheck++;
        else cntError++;
    }

    cout << "\n--- Normalization summary ---\n";
    cout << "CORRECT     : " << cntCorrect << "\n";
    cout << "CORRECTED   : " << cntCorrected << "\n";
    cout << "NEEDS CHECK : " << cntCheck << "\n";
    cout << "ERROR       : " << cntError << "\n";
    cout << "TOTAL       : " << fans.size() << "\n";

    if (!saveCSV(outPath, fans)) return 1;
    cout << "\nSaved to: " << outPath << "\n";

    // -------- демонстрация фильтрации --------
    cout << "\n--- Demo query ---\n";
    cout << "type=ВО, Q in [1000..5000], P in [200..1000]\n";

    Query q;
    q.Q_min = 1000; q.Q_max = 5000;
    q.P_min = 200;  q.P_max = 1000;
    q.type = "ВО";
    q.requireGreen = false;

    vector<unsigned int> matches;
    filter(fans, q, matches);

    cout << "Matches: " << matches.size() << "\n";

    unsigned int show = matches.size() < 5 ? (unsigned int)matches.size() : 5;
    for (unsigned int k = 0; k < show; k++)
    {
        const Fan& f = fans[matches[k]];
        cout << "  id=" << f.id
             << "  " << f.model
             << "  Q=[" << f.flowMin << ".." << f.flowMax << "]"
             << "  P=[" << f.pressureMin << ".." << f.pressureMax << "]"
             << "  N_y=" << f.power << " kW"
             << "  status=" << f.status
             << "\n";
    }

    return 0;
}
