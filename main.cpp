#include <iostream>
using namespace std;


#include <iostream>
#include <locale>

#ifdef _WIN32
#include <windows.h>
#endif

void setupConsole()
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}



static string removeSpaces(const string& s)
{
    string result;
    for (char c : s)
        if (c != ' ' && c != '\xC2')
            result += c;
    return result;
}










bool validateFan(Fan& fan)
{

    const double Q_MIN   = 0.1,    Q_MAX   = 100000.0; // м3/ч
    const double P_MIN   = 1.0,    P_MAX   = 10000.0;  // Па
    const double Ny_MIN  = 0.01,   Ny_MAX  = 500.0;    // кВт
    const double ETA_MIN = 0.1,    ETA_MAX = 1.0;      // КПД

    if (fan.flowMin  <= 0 || fan.flowMin  > Q_MAX)  return false; //если минимальный
    if (fan.flowMax  <= 0 || fan.flowMax  > Q_MAX)  return false;
    if (fan.flowMin  > fan.flowMax)                 return false;
    if (fan.pressureMin < P_MIN || fan.pressureMin > P_MAX) return false;
    if (fan.pressureMax < P_MIN || fan.pressureMax > P_MAX) return false;
    if (fan.power   < Ny_MIN  || fan.power   > Ny_MAX)     return false;
    if (fan.efficiency < ETA_MIN || fan.efficiency > ETA_MAX) return false;

    // Физическая согласованность: N_y = Q_avg * p_avg / (eta * 3600)
    // Проверяем что реальная мощность не отклоняется более чем в 3 раза
    double Q_avg = (fan.flowMin + fan.flowMax) / 2.0;
    double P_avg = (fan.pressureMin + fan.pressureMax) / 2.0;
    double Ny_theory = (Q_avg / 3600.0) * P_avg / fan.efficiency; // Вт
    double Ny_actual = fan.power * 1000.0; // кВт → Вт

    double ratio = Ny_actual / Ny_theory;
    if (ratio < 0.1 || ratio > 10.0)  // отклонение > порядок - подозрительно
        fan.status = "НУЖНА ПРОВЕРКА";
    else
        fan.status = "КОРРЕКТНО";

    return true;
}


bool parseRange(const string& input, double& minVal, double& maxVal)
{
    // Ищем разделитель: обычный дефис '-' или UTF-8 тире '–' (0xE2 0x80 0x93)
    size_t dash = string::npos;

    // Сначала ищем UTF-8 тире (3 байта: E2 80 93)
    size_t pos = input.find("\xE2\x80\x93");
    if (pos != string::npos)
    {
        dash = pos;
        string sMin = removeSpaces(input.substr(0, dash));
        string sMax = removeSpaces(input.substr(dash + 3)); // 3 байта тире
        try {
            minVal = stod(sMin);
            maxVal = stod(sMax);
        } catch (...) { return false; }
    }
    else
    {
        // Ищем ASCII дефис
        dash = input.find('-');
        if (dash == string::npos) return false;
        string sMin = removeSpaces(input.substr(0, dash));
        string sMax = removeSpaces(input.substr(dash + 1));
        try {
            minVal = stod(sMin);
            maxVal = stod(sMax);
        } catch (...) { return false; }
    }

    // Логическая проверка: min > max — переставляем (это и есть "52–22")
    if (minVal > maxVal)
        swap(minVal, maxVal);

    return true;
}



struct Fan
{
    unsigned int id;

    double flowMin, flowMax;            //Q^3 min/max
    double pressureMin, pressureMax;    //Pressure
    double power;                       // Ny
    double efficiency;                  // Ню
    string status;                      //корректно, скорректированно, нужна проверка

};






bool parseRange(const string& input, double& minVal, double& maxVal)
{
    size_t dash = input.find('-');
    if (dash==string::npos) return false;

    string sMin = input.substr(0,dash);
    string sMax = input.substr(dash+1);

    try
    {

    }
}







bool validateFan(Fan& fan)
{
    if (fan.flowRate <=0) return false;
    if
}







unsigned short int menu()
{
    cout<<"fansearcher ver.0.1"<<endl;
    cout<<"1.test"<<endl;
    cout<<"0.exit"<<endl;

    cout<<"choice: ";


    unsigned short int choice;
    cin>>choice;

    return choice;
}




int main()
{
    while (true) {
        unsigned short int choice = menu();
        switch(choice)
        {
            case 0:
                break;



            case 1:
                cout<< "test case 1"<<endl;
                break;



            default:
                cout<<"unknown command..."<<endl;
                break;
        }
    }
}
