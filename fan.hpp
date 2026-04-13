#pragma once
#include <string>
using namespace std;

struct Fan
{
    unsigned int id;
    string type;       // “ип (¬ќ, ¬÷ и т.д.)
    string model;      // ћодель

    double flowMin, flowMax;        // Q, м3/ч
    double pressureMin, pressureMax; // p, ѕа

    double powerKw;    // ћощность в к¬т (конвертируем из ¬т при чтении)
    double noiseLevel; // ”ровень шума (дЅ)
    double price;      // ÷ена

    string status;     //  ќ––≈ “Ќќ / — ќ––≈ “»–ќ¬јЌќ / ќЎ»Ѕ ј
};
