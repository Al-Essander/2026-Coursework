#pragma once
#include <string>
#include <cctype>
using namespace std;

// убираем пробелы и мусорный байт 0xC2
static string removeSpaces(const string& s)
{
    string res;
    for (char c : s)
        if (c != ' ' && c != '\t' && (unsigned char)c != 0xC2)
            res += c;
    return res;
}

// оставляем цифры и точку, запятую меняем на точку
static string cleanNumber(const string& s)
{
    string res;
    for (char c : s)
        if (isdigit((unsigned char)c) || c == '.' || c == ',')
            res += (c == ',') ? '.' : c;
    return res;
}

// нормализация поля до парсинга:
// снимает BOM, заменяет UTF-8 тире на '-', убирает '\r'
static string normalizeField(const string& raw)
{
    string s;
    size_t i = 0;

    // BOM в начале поля (EF BB BF)
    if (raw.size() >= 3 &&
        (unsigned char)raw[0] == 0xEF &&
        (unsigned char)raw[1] == 0xBB &&
        (unsigned char)raw[2] == 0xBF)
        i = 3;

    for (; i < raw.size(); ++i)
    {
        unsigned char c = raw[i];

        if (c == '\r') continue; // мусор от Windows

        // UTF-8 тире U+2013 (E2 80 93) и U+2014 (E2 80 94) -> '-'
        if (c == 0xE2 && i + 2 < raw.size() &&
            (unsigned char)raw[i+1] == 0x80 &&
            ((unsigned char)raw[i+2] == 0x93 ||
             (unsigned char)raw[i+2] == 0x94))
        {
            s += '-';
            i += 2;
            continue;
        }

        s += (char)c;
    }
    return s;
}
