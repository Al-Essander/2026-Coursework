#include <iostream>
#include <string>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;





void setupConsole()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}





struct Fan
{
    unsigned int id;

    double flowMin, flowMax;        // Q
    double pressureMin, pressureMax;// p
    double power;                   // Ny (кВт)
    double efficiency;              // η

    string status;
};






string removeSpaces(const string& s)
{
    string res;
    for (char c : s)
        if (c != ' ' && c != '\t' && (unsigned char)c != 0xC2)
            res += c;
    return res;
}





string cleanNumber(string s)
{
    string res;

    for (char c : s)
    {
        if (isdigit(c) || c == '.' || c == ',')
            res += c;
    }

    // заменяем , на .
    for (char& c : res)
        if (c == ',') c = '.';

    return res;
}






double normalizePower(string s)
{
    s = removeSpaces(s);

    // кВт
    if (s.find("kW") != string::npos || s.find("кВт") != string::npos)
    {
        s = cleanNumber(s);
        return stod(s);
    }

    // Вт
    if (s.find("W") != string::npos || s.find("Вт") != string::npos)
    {
        s = cleanNumber(s);
        return stod(s) / 1000.0;
    }

    // без единиц - считаем кВт
    return stod(cleanNumber(s));
}





bool parseRange(const string& input, double& minVal, double& maxVal)
{
    string sMin, sMax;

    // UTF-8 тире
    size_t pos = input.find("\xE2\x80\x93");

    if (pos != string::npos)
    {
        sMin = removeSpaces(input.substr(0, pos));
        sMax = removeSpaces(input.substr(pos + 3));
    }
    else
    {
        size_t dash = input.find('-');
        if (dash == string::npos) return false;

        sMin = removeSpaces(input.substr(0, dash));
        sMax = removeSpaces(input.substr(dash + 1));
    }

    try
    {
        minVal = stod(cleanNumber(sMin));
        maxVal = stod(cleanNumber(sMax));
    }
    catch (...) { return false; }

    if (minVal > maxVal)
        swap(minVal, maxVal);

    return true;
}





bool validateFan(Fan& fan)
{
    if (fan.flowMin <= 0 || fan.flowMax <= 0) return false;
    if (fan.flowMin > fan.flowMax) return false;

    if (fan.pressureMin <= 0 || fan.pressureMax <= 0) return false;

    if (fan.power <= 0) return false;

    if (fan.efficiency <= 0 || fan.efficiency > 1) return false;

    // физическая проверка
    double Q = (fan.flowMin + fan.flowMax) / 2.0;
    double P = (fan.pressureMin + fan.pressureMax) / 2.0;

    double Ny_theory = (Q / 3600.0) * P / fan.efficiency;
    double Ny_actual = fan.power * 1000.0;

    double ratio = Ny_actual / Ny_theory;

    if (ratio < 0.1 || ratio > 10.0)
        fan.status = "НУЖНА ПРОВЕРКА";
    else
        fan.status = "КОРРЕКТНО";

    return true;
}






unsigned short menu()
{
    cout << "\nfansearcher v0.3\n";
    cout << "1. тест диапазонов\n";
    cout << "2. тест мощности\n";
    cout << "0. выход\n";
    cout << "choice: ";

    unsigned short c;
    cin >> c;
    return c;
}




int main()
{
    setupConsole();

    while (true)
    {
        switch (menu())
        {
        case 0:
            return 0;

        case 1:
        {
            string tests[] = {
                "100-500",
                "52-22",
                "100–500",
                "1 140-5 000",
                "≤300-~800"
            };

            for (string t : tests)
            {
                double lo, hi;

                if (parseRange(t, lo, hi))
                    cout << t << " -> [" << lo << ", " << hi << "]\n";
                else
                    cout << t << " -> ERROR\n";
            }

            break;
        }

        case 2:
        {
            string tests[] = {
                "1000 W",
                "1 kW",
                "500",
                "2.5kW",
                "750Вт"
            };

            for (string t : tests)
            {
                try
                {
                    double val = normalizePower(t);
                    cout << t << " -> " << val << " kW\n";
                }
                catch (...)
                {
                    cout << t << " -> ERROR\n";
                }
            }

            break;
        }

        default:
            cout << "unknown command\n";
        }
    }
}
