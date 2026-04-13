#include <iostream>
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
    INVALID_STRUCTURE
};

// -------------------- fan structure --------------------
struct Fan
{
    unsigned int id;

    double flowMin;
    double flowMax;
    double pressureMin;
    double pressureMax;

    double power;
    double efficiency;
    double diameter;

    string status;
    ErrorType errorType;

    bool corrected;
};

// -------------------- raw input --------------------
struct RawFan
{
    string flow;
    string pressure;
    string power;
    double efficiency;
};

// -------------------- helpers --------------------
string removeSpaces(const string& s)
{
    string res;
    for (char c : s)
        if (c != ' ' && c != '\t')
            res += c;
    return res;
}

string cleanNumber(string s)
{
    string res;

    for (char c : s)
        if (isdigit(c) || c == '.' || c == ',')
            res += c;

    for (char& c : res)
        if (c == ',') c = '.';

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

// -------------------- range parsing --------------------
bool parseRange(const string& input, double& minVal, double& maxVal, bool& corrected)
{
    string sMin;
    string sMax;

    size_t pos = input.find("\xE2\x80\x93");

    if (pos != string::npos)
    {
        sMin = input.substr(0, pos);
        sMax = input.substr(pos + 3);
    }
    else
    {
        size_t dash = input.find('-');
        if (dash == string::npos) return false;

        sMin = input.substr(0, dash);
        sMax = input.substr(dash + 1);
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
double normalizePower(string s, bool& corrected, ErrorType& err)
{
    s = removeSpaces(s);

    bool hasKW = (s.find("kW") != string::npos);
    bool hasW = (s.find("W") != string::npos);

    double value;

    if (!safeStod(cleanNumber(s), value))
    {
        err = PARSE_ERROR;
        return 0;
    }

    if (hasKW) return value;
    if (hasW) return value / 1000.0;

    double asKW = value;
    double asW = value / 1000.0;

    const double MIN = 0.01;
    const double MAX = 500.0;

    bool okKW = (asKW >= MIN && asKW <= MAX);
    bool okW = (asW >= MIN && asW <= MAX);

    if (okKW && !okW) return asKW;

    if (!okKW && okW)
    {
        corrected = true;
        return asW;
    }

    if (okKW && okW)
    {
        corrected = true;
        return min(asKW, asW);
    }

    err = INVALID_POWER;
    return asKW;
}

// -------------------- validation --------------------
void validateFan(Fan& f)
{
    if (f.errorType != NONE)
    {
        f.status = "ERROR DETECTED";
        return;
    }

    if (f.efficiency <= 0 || f.efficiency > 1)
    {
        f.errorType = INVALID_EFFICIENCY;
        f.status = "ERROR DETECTED";
        return;
    }

    if (f.flowMin <= 0 || f.flowMax <= 0 || f.pressureMin <= 0 || f.pressureMax <= 0)
    {
        f.errorType = INVALID_PHYSICAL_VALUE;
        f.status = "ERROR DETECTED";
        return;
    }

    if (f.power <= 0)
    {
        f.errorType = INVALID_POWER;
        f.status = "ERROR DETECTED";
        return;
    }

    double Q = (f.flowMin + f.flowMax) / 2.0;
    double P = (f.pressureMin + f.pressureMax) / 2.0;

    double Ny_theory = (Q / 3600.0) * P / f.efficiency;
    double Ny_actual = f.power * 1000.0;

    if (Ny_theory <= 0)
    {
        f.errorType = INVALID_STRUCTURE;
        f.status = "ERROR DETECTED";
        return;
    }

    double ratio = Ny_actual / Ny_theory;

    if (ratio < 0.1 || ratio > 10.0)
        f.status = "NEEDS CHECK";
    else if (f.corrected)
        f.status = "CORRECTED";
    else
        f.status = "CORRECT";
}

// -------------------- error message --------------------
string errorMessage(ErrorType e)
{
    switch (e)
    {
    case PARSE_ERROR:
        return "Parse error in numeric value";

    case INVALID_RANGE:
        return "Invalid range structure";

    case INVALID_PHYSICAL_VALUE:
        return "Physical constraints violated";

    case INVALID_EFFICIENCY:
        return "Efficiency out of range";

    case INVALID_POWER:
        return "Power value invalid or ambiguous";

    case INVALID_STRUCTURE:
        return "Invalid data structure or division error";

    default:
        return "No error";
    }
}

// -------------------- main --------------------
int main()
{
    setupConsole();

    vector<RawFan> data =
    {
        {"100-500", "200-400", "1 kW", 0.7},
        {"52-22", "300-100", "500 W", 0.6},
        {"100–500", "1000–2000", "2.5kW", 0.8},
        {"1 000-5 000", "50-200", "750", 0.65},
        {"≤300-~800", "100-300", "1000", 0.5},
        {"200-400", "???", "1.2kW", 0.7},
        {"-100-200", "100-200", "1kW", 0.6},
        {"300-600", "150-350", "0", 0.7},
        {"100-200", "100-200", "100000", 0.9},
        {"500-1000", "200-500", "5", 1.2}
    };

    cout << "PROCESSING DATA\n";

    for (int i = 0; i < (int)data.size(); i++)
    {
        Fan f;
        f.id = i + 1;
        f.corrected = false;
        f.errorType = NONE;

        bool r1 = parseRange(data[i].flow, f.flowMin, f.flowMax, f.corrected);
        bool r2 = parseRange(data[i].pressure, f.pressureMin, f.pressureMax, f.corrected);

        if (!r1 || !r2)
        {
            f.errorType = INVALID_RANGE;
        }

        f.power = normalizePower(data[i].power, f.corrected, f.errorType);
        f.efficiency = data[i].efficiency;

        validateFan(f);

        cout << "\nFan " << f.id << "\n";

        cout << "Status: " << f.status << "\n";
        cout << "Error: " << errorMessage(f.errorType) << "\n";
    }

    return 0;
}
