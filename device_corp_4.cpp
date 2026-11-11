// «Девайс-Корп» — симулятор компании по разработке электроники.
// Один файл, C++14 + SDL2. Без внешних ассетов: графика — примитивы, шрифт — зашитый массив глифов,
// звук — процедурный синтез.
// Сборка: g++ -std=c++14 -O2 -Wall -Wextra device_corp.cpp -o device_corp $(sdl2-config --cflags --libs)

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ============================================================================
//  Базовое
// ============================================================================
const int W = 1280, H = 720;
SDL_Renderer* R = nullptr;
std::mt19937 rng(12345u);

double frand() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); }
int irand(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }
double clampd(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }

struct Col { Uint8 r, g, b, a; };
const Col C_BG     = {16, 20, 32, 255};
const Col C_PANEL  = {28, 34, 52, 255};
const Col C_PANEL2 = {22, 27, 42, 255};
const Col C_BORDER = {58, 70, 104, 255};
const Col C_TEXT   = {232, 236, 246, 255};
const Col C_DIM    = {140, 152, 178, 255};
const Col C_ACC    = {70, 180, 255, 255};
const Col C_GREEN  = {80, 210, 120, 255};
const Col C_RED    = {235, 85, 85, 255};
const Col C_YEL    = {255, 214, 40, 255};
const Col C_ORANGE = {255, 150, 60, 255};
const Col C_BTN    = {48, 60, 92, 255};
const Col C_BTNH   = {66, 82, 124, 255};
const Col C_BTNSEL = {36, 120, 200, 255};
const Col C_BTND   = {36, 40, 54, 255};

Col shade(Col c, double k) {
    Col o;
    o.r = (Uint8)clampi((int)(c.r * k), 0, 255);
    o.g = (Uint8)clampi((int)(c.g * k), 0, 255);
    o.b = (Uint8)clampi((int)(c.b * k), 0, 255);
    o.a = c.a;
    return o;
}

void setc(Col c) { SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a); }
void fillRect(int x, int y, int w, int h, Col c) {
    if (w <= 0 || h <= 0) return;
    setc(c);
    SDL_Rect rr = {x, y, w, h};
    SDL_RenderFillRect(R, &rr);
}
void fillRect(const SDL_Rect& r, Col c) { fillRect(r.x, r.y, r.w, r.h, c); }
void drawRect(const SDL_Rect& r, Col c, int t = 1) {
    fillRect(r.x, r.y, r.w, t, c);
    fillRect(r.x, r.y + r.h - t, r.w, t, c);
    fillRect(r.x, r.y, t, r.h, c);
    fillRect(r.x + r.w - t, r.y, t, r.h, c);
}
void fillCircle(int cx, int cy, int rad, Col c) {
    setc(c);
    for (int dy = -rad; dy <= rad; ++dy) {
        int dx = (int)std::sqrt((double)(rad * rad - dy * dy));
        SDL_RenderDrawLine(R, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}
void fillRound(SDL_Rect r, int rad, Col c) {
    rad = std::min(rad, std::min(r.w, r.h) / 2);
    if (rad <= 0) { fillRect(r, c); return; }
    fillRect(r.x + rad, r.y, r.w - 2 * rad, r.h, c);
    fillRect(r.x, r.y + rad, rad, r.h - 2 * rad, c);
    fillRect(r.x + r.w - rad, r.y + rad, rad, r.h - 2 * rad, c);
    fillCircle(r.x + rad, r.y + rad, rad, c);
    fillCircle(r.x + r.w - rad - 1, r.y + rad, rad, c);
    fillCircle(r.x + rad, r.y + r.h - rad - 1, rad, c);
    fillCircle(r.x + r.w - rad - 1, r.y + r.h - rad - 1, rad, c);
}
// Вертикальный градиент (для фонов)
void fillGradV(SDL_Rect r, Col top, Col bot) {
    for (int i = 0; i < r.h; ++i) {
        double t = r.h > 1 ? (double)i / (r.h - 1) : 0.0;
        Col c = {(Uint8)(top.r + (int)((bot.r - top.r) * t)), (Uint8)(top.g + (int)((bot.g - top.g) * t)),
                 (Uint8)(top.b + (int)((bot.b - top.b) * t)), 255};
        fillRect(r.x, r.y + i, r.w, 1, c);
    }
}
bool inRect(int x, int y, const SDL_Rect& r) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

// ============================================================================
//  Растровый шрифт 5x7 с кириллицей (строчные рисуются капителью)
// ============================================================================
struct Glyph { unsigned cp; unsigned char r[7]; };
const Glyph FONT[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
    {'"', {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'#', {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}},
    {'$', {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04}},
    {'%', {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03}},
    {'\'', {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    {'*', {0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {'/', {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {';', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08}},
    {'<', {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}},
    {'=', {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}},
    {'>', {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}},
    {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {'[', {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}},
    {']', {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}},
    {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}},
    {'|', {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {0xAB, {0x00, 0x05, 0x0A, 0x14, 0x0A, 0x05, 0x00}},     // «
    {0xBB, {0x00, 0x14, 0x0A, 0x05, 0x0A, 0x14, 0x00}},     // »
    {0x2014, {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},   // —
    {0x2248, {0x00, 0x08, 0x15, 0x02, 0x08, 0x15, 0x02}},   // ≈
    {0x2265, {0x08, 0x04, 0x02, 0x04, 0x08, 0x00, 0x1F}},   // ≥
    // Кириллица (прописные)
    {0x0401, {0x0A, 0x1F, 0x10, 0x1E, 0x10, 0x10, 0x1F}},   // Ё
    {0x0410, {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},   // А
    {0x0411, {0x1F, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x1E}},   // Б
    {0x0412, {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},   // В
    {0x0413, {0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}},   // Г
    {0x0414, {0x06, 0x0A, 0x0A, 0x0A, 0x0A, 0x1F, 0x11}},   // Д
    {0x0415, {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},   // Е
    {0x0416, {0x15, 0x15, 0x15, 0x0E, 0x15, 0x15, 0x15}},   // Ж
    {0x0417, {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E}},   // З
    {0x0418, {0x11, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11}},   // И
    {0x0419, {0x0A, 0x04, 0x11, 0x13, 0x15, 0x19, 0x11}},   // Й
    {0x041A, {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},   // К
    {0x041B, {0x07, 0x09, 0x09, 0x09, 0x09, 0x09, 0x11}},   // Л
    {0x041C, {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},   // М
    {0x041D, {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},   // Н
    {0x041E, {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},   // О
    {0x041F, {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}},   // П
    {0x0420, {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},   // Р
    {0x0421, {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},   // С
    {0x0422, {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},   // Т
    {0x0423, {0x11, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E}},   // У
    {0x0424, {0x04, 0x0E, 0x15, 0x15, 0x15, 0x0E, 0x04}},   // Ф
    {0x0425, {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},   // Х
    {0x0426, {0x12, 0x12, 0x12, 0x12, 0x12, 0x1F, 0x01}},   // Ц
    {0x0427, {0x11, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x01}},   // Ч
    {0x0428, {0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x1F}},   // Ш
    {0x0429, {0x15, 0x15, 0x15, 0x15, 0x15, 0x1F, 0x01}},   // Щ
    {0x042A, {0x18, 0x08, 0x08, 0x0E, 0x09, 0x09, 0x0E}},   // Ъ
    {0x042B, {0x11, 0x11, 0x11, 0x1D, 0x13, 0x13, 0x1D}},   // Ы
    {0x042C, {0x10, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x1E}},   // Ь
    {0x042D, {0x0E, 0x11, 0x01, 0x07, 0x01, 0x11, 0x0E}},   // Э
    {0x042E, {0x12, 0x15, 0x15, 0x1D, 0x15, 0x15, 0x12}},   // Ю
    {0x042F, {0x0F, 0x11, 0x11, 0x0F, 0x05, 0x09, 0x11}},   // Я
};
const int FONT_N = (int)(sizeof(FONT) / sizeof(FONT[0]));
int g_glyphIdx[0x460];

void initFont() {
    for (int i = 0; i < 0x460; ++i) g_glyphIdx[i] = -1;
    for (int k = 0; k < FONT_N; ++k)
        if (FONT[k].cp < 0x460) g_glyphIdx[FONT[k].cp] = k;
}

unsigned nextCp(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { i += 1; return c; }
    if ((c >> 5) == 6 && i + 1 < s.size()) {
        unsigned cp = ((unsigned)(c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3Fu);
        i += 2; return cp;
    }
    if ((c >> 4) == 14 && i + 2 < s.size()) {
        unsigned cp = ((unsigned)(c & 0x0F) << 12) | (((unsigned char)s[i + 1] & 0x3Fu) << 6) |
                      ((unsigned char)s[i + 2] & 0x3Fu);
        i += 3; return cp;
    }
    if ((c >> 3) == 30) { i += 4; return '?'; }
    i += 1;
    return '?';
}

const unsigned char* glyphFor(unsigned cp, bool& small) {
    small = false;
    if (cp >= 'a' && cp <= 'z') { cp -= 32; small = true; }
    else if (cp >= 0x430 && cp <= 0x44F) { cp -= 0x20; small = true; }
    else if (cp == 0x451) { cp = 0x401; small = true; }
    if (cp < 0x460) {
        int k = g_glyphIdx[cp];
        if (k >= 0) return FONT[k].r;
    } else {
        for (int k = 0; k < FONT_N; ++k) if (FONT[k].cp == cp) return FONT[k].r;
    }
    return FONT[g_glyphIdx[(unsigned)'?']].r;
}

int cpLen(const std::string& s) {
    int n = 0; size_t i = 0;
    while (i < s.size()) { nextCp(s, i); ++n; }
    return n;
}
int textW(const std::string& s, int sc) { int n = cpLen(s); return n > 0 ? n * 6 * sc - sc : 0; }

std::vector<SDL_Rect> g_rects;
void drawText(const std::string& s, int x, int y, int sc, Col c) {
    g_rects.clear();
    size_t i = 0;
    int cx = x;
    while (i < s.size()) {
        unsigned cp = nextCp(s, i);
        bool small;
        const unsigned char* g = glyphFor(cp, small);
        for (int r = 0; r < 7; ++r) {
            int y0, y1;
            if (small) {
                y0 = y + 2 * sc + (r * 5 * sc) / 7;
                y1 = y + 2 * sc + ((r + 1) * 5 * sc) / 7;
                if (y1 <= y0) y1 = y0 + 1;
            } else {
                y0 = y + r * sc; y1 = y0 + sc;
            }
            for (int col = 0; col < 5; ++col)
                if (g[r] & (0x10 >> col)) {
                    SDL_Rect rr = {cx + col * sc, y0, sc, y1 - y0};
                    g_rects.push_back(rr);
                }
        }
        cx += 6 * sc;
    }
    if (!g_rects.empty()) {
        setc(c);
        SDL_RenderFillRects(R, g_rects.data(), (int)g_rects.size());
    }
}
std::string fitText(const std::string& s, int sc, int maxw) {
    if (textW(s, sc) <= maxw) return s;
    std::vector<size_t> starts;
    size_t i = 0;
    while (i < s.size()) { starts.push_back(i); nextCp(s, i); }
    for (int n = (int)starts.size() - 1; n > 0; --n) {
        std::string t = s.substr(0, starts[(size_t)n]) + "..";
        if (textW(t, sc) <= maxw) return t;
    }
    return "..";
}
void drawTextC(const std::string& s, int cx, int y, int sc, Col c) { drawText(s, cx - textW(s, sc) / 2, y, sc, c); }
void drawTextR(const std::string& s, int rx, int y, int sc, Col c) { drawText(s, rx - textW(s, sc), y, sc, c); }
// Текст с тенью: заголовки выглядят объёмнее
void drawTextShadow(const std::string& t, int x, int y, int sc, Col c) {
    drawText(t, x + sc, y + sc, sc, shade(C_BG, 0.4));
    drawText(t, x, y, sc, c);
}
void drawTextCShadow(const std::string& t, int cx, int y, int sc, Col c) {
    drawTextShadow(t, cx - textW(t, sc) / 2, y, sc, c);
}

std::string fmtNum(double v) {
    long long n = (long long)std::llround(std::fabs(v));
    std::string s = std::to_string(n), o;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        o.insert(o.begin(), s[(size_t)i]);
        if (++c % 3 == 0 && i > 0) o.insert(o.begin(), ' ');
    }
    return (v < -0.5 ? std::string("-") : std::string()) + o;
}
std::string fmtMoney(double v) {
    std::string n = fmtNum(std::fabs(v));
    return (v < -0.5 ? "-$" : "$") + n;
}
std::string fmtShort(double v) {
    char b[32];
    double a = std::fabs(v);
    if (a >= 1e6) std::snprintf(b, sizeof b, "%.1fМ", v / 1e6);
    else if (a >= 1e3) std::snprintf(b, sizeof b, "%.0fк", v / 1e3);
    else std::snprintf(b, sizeof b, "%.0f", v);
    return b;
}
std::string fmt1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); return b; }

// ============================================================================
//  Процедурный звук
// ============================================================================
enum { SFX_CLICK, SFX_BACK, SFX_ERR, SFX_NAV, SFX_CASH, SFX_DONE, SFX_LAUNCH, SFX_BAD };
struct Voice { float f0, f1, dur, t, delay, vol, ph; int wave; };
struct AudioSys {
    SDL_AudioDeviceID dev = 0;
    int rate = 44100;
    std::vector<Voice> v;
    float vol = 0.6f;
    Uint32 seed = 22222u;
};
AudioSys A;

void SDLCALL audioCb(void* ud, Uint8* stream, int len) {
    AudioSys* a = static_cast<AudioSys*>(ud);
    Sint16* out = reinterpret_cast<Sint16*>(stream);
    int n = len / 2;
    float dt = 1.0f / (float)a->rate;
    for (int i = 0; i < n; ++i) {
        float s = 0.0f;
        for (size_t k = 0; k < a->v.size(); ++k) {
            Voice& vc = a->v[k];
            if (vc.delay > 0.0f) { vc.delay -= dt; continue; }
            if (vc.t >= vc.dur) continue;
            float q = vc.t / vc.dur;
            float f = vc.f0 + (vc.f1 - vc.f0) * q;
            vc.ph += f * dt;
            vc.ph -= std::floor(vc.ph);
            float w;
            switch (vc.wave) {
                case 0: w = std::sin(vc.ph * 6.2831853f); break;
                case 1: w = vc.ph < 0.5f ? 0.6f : -0.6f; break;
                case 2:
                    a->seed = a->seed * 1664525u + 1013904223u;
                    w = (float)((a->seed >> 9) & 0xFFFFu) / 32768.0f - 1.0f;
                    break;
                default: w = 4.0f * std::fabs(vc.ph - 0.5f) - 1.0f; break;
            }
            float env = std::min(1.0f, vc.t * 200.0f) * (1.0f - q);
            s += w * env * vc.vol;
            vc.t += dt;
        }
        s *= a->vol;
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        out[i] = (Sint16)(s * 30000.0f);
    }
    a->v.erase(std::remove_if(a->v.begin(), a->v.end(),
                              [](const Voice& vc) { return vc.delay <= 0.0f && vc.t >= vc.dur; }),
               a->v.end());
}

void addVoice(float f0, float f1, float dur, float delay, float vol, int wave) {
    Voice v;
    v.f0 = f0; v.f1 = f1; v.dur = dur; v.t = 0.0f; v.delay = delay; v.vol = vol; v.ph = 0.0f; v.wave = wave;
    A.v.push_back(v);
}

void sfx(int id) {
    if (!A.dev) return;
    SDL_LockAudioDevice(A.dev);
    if (A.v.size() < 48) {
        switch (id) {
            case SFX_CLICK: addVoice(660, 760, 0.05f, 0, 0.30f, 0); break;
            case SFX_BACK: addVoice(520, 360, 0.07f, 0, 0.28f, 0); break;
            case SFX_ERR: addVoice(190, 150, 0.16f, 0, 0.18f, 1); break;
            case SFX_NAV: addVoice(980, 980, 0.025f, 0, 0.12f, 0); break;
            case SFX_CASH:
                addVoice(1250, 1250, 0.06f, 0, 0.22f, 0);
                addVoice(1680, 1680, 0.10f, 0.06f, 0.22f, 0);
                break;
            case SFX_DONE:
                addVoice(523, 523, 0.12f, 0.00f, 0.25f, 3);
                addVoice(659, 659, 0.12f, 0.10f, 0.25f, 3);
                addVoice(784, 784, 0.22f, 0.20f, 0.25f, 3);
                break;
            case SFX_LAUNCH:
                addVoice(392, 392, 0.14f, 0.00f, 0.25f, 3);
                addVoice(523, 523, 0.14f, 0.12f, 0.25f, 3);
                addVoice(659, 659, 0.14f, 0.24f, 0.25f, 3);
                addVoice(1046, 1046, 0.45f, 0.36f, 0.25f, 3);
                addVoice(0, 0, 0.6f, 0.36f, 0.10f, 2);
                break;
            case SFX_BAD:
                addVoice(330, 300, 0.2f, 0.0f, 0.2f, 3);
                addVoice(262, 220, 0.4f, 0.18f, 0.2f, 3);
                break;
            default: break;
        }
    }
    SDL_UnlockAudioDevice(A.dev);
}

// ============================================================================
//  Немедленный UI: кнопки, фокус для геймпада, прокрутка, ползунки
// ============================================================================
struct WInfo { SDL_Rect r; int area; };
struct AInfo { int id; SDL_Rect v; };
struct UIState {
    std::vector<WInfo> pw, cw;
    std::vector<AInfo> pa, ca;
    std::vector<SDL_Rect> psl, csl;
    int focus = 0;
    bool pad = false;
    int activate = -1;
    bool focusMoved = false;
    bool click = false;
    int cx = 0, cy = 0;
    bool mdown = false;
    int mx = -1000, my = -1000, dx0 = 0, dy0 = 0, lastY = 0;
    bool dragging = false;
    int dragArea = -1;
    bool sliderPress = false;
    int area = -1;
    SDL_Rect view{0, 0, 0, 0};
    float scroll[16] = {};
    float maxScroll[16] = {};
    int lastIdx = -1;
    bool reset = false;
};
UIState U;

bool widget(const SDL_Rect& r, bool enabled) {
    int idx = (int)U.cw.size();
    WInfo wi; wi.r = r; wi.area = U.area;
    U.cw.push_back(wi);
    U.lastIdx = idx;
    bool hit = false;
    if (U.click && inRect(U.cx, U.cy, r) && (U.area < 0 || inRect(U.cx, U.cy, U.view))) {
        U.click = false;
        if (enabled) hit = true; else sfx(SFX_ERR);
    }
    if (U.activate == idx) {
        U.activate = -1;
        if (enabled) hit = true; else sfx(SFX_ERR);
    }
    return hit;
}
bool isFocused() { return U.pad && U.lastIdx == U.focus; }
void drawFocus(const SDL_Rect& r) {
    if (!isFocused()) return;
    SDL_Rect f = {r.x - 3, r.y - 3, r.w + 6, r.h + 6};
    drawRect(f, C_YEL, 3);
}
bool hovered(const SDL_Rect& r) {
    return !U.pad && inRect(U.mx, U.my, r) && (U.area < 0 || inRect(U.mx, U.my, U.view));
}
bool button(const SDL_Rect& r, const std::string& label, bool enabled = true, bool sel = false, int sc = 2) {
    bool hit = widget(r, enabled);
    bool hov = hovered(r);
    Col c = !enabled ? C_BTND : (sel ? C_BTNSEL : (hov ? C_BTNH : C_BTN));
    if (enabled && hov && U.mdown && !U.dragging) c = shade(C_BTNSEL, 0.8);
    if (enabled) fillRound({r.x, r.y + 3, r.w, r.h}, 8, shade(C_BG, 0.55));   // тень
    fillRound(r, 8, shade(c, 0.7));                                             // нижний край
    fillRound({r.x + 1, r.y + 1, r.w - 2, r.h - 3}, 7, c);                      // тело
    fillRect(r.x + 10, r.y + 2, r.w - 20, 1, shade(c, 1.5));                    // блик
    std::string t = fitText(label, sc, r.w - 12);
    drawTextC(t, r.x + r.w / 2, r.y + (r.h - 7 * sc) / 2, sc, enabled ? C_TEXT : C_DIM);
    drawFocus(r);
    if (hit) sfx(SFX_CLICK);
    return hit;
}

void beginArea(int id, SDL_Rect v) {
    U.area = id;
    U.view = v;
    AInfo ai; ai.id = id; ai.v = v;
    U.ca.push_back(ai);
    SDL_RenderSetClipRect(R, &v);
}
int scr(int id) { return (int)U.scroll[id]; }
void endArea(int contentH) {
    int id = U.area;
    SDL_RenderSetClipRect(R, nullptr);
    float mx = (float)std::max(0, contentH - U.view.h);
    U.maxScroll[id] = mx;
    if (U.scroll[id] > mx) U.scroll[id] = mx;
    if (U.scroll[id] < 0) U.scroll[id] = 0;
    if (mx > 0) {
        int th = std::max(30, U.view.h * U.view.h / std::max(1, contentH));
        int ty = U.view.y + (int)((U.view.h - th) * (U.scroll[id] / mx));
        fillRect(U.view.x + U.view.w - 5, U.view.y, 4, U.view.h, C_PANEL2);
        fillRect(U.view.x + U.view.w - 5, ty, 4, th, C_DIM);
    }
    U.area = -1;
}

// Ползунок с дискретными значениями: тап и перетаскивание
bool slider(const SDL_Rect& r, int count, int& val, const bool* allowed) {
    U.csl.push_back(r);
    if (U.mdown && U.sliderPress && inRect(U.dx0, U.dy0, r) && (U.area < 0 || inRect(U.dx0, U.dy0, U.view))) {
        int seg = clampi((U.mx - r.x) * count / std::max(1, r.w), 0, count - 1);
        if (allowed[seg] && seg != val) { val = seg; sfx(SFX_NAV); return true; }
    }
    return false;
}

void uiNav(int dir) {  // 0 вверх 1 вниз 2 влево 3 вправо
    if (!U.pad) { U.pad = true; return; }
    int n = (int)U.pw.size();
    if (n == 0) return;
    if (U.focus >= n) U.focus = 0;
    const SDL_Rect& a = U.pw[(size_t)U.focus].r;
    int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int best = -1;
    double bestScore = 1e18;
    for (int i = 0; i < n; ++i) {
        if (i == U.focus) continue;
        const SDL_Rect& b = U.pw[(size_t)i].r;
        int bx = b.x + b.w / 2, by = b.y + b.h / 2;
        int dx = bx - ax, dy = by - ay;
        double prim, sec;
        if (dir == 0) { if (dy >= -4) continue; prim = -dy; sec = std::abs(dx); }
        else if (dir == 1) { if (dy <= 4) continue; prim = dy; sec = std::abs(dx); }
        else if (dir == 2) { if (dx >= -4) continue; prim = -dx; sec = std::abs(dy); }
        else { if (dx <= 4) continue; prim = dx; sec = std::abs(dy); }
        double score = prim + sec * 2.5;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    if (best >= 0) { U.focus = best; U.focusMoved = true; sfx(SFX_NAV); }
}
void uiActivate() {
    if (!U.pad) { U.pad = true; return; }
    U.activate = U.focus;
}
int areaAt(int x, int y) {
    for (size_t i = 0; i < U.pa.size(); ++i) if (inRect(x, y, U.pa[i].v)) return U.pa[i].id;
    return -1;
}
void scrollBy(int id, float d) {
    if (id < 0 || id >= 16) return;
    U.scroll[id] = (float)clampd(U.scroll[id] + d, 0.0, U.maxScroll[id]);
}
void uiStick(float v, float dt) {
    if (std::fabs(v) < 0.25f) return;
    int area = -1;
    if (U.focus < (int)U.pw.size() && U.pw[(size_t)U.focus].area >= 0) area = U.pw[(size_t)U.focus].area;
    else if (!U.pa.empty()) area = U.pa[0].id;
    if (area < 0) return;
    U.pad = true;
    scrollBy(area, v * dt * 900.0f);
}
void uiEndFrame() {
    if (U.reset) {
        U.cw.clear(); U.ca.clear(); U.csl.clear();
        U.focus = 0;
        for (int i = 0; i < 16; ++i) { U.scroll[i] = 0; U.maxScroll[i] = 0; }
        U.reset = false;
    } else if (U.pad && U.focus < (int)U.cw.size()) {
        const WInfo& w = U.cw[(size_t)U.focus];
        if (w.area >= 0) {
            SDL_Rect v{0, 0, 0, 0};
            for (size_t i = 0; i < U.ca.size(); ++i) if (U.ca[i].id == w.area) v = U.ca[i].v;
            if (v.h > 0) {
                if (U.focusMoved) {
                    if (w.r.y < v.y) scrollBy(w.area, (float)(w.r.y - v.y - 8));
                    else if (w.r.y + w.r.h > v.y + v.h) scrollBy(w.area, (float)(w.r.y + w.r.h - v.y - v.h + 8));
                } else if (w.r.y + w.r.h <= v.y || w.r.y >= v.y + v.h) {
                    // прокрутили стиком: переводим фокус на видимый элемент
                    for (size_t i = 0; i < U.cw.size(); ++i)
                        if (U.cw[i].area == w.area && U.cw[i].r.y >= v.y && U.cw[i].r.y + U.cw[i].r.h <= v.y + v.h) {
                            U.focus = (int)i; break;
                        }
                }
            }
        }
    }
    if (U.focus >= (int)U.cw.size()) U.focus = U.cw.empty() ? 0 : (int)U.cw.size() - 1;
    U.focusMoved = false;
    U.pw.swap(U.cw); U.cw.clear();
    U.pa.swap(U.ca); U.ca.clear();
    U.psl.swap(U.csl); U.csl.clear();
    U.click = false;
    U.activate = -1;
}

void panel(int x, int y, int w, int h, const std::string& title) {
    fillRound({x + 4, y + 6, w, h}, 12, shade(C_BG, 0.5));          // тень
    fillRound({x, y, w, h}, 12, C_BORDER);                          // рамка
    fillRound({x + 1, y + 1, w - 2, h - 2}, 11, C_PANEL);           // тело
    fillRect(x + 12, y + 1, w - 24, 1, shade(C_PANEL, 1.4));        // блик
    if (!title.empty()) drawTextShadow(title, x + 16, y + 12, 2, C_ACC);
}
void progressBar(SDL_Rect r, double f, Col c) {
    fillRound(r, r.h / 2, C_PANEL2);
    int fw = (int)(r.w * clampd(f, 0, 1));
    if (fw > 0) fillRound({r.x, r.y, std::max(r.h, fw), r.h}, r.h / 2, c);
    fillRect(r.x + r.h / 2, r.y + 1, std::max(0, r.w - r.h), 1, shade(C_PANEL2, 1.5));
}

// ============================================================================
//  Игровые данные
// ============================================================================
enum { P_SIZE, P_COLOR, P_SCREEN, P_CPU, P_BATT, P_SPK, P_PACK, P_CAMN, P_CAMRES, P_CAMSZ, P_OS, P_CPUSRC, NPARAM };
const char* PARAM_NAMES[NPARAM] = {"Размер", "Цвет", "Экран", "Процессор", "Батарея", "Динамики", "Упаковка",
                                    "Камеры", "Разрешение", "Сенсор", "ОС", "Источник ЦП"};

struct Opt { const char* name; double cost; double q; };
const Opt SIZE_OPT[4] = {{"Компактный", 0.85, 0}, {"Стандарт", 1.0, 0}, {"Большой", 1.2, 0}, {"Огромный", 1.45, 0}};
const Opt COLOR_OPT[8] = {{"Чёрный", 0, 8}, {"Бежевый", 0, 8}, {"Хром", 2, 11}, {"Синий", 1, 10},
                          {"Красный", 1, 10}, {"Зелёный", 1, 10}, {"Кремовый", 4, 14}, {"Золотой", 6, 16}};
const Col COLOR_RGB[8] = {{38, 40, 46, 255},  {230, 226, 210, 255}, {176, 182, 192, 255}, {50, 100, 210, 255},
                          {200, 50, 60, 255}, {50, 160, 90, 255},   {236, 226, 200, 255}, {220, 180, 70, 255}};
const Opt SCREEN_OPT[6] = {{"Нет экрана", 0, 0}, {"Неоновая панель", 15, 10}, {"ЭЛТ монохром", 28, 20},
                           {"ЭЛТ цветной", 48, 32}, {"ЖК монохром", 75, 45}, {"ЖК цветной", 110, 59}};
const Opt CPU_OPT[6] = {{"Электронные лампы", 10, 8}, {"Транзисторы", 22, 18}, {"Интегральная схема", 40, 30},
                        {"Микропроцессор 8-бит", 68, 44}, {"Микропроцессор 32-бит", 105, 59}, {"Квантовый чип", 155, 75}};
const Opt BATT_OPT[6] = {{"Сухие элементы", 8, 8}, {"Никель-кадмиевая", 15, 18}, {"Никель-металлгидридная", 26, 29},
                         {"Литий-ионная", 40, 41}, {"Литий-полимерная", 60, 55}, {"Твердотельная", 88, 70}};
const Opt SPK_OPT[5] = {{"Громкоговоритель", 4, 6}, {"Стерео", 9, 15}, {"Hi-Fi стерео", 18, 26},
                        {"Объёмный звук", 30, 39}, {"Студийный", 46, 53}};
const Opt PACK_OPT[5] = {{"Бумажный пакет", 1, 2}, {"Картонная коробка", 3, 7}, {"Пластиковый блистер", 5, 12},
                         {"Эко-коробка", 9, 16}, {"Премиум-коробка", 16, 22}};

const Opt CAMN_OPT[5] = {{"Без камер", 0, 0}, {"1 камера", 12, 12}, {"2 камеры", 26, 22},
                          {"3 камеры", 40, 30}, {"4 камеры", 58, 38}};
const Opt CAMRES_OPT[6] = {{"Без камеры", 0, 0}, {"0.3 Мп VGA", 6, 8}, {"1.3 Мп", 12, 14},
                           {"5 Мп", 24, 24}, {"12 Мп", 40, 36}, {"48 Мп", 70, 50}};
const Opt CAMSZ_OPT[4] = {{"Миниатюрный", 2, 0}, {"Стандартный", 6, 4}, {"Крупный сенсор", 14, 10},
                          {"Полнокадровый", 30, 18}};

// 0 — своя ОС (разработка за свой счёт), 1..5 — лицензии на готовые системы по годам
// Варианты ОС и процессора: 0 — без лицензии/своего, 1 — своя (после проекта), далее лицензии по годам.
// Значения q у «своих» обновляются, когда проект завершён.
Opt OS_OPT[7] = {{"Без ОС", 0, 0}, {"Своя ОС", 0, 0}, {"Лицензия: UNIX-подобная", 4, 14},
                 {"Лицензия: DOS-подобная", 8, 20}, {"Лицензия: оконная ОС", 14, 28},
                 {"Лицензия: мобильная ОС", 18, 36}, {"Лицензия: смартфонная ОС", 25, 48}};
Opt CPUSRC_OPT[6] = {{"Без чипа", 0, 0}, {"Свой процессор", 0, 0}, {"Лицензия: 16-бит ЦП", 6, 10},
                     {"Лицензия: 32-бит ЦП", 12, 18}, {"Лицензия: мобильный SoC", 18, 30},
                     {"Лицензия: ИИ-ускоритель", 30, 44}};

int optCount(int p) {
    switch (p) {
        case P_SIZE: return 4;
        case P_COLOR: return 8;
        case P_SCREEN: case P_CPU: case P_BATT: return 6;
        case P_CAMN: return 5;
        case P_CAMRES: return 6;
        case P_CAMSZ: return 4;
        case P_OS: return 7;
        case P_CPUSRC: return 6;
        default: return 5;
    }
}
const Opt& opt(int p, int i) {
    switch (p) {
        case P_SIZE: return SIZE_OPT[i];
        case P_COLOR: return COLOR_OPT[i];
        case P_SCREEN: return SCREEN_OPT[i];
        case P_CPU: return CPU_OPT[i];
        case P_BATT: return BATT_OPT[i];
        case P_SPK: return SPK_OPT[i];
        case P_CAMN: return CAMN_OPT[i];
        case P_CAMRES: return CAMRES_OPT[i];
        case P_CAMSZ: return CAMSZ_OPT[i];
        case P_OS: return OS_OPT[i];
        case P_CPUSRC: return CPUSRC_OPT[i];
        default: return PACK_OPT[i];
    }
}

struct Cat { const char* name; const char* series; double costMult; double demand; int idealSize; int defPrice; int devDays; double w[NPARAM]; };
const int NCAT = 5;
const Cat CATS[NCAT] = {
    {"Радиоприёмник", "Радио", 0.8, 50, 1, 45, 16, {0.8, 0.6, 0.0, 0.8, 1.0, 2.0, 0.5, 0, 0, 0, 0, 0.8}},
    {"Телефон", "Тел", 1.2, 40, 1, 79, 20, {1.0, 0.7, 1.5, 1.5, 1.2, 0.8, 0.5, 0.8, 1.2, 0.8, 1.5, 1.5}},
    {"Наручные часы", "Часы", 1.0, 28, 0, 120, 16, {1.0, 1.0, 1.2, 0.8, 1.5, 0.3, 0.5, 0, 0, 0, 0.4, 0.8}},
    {"Калькулятор", "Калк", 1.5, 20, 1, 150, 20, {0.8, 0.5, 1.0, 1.2, 1.0, 0.0, 0.5, 0, 0, 0, 0, 1.2}},
    {"Компьютер", "ЭВМ", 4.0, 14, 2, 1500, 30, {0.9, 0.5, 1.4, 2.0, 1.4, 0.7, 0.5, 0.6, 0.6, 0.5, 2.0, 2.0}},
};
const char* SUFFIX[10] = {"Альфа", "Бета", "Гамма", "Дельта", "Эпсилон", "Омега", "Супер", "Мини", "Модель 1", "Модель 2"};

// wealth — достаток покупателей, tariff — пошлина с выручки, year — с какого года рынок доступен,
// pref — вкусы по категориям (радио, телефон, часы, калькулятор, компьютер)
struct Market { const char* name; double size; double cost; double rep; double wealth; double tariff; int year; double pref[NCAT]; };
const int NMARKET = 10;
const Market MARKETS[NMARKET] = {
    {"Россия", 1.0, 0, 0, 1.0, 0.00, 1960, {1.1, 1.0, 1.0, 1.0, 0.9}},
    {"Казахстан", 0.4, 15000, 3, 0.75, 0.05, 1992, {1.2, 1.0, 0.9, 0.9, 0.8}},
    {"Беларусь", 0.35, 15000, 5, 0.8, 0.05, 1992, {1.1, 1.0, 0.9, 1.0, 0.9}},
    {"Германия", 1.2, 150000, 18, 1.4, 0.12, 1960, {1.0, 0.9, 1.1, 1.1, 1.2}},
    {"Франция", 1.0, 180000, 25, 1.3, 0.12, 1960, {1.1, 1.0, 1.2, 0.9, 1.0}},
    {"Бразилия", 1.1, 300000, 35, 0.7, 0.25, 1960, {1.3, 1.1, 0.9, 0.8, 0.7}},
    {"Индия", 2.0, 600000, 45, 0.5, 0.30, 1960, {1.4, 1.2, 0.8, 0.9, 0.6}},
    {"Япония", 1.6, 900000, 55, 1.4, 0.10, 1960, {1.2, 1.0, 1.3, 1.4, 1.1}},
    {"США", 2.8, 2000000, 65, 1.6, 0.08, 1960, {0.9, 1.1, 1.0, 1.0, 1.4}},
    {"Китай", 3.2, 3000000, 75, 0.6, 0.20, 1979, {1.2, 1.2, 0.9, 1.0, 0.8}},
};

struct Camp { const char* name; double cost; double hype; };
const Camp CAMPS[4] = {{"Посты в соцсетях", 4000, 0.15}, {"Обзоры у блогеров", 15000, 0.4},
                       {"Реклама на ТВ", 50000, 0.9},     {"Глобальная кампания", 180000, 1.8}};

struct Pres { const char* name; double cost; double bonus; double hype; };
const Pres PRES[4] = {{"Без презентации", 0, 0, 0}, {"Пресс-релиз", 3000, 0.3, 0.05},
                      {"Презентация в зале", 20000, 0.7, 0.15}, {"Мега-шоу на стадионе", 90000, 1.2, 0.3}};

const int NOFFICE = 5;
const char* OFFICE_NAME[NOFFICE] = {"Гараж", "Небольшой офис", "Бизнес-центр", "Технопарк", "Штаб-квартира"};
const int OFFICE_CAP[NOFFICE] = {2, 5, 10, 20, 40};
const double OFFICE_MULT[NOFFICE] = {1.0, 1.1, 1.25, 1.45, 1.7};
const double OFFICE_COST[NOFFICE] = {0, 30000, 120000, 400000, 1200000};
const double OFFICE_RENT[NOFFICE] = {0, 2000, 7000, 20000, 50000};

const char* ROLE_NAME[3] = {"Дизайнер", "Программист", "Инженер"};
const char* FIRST_NAMES[16] = {"Алексей", "Мария", "Иван",   "Ольга", "Дмитрий", "Анна",  "Сергей", "Елена",
                               "Никита",  "Юлия",  "Артём",  "Дарья", "Павел",   "Ксения", "Игорь", "Вера"};
const char* INITIALS[12] = {"А.", "Б.", "В.", "Г.", "Д.", "К.", "Л.", "М.", "Н.", "П.", "С.", "Т."};

const int NCOMP = 20;
const char* COMP_NAMES[NCOMP] = {
    "Грушафон", "СуперЛаб", "Восток-Электро", "НеоТек", "ПиксельКо", "Альфа-Радио", "Звёздный Ток",
    "ТрансКонтур", "Северный Вектор", "Кварц-Плюс", "Орбита", "Ритм-Тех", "Светозар", "Глобал-Чип",
    "Меридиан", "Электрон-Пром", "Дельта-Лаб", "Ампер", "Зенит-Электро", "Сигма-Тек"};
const char* MONTHS[12] = {"Январь", "Февраль", "Март",     "Апрель",  "Май",    "Июнь",
                          "Июль",   "Август",  "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"};
const char* MONTHS_S[12] = {"Янв", "Фев", "Мар", "Апр", "Май", "Июн", "Июл", "Авг", "Сен", "Окт", "Ноя", "Дек"};
const char* OUTLETS[4] = {"ТехноОбзор", "Гаджет-Мир", "Цифра", "Девайс-Ревью"};
const char* COMPANY_NAMES[10] = {"Нова", "Кварц", "Ионика", "ТехноЛис", "Вектор",
                                 "Альфа-Девайс", "Спектр", "Орбита", "Микрон", "Гелиос"};

// Исследования
struct Tech { std::string name; int kind; int a; int b; double cost; int prereq; int year = 0; };
std::vector<Tech> TECHS;
int techFor[NPARAM][8];
int techCat[NCAT];

// Год появления каждого варианта компонента (реальная хронология)
const int PARAM_YEAR[NPARAM][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1960, 1962, 1968, 1976, 1985, 0, 0},   // экран
    {0, 1960, 1965, 1974, 1985, 2035, 0, 0},   // процессор
    {0, 1963, 1989, 1991, 2000, 2025, 0, 0},   // батарея
    {0, 1961, 1968, 1992, 2005, 0, 0, 0},      // динамики
    {0, 1960, 1965, 2005, 2015, 0, 0, 0},      // упаковка
    {0, 2000, 2016, 2019, 2021, 0, 0, 0},      // камеры: количество
    {0, 2001, 2003, 2006, 2014, 2019, 0, 0},   // камеры: разрешение
    {0, 2003, 2014, 2019, 0, 0, 0, 0},         // камеры: размер сенсора
    {0, 0, 1971, 1981, 1985, 2000, 2008, 0},   // ОС: своя и лицензии
    {0, 0, 1978, 1985, 2007, 2017, 0, 0},      // источник ЦП: свой и лицензии
};

void buildTechs() {
    TECHS.clear();
    for (int p = 0; p < NPARAM; ++p) for (int t = 0; t < 8; ++t) techFor[p][t] = -1;
    for (int c = 0; c < NCAT; ++c) techCat[c] = -1;
    auto add = [](const std::string& n, int kind, int a, int b, double cost, int pre) {
        Tech t; t.name = n; t.kind = kind; t.a = a; t.b = b; t.cost = cost; t.prereq = pre;
        TECHS.push_back(t);
        return (int)TECHS.size() - 1;
    };
    techCat[2] = add("Категория: Наручные часы", 1, 2, 0, 220, -1);
    TECHS[(size_t)techCat[2]].year = 1969;
    techCat[3] = add("Категория: Калькулятор", 1, 3, 0, 380, -1);
    TECHS[(size_t)techCat[3]].year = 1972;
    techCat[4] = add("Категория: Компьютер", 1, 4, 0, 650, -1);
    TECHS[(size_t)techCat[4]].year = 1981;
    const int params[8] = {P_SCREEN, P_CPU, P_BATT, P_SPK, P_PACK, P_CAMN, P_CAMRES, P_CAMSZ};
    for (int k = 0; k < 8; ++k) {
        int p = params[k];
        int prev = -1;
        double mul = (p == P_SPK || p == P_PACK) ? 0.6 : 1.0;
        for (int t = 1; t < optCount(p); ++t) {
            prev = add(std::string(PARAM_NAMES[p]) + ": " + opt(p, t).name, 0, p, t, (75.0 * t * t + 60.0 * t) * mul, prev);
            techFor[p][t] = prev;
            TECHS[(size_t)prev].year = PARAM_YEAR[p][t];
        }
    }
    int a1 = add("Автоматизация завода I", 2, 0, 0, 450, -1);
    add("Автоматизация завода II", 2, 0, 0, 1100, a1);
    int d1 = add("Agile-разработка I", 3, 0, 0, 400, -1);
    add("Agile-разработка II", 3, 0, 0, 1000, d1);
    // Технологии, которые появляются в реальные годы
    struct YT { const char* n; int kind; double cost; int year; };
    const YT yearly[] = {
        {"Автоматизированное проектирование (САПР)", 3, 320, 1964},
        {"Роботизированный завод", 2, 600, 1971},
        {"Нейросети: обратное распространение", 4, 900, 1986},
        {"Облачные вычисления", 3, 1100, 2006},
        {"Нейросети: голосовой ассистент", 4, 1600, 2011},
        {"Нейросети: распознавание изображений", 4, 1800, 2012},
        {"Самообучающийся контроль качества", 2, 2000, 2016},
        {"Нейросети: генерация текста", 4, 2400, 2020},
        {"Нейросети: генеративные модели для дизайна", 4, 2800, 2023},
        {"Квантовая разработка", 3, 3500, 2030},
        {"Нейроморфные чипы", 2, 3800, 2035},
    };
    for (const YT& y : yearly) {
        int id = add(y.n, y.kind, 0, 0, y.cost, -1);
        TECHS[(size_t)id].year = y.year;
    }
}

// xp — опыт (растёт со временем и даёт навык), ask — оклад, который сотрудник просит (0 — не просит)
struct Staff { std::string name; int role = 0; int skill = 1; int salary = 1000; int mood = 70; int xp = 0; int ask = 0; };
struct Draft { int cat = 0; int p[NPARAM] = {1, 0, 0, 0, 0, 0, 0}; int price = 199; int suffix = 0; };
struct Product {
    std::string name;
    int cat = 0;
    int p[NPARAM] = {0, 0, 0, 0, 0, 0, 0};
    int price = 0;
    double unitCost = 0, quality = 0, rating = 0;
    int pres = 0;
    int launchDay = 0;
    bool onSale = true;
    int adDays = 0;          // сколько дней идёт кампания
    double adBoost = 1.0;    // множитель спроса во время кампании
    double units = 0, revenue = 0, profit = 0, weekAcc = 0, carry = 0, lastDay = 0;
    std::vector<int> weekly;
    // Склад и производство
    double stock = 0, queue = 0;   // готово на складе / заказано и ждёт выпуска
    bool autoProd = true;          // автозаказ партий
    double avgD = 0, lost = 0;     // средний спрос в день / неудовлетворённый спрос за вчера
    // Качество
    double defect = 0.04;          // доля брака
    double returns = 0;            // возвращено по гарантии, шт.
    int qa = 1;                    // уровень контроля качества при запуске
    bool fbDone = false;           // отзывы покупателей уже учтены
    // Экономика
    double inflAt = 1.0;           // индекс цен на момент запуска
    // Обновления
    int fw = 0, fwDay = 0;
    double lifeBonus = 0;
    bool warned = false;
    // Не сохраняется: считается каждый день
    double seg[3] = {0, 0, 0};
    double compF = 1.0, tariffNow = 0, lifeNow = 1.0;
};

// Слот разработки: одновременно может идти несколько проектов
struct DevSlot {
    Draft d;
    bool active = false, ready = false, crunch = false;
    double prog = 0, total = 1;
    std::string name;
};
const int NSLOT = 3;

// Модель конкурента на рынке
struct RProd { int comp = 0, cat = 0, num = 1; double rating = 5; int price = 100; int launchDay = 0; };

// Награда: владелец -1 — игрок, 0..NCOMP-1 — конкурент
struct Award { std::string title, winner, detail; int year = 0; int owner = -1; };

struct Suit { bool active = false; int rival = 0; double claim = 0; int reason = 0; };

struct Game {
    std::string company = "Девайс-Корп";
    double money = 150000, rep = 5, fans = 0;
    int day = 0, speed = 1, office = 0;
    std::vector<Staff> staff, cand;
    std::vector<char> techDone;
    int researching = -1;
    double researchProg = 0;
    bool market[NMARKET] = {true, false, false, false, false, false, false, false, false, false};
    double hype = 0;
    int trend = 3, negMonths = 0;
    double monInc = 0, monExp = 0;
    std::vector<double> histInc, histExp;
    double todayUnits = 0, todayRev = 0;
    std::vector<Product> products;
    Draft draft;
    DevSlot slot[NSLOT];
    int gen[NCAT] = {0, 0, 0, 0, 0};
    double comp[NCOMP] = {900, 650, 420, 260, 140};
    std::vector<std::string> log;
    double rivalProfit[NCOMP] = {};   // прибыль конкурентов за текущий год
    std::vector<Award> awardLog;      // история наград
    int factory = 0;                  // уровень производства
    double capFactor = 1.0, lastDemand = 0;
    int patents = 0;                  // маска оформленных патентов
    double evDemand = 1.0, evCost = 1.0;
    int evDays = 0, evActive = -1, pendEv = -1;
    Suit suit;
    // Экономика
    double infl = 1.0;                // индекс цен к 1960 году
    double lastInfl = 0;              // инфляция за прошлый год
    double loan = 0;                  // долг банку
    double stake = 1.0;               // доля основателя
    bool ipo = false;
    double lastTax = 0, lastDiv = 0;
    // Снабжение
    int supplier = 0, supUntil = 0, supHalt = 0;
    double prodToday = 0;
    // Люди
    bool crunch = false;
    bool ussrSplit = false;
    std::vector<RProd> rivals;
};
Game G;

// ---------------------------------------------------------------------------
//  Экранное состояние
// ---------------------------------------------------------------------------
enum Screen { SC_NEW, SC_MAIN, SC_DESIGN, SC_LAUNCH, SC_OFFICE, SC_RESEARCH, SC_MARKET, SC_STATS, SC_PAUSE, SC_OVER, SC_PLATFORM, SC_AWARDS, SC_HALL, SC_ADS, SC_FACTORY, SC_PATENT, SC_EVENT, SC_SUIT, SC_BANK, SC_SUPPLY };
Screen screen = SC_NEW, retScreen = SC_MAIN;
std::string toastMsg;
float toastT = 0;
int selProduct = -1;
int resIdx = -1;
float resT = 0;
double resOutlet[4] = {0, 0, 0, 0};
double resRepDelta = 0;
int presChoice = 1;
int qaChoice = 1;      // контроль качества при запуске
int batchChoice = -1;  // размер стартовой партии (-1 — подобрать по ожидаемому спросу)
int orderIdx = 4;      // размер партии для ручного заказа
int statsTab = 0;
std::string newName;
int capChoice = 1;
bool textActive = false;
bool running = true;
std::string savePath;
bool saveExists = false;
float dayTimer = 0;
float uiTime = 0;

void setScreen(Screen s) {
    if (s == SC_PAUSE && screen != SC_PAUSE) retScreen = screen;
    screen = s;
    U.reset = true;
}
const int START_YEAR = 1960;  // игра начинается в 1960 году
void toast(const std::string& s) { toastMsg = s; toastT = 3.0f; }
void logMsg(const std::string& s) {
    std::string d = std::string(MONTHS_S[(G.day / 30) % 12]) + " " + std::to_string(START_YEAR + G.day / 360) + ": ";
    G.log.push_back(d + s);
    if (G.log.size() > 40) G.log.erase(G.log.begin());
}

// ---------------------------------------------------------------------------
//  Формулы
// ---------------------------------------------------------------------------
bool techDone(int i) { return i >= 0 && i < (int)G.techDone.size() && G.techDone[(size_t)i]; }
// Разработка своей платформы по этапам
struct PO { const char* n; double cost; int days; int q; };
struct Proj { int st = 0; int pick = -1; double prog = 0; int q = 0; };  // st: 0..2 этапы, 3 — готово
Proj PRJ_CPU, PRJ_OS;
const PO CPU_STAGES[3][3] = {
    {{"RISC-подобная", 20000, 30, 4}, {"CISC-подобная", 15000, 40, 3}, {"Сверхскалярная", 45000, 60, 7}},
    {{"Техпроцесс 5 мкм", 10000, 20, 2}, {"Техпроцесс 3 мкм", 30000, 40, 5}, {"Техпроцесс 1 мкм", 70000, 70, 9}},
    {{"Быстрая проверка", 5000, 10, 1}, {"Полный цикл", 25000, 40, 5}, {"Ручная доводка", 60000, 80, 10}}};
const char* const CPU_NAMES[3] = {"Архитектура", "Техпроцесс", "Тестирование"};
const PO OS_STAGES[3][3] = {
    {{"Монолитное ядро", 15000, 30, 4}, {"Микроядро", 30000, 50, 7}, {"Однопользовательская", 5000, 15, 2}},
    {{"Командная строка", 5000, 10, 2}, {"Оконный интерфейс", 40000, 60, 8}, {"Сенсорный интерфейс", 70000, 80, 12}},
    {{"Минимум драйверов", 5000, 10, 1}, {"Совместимость с периферией", 25000, 40, 5}, {"Полная библиотека", 50000, 70, 9}}};
const char* const OS_NAMES[3] = {"Ядро", "Интерфейс", "Драйверы и тесты"};

int calYear();
bool tierUnlocked(int p, int t) {
    if (p == P_SIZE || p == P_COLOR) return true;
    if (p == P_OS) {
        if (t == 0) return true;
        if (t == 1) return PRJ_OS.st == 3;
        return calYear() >= PARAM_YEAR[p][t];
    }
    if (p == P_CPUSRC) {
        if (t == 0) return true;
        if (t == 1) return PRJ_CPU.st == 3;
        return calYear() >= PARAM_YEAR[p][t];
    }
    if (t == 0) return true;
    return techDone(techFor[p][t]);
}
bool catUnlocked(int c) { return c < 2 || techDone(techCat[c]); }
int countKind(int kind) {
    int n = 0;
    for (size_t i = 0; i < TECHS.size(); ++i) if (TECHS[i].kind == kind && techDone((int)i)) ++n;
    return n;
}
double skillSum(int role) {
    double s = 0;
    for (size_t i = 0; i < G.staff.size(); ++i) if (G.staff[i].role == role) s += G.staff[i].skill * (0.7 + 0.3 * G.staff[i].mood / 100.0);  // настроение влияет на отдачу
    return s;
}
double researchRate() {
    return (5.0 + 0.3 * skillSum(0) + 0.6 * skillSum(1) + 0.45 * skillSum(2)) * OFFICE_MULT[G.office];
}
double devSpeed() {
    return (1.0 + 0.03 * skillSum(2) + 0.02 * skillSum(1) + 0.01 * skillSum(0)) * OFFICE_MULT[G.office] *
           (1.0 + 0.15 * countKind(3)) * (1.0 + 0.08 * countKind(4)) * (G.crunch ? 1.35 : 1.0);
}
// До 1992 года первый рынок — СССР (включает Казахстан и Беларусь)
bool ussr() { return calYear() < 1992; }
std::string marketName(int i) { return (i == 0 && ussr()) ? std::string("СССР") : std::string(MARKETS[i].name); }
bool marketAvail(int i) { return calYear() >= MARKETS[i].year; }
double marketBaseSize(int i) { return (i == 0 && ussr()) ? 1.75 : MARKETS[i].size; }
bool marketOn(int i) { return G.market[i] && marketAvail(i); }
double marketSize() {
    double s = 0;
    for (int i = 0; i < NMARKET; ++i) if (marketOn(i)) s += marketBaseSize(i);
    return s;
}
int marketsOpen() { int n = 0; for (int i = 0; i < NMARKET; ++i) if (marketOn(i)) ++n; return n; }
double years() { return G.day / 360.0; }
// Уровень техники года: качество устройства из лучших доступных в этом году компонентов (среднее по категориям)
double eraQuality(int year) {
    double sum = 0;
    int n = 0;
    for (int c = 0; c < NCAT; ++c) {
        if (c >= 2 && year < TECHS[(size_t)techCat[c]].year) continue;
        const Cat& cat = CATS[c];
        double sw = 0, sq = 0;
        for (int p = 0; p < NPARAM; ++p) {
            double w = cat.w[p];
            if (w <= 0) continue;
            double q = 0;
            if (p == P_SIZE) {
                q = 20;
            } else {
                for (int t = 0; t < optCount(p); ++t) {
                    if ((p == P_OS || p == P_CPUSRC) && t == 1) continue;  // своя платформа зависит от игрока
                    if (p != P_COLOR && t > 0 && PARAM_YEAR[p][t] > year) continue;
                    q = std::max(q, opt(p, t).q);
                }
            }
            sw += w;
            sq += w * q;
        }
        if (sw > 0) { sum += sq / sw; ++n; }
    }
    return n ? sum / n : 10.0;
}
// Ожидания рынка растут вместе с доступной техникой: в 1960 году планка та же, что и раньше (18),
// дальше она поднимается по мере появления новых компонентов, а не линейно по годам
double expectation() {
    static int cy = -1;
    static double cv = 18.0;
    int y = calYear();
    if (y != cy) {
        cy = y;
        cv = 18.0 * (eraQuality(y) + 15.0) / (eraQuality(START_YEAR) + 15.0);
    }
    return cv;
}
double salaries() { double s = 0; for (size_t i = 0; i < G.staff.size(); ++i) s += G.staff[i].salary; return s; }

double draftUnitCost(const Draft& d) {
    const Cat& c = CATS[d.cat];
    double sum = COLOR_OPT[d.p[P_COLOR]].cost;
    for (int p = P_SCREEN; p < NPARAM; ++p) if (c.w[p] > 0) sum += opt(p, d.p[p]).cost;
    return sum * SIZE_OPT[d.p[P_SIZE]].cost * c.costMult * (1.0 - 0.08 * countKind(2)) * G.infl;
}
double groupBonus(int role) { return 15.0 * (1.0 - std::exp(-skillSum(role) / 25.0)); }
double draftQuality(const Draft& d) {
    const Cat& c = CATS[d.cat];
    double bD = groupBonus(0), bP = groupBonus(1), bE = groupBonus(2);
    double sw = 0, sq = 0;
    for (int p = 0; p < NPARAM; ++p) {
        double w = c.w[p];
        if (w <= 0) continue;
        double q, b;
        if (p == P_SIZE) { q = std::max(0.0, 20.0 - 9.0 * std::abs(d.p[p] - c.idealSize)); b = bD; }
        else if (p == P_COLOR) { q = COLOR_OPT[d.p[p]].q + (d.p[p] == G.trend ? 10.0 : 0.0); b = bD; }
        else {
            q = opt(p, d.p[p]).q;
            b = (p == P_PACK) ? bD : ((p == P_CPU || p == P_SCREEN) ? bP : bE);
        }
        sw += w; sq += w * (q + b);
    }
    return sw > 0 ? sq / sw : 0;
}
double scoreFromQ(double q) { return clampd(10.0 * (1.0 - std::exp(-0.9 * q / expectation())), 1.0, 10.0); }
double fairPrice(double unitCost, double rating) { return unitCost * (1.25 + 0.08 * rating); }
double draftDevTotal(const Draft& d) {
    int sum = 0;
    for (int p = P_SCREEN; p < NPARAM; ++p) sum += d.p[p];
    return CATS[d.cat].devDays * (1.0 + 0.12 * sum);
}
double toolingCost(const Draft& d) { return draftUnitCost(d) * 150.0 * std::sqrt(marketSize()) + std::round(5000.0 * G.infl); }
std::string draftName(const Draft& d) {
    return std::string(CATS[d.cat].series) + " " + SUFFIX[d.suffix] + " " + std::to_string(G.gen[d.cat] + 1);
}
Col ratingCol(double r) { return r >= 7.5 ? C_GREEN : (r >= 5.0 ? C_YEL : (r >= 3.5 ? C_ORANGE : C_RED)); }
int priceStep(int p) { return p < 100 ? 5 : (p < 500 ? 10 : (p < 2000 ? 25 : 50)); }

void fixDraft(Draft& d) {
    for (int p = 0; p < NPARAM; ++p) {
        d.p[p] = clampi(d.p[p], 0, optCount(p) - 1);
        while (d.p[p] > 0 && !tierUnlocked(p, d.p[p])) --d.p[p];
    }
    if (CATS[d.cat].w[P_SCREEN] <= 0) d.p[P_SCREEN] = 0;
}

Staff genCandidate() {
    Staff s;
    s.role = irand(0, 2);
    s.skill = clampi(irand(1, 4 + G.office * 2), 1, 10);
    s.salary = ((int)((1200 + s.skill * 550 + irand(0, 300)) * G.infl) / 50) * 50;
    s.name = std::string(FIRST_NAMES[irand(0, 15)]) + " " + INITIALS[irand(0, 11)];
    return s;
}
void refreshCandidates() {
    G.cand.clear();
    for (int i = 0; i < 3; ++i) G.cand.push_back(genCandidate());
}

int calYear() { return START_YEAR + G.day / 360; }

// ===== Производство =====
const int NFACT = 6;
const char* const FACT_NAME[NFACT] = {"Подрядчик (без своего завода)", "Мастерская", "Малый завод", "Завод",
                                      "Автоматизированный завод", "Роботизированный комплекс"};
const double FACT_CAP[NFACT] = {400, 700, 1500, 3500, 8000, 20000};     // шт/день
const double FACT_MULT[NFACT] = {1.15, 1.05, 1.0, 0.93, 0.86, 0.80};    // множитель себестоимости
const double FACT_COST[NFACT] = {0, 40000, 150000, 500000, 1500000, 4000000};
const double FACT_UPKEEP[NFACT] = {0, 3000, 10000, 30000, 90000, 250000};
const int FACT_YEAR[NFACT] = {1960, 1960, 1962, 1975, 1985, 2005};
double factoryCap() { return FACT_CAP[G.factory]; }
double factoryMult() { return FACT_MULT[G.factory]; }
std::string pct(double m) { return std::to_string((int)std::round(m * 100)) + "%"; }

// ===== Инфляция =====
// Все «табличные» цены заданы в ценах 1960 года и пересчитываются через индекс G.infl
double ic(double v) { return std::round(v * G.infl); }
double officeRent() { return ic(OFFICE_RENT[G.office]); }
double factUpkeep() { return ic(FACT_UPKEEP[G.factory]); }
int defPrice(int c) { return std::max(10, (int)(CATS[c].defPrice * G.infl / 5.0) * 5); }
int marketSalary(int skill) { return ((int)((1200 + skill * 550) * G.infl) / 50) * 50; }

// ===== Сезонность: спрос по месяцам (пик перед Новым годом) =====
const double SEASON[12] = {0.82, 0.85, 0.95, 1.0, 1.0, 0.95, 0.9, 0.95, 1.05, 1.1, 1.25, 1.5};
double seasonNow() { return SEASON[(G.day / 30) % 12]; }

// ===== Поставщики комплектующих =====
// cm — множитель себестоимости, defAdd — добавка к браку, fee — плата в месяц (цены 1960), fail — шанс срыва в месяц
struct Sup { const char* n; const char* d; double cm, defAdd, fee, fail; };
const int NSUP = 4;
const Sup SUPS[NSUP] = {
    {"Разовые закупки", "Без обязательств, рыночные цены.", 1.00, 0.0, 0, 0.02},
    {"Дешёвый поставщик", "-10% к себестоимости, но больше брака и срывов.", 0.90, 0.02, 0, 0.07},
    {"Надёжный партнёр", "Меньше брака, кризисы бьют по ценам вдвое слабее.", 1.02, -0.01, 3000, 0.0},
    {"Эксклюзивный контракт", "-7% к себестоимости и защита от кризисов.", 0.93, -0.005, 12000, 0.0},
};
double supFee(int s) { return ic(SUPS[s].fee * std::sqrt(marketSize())); }
double evCostEff() {
    if (G.evDays <= 0) return 1.0;
    double c = G.evCost;
    if (c > 1.0 && G.supplier >= 2) c = 1.0 + (c - 1.0) * 0.5;
    return c;
}
// Себестоимость выпуска одной штуки сегодня
double unitProdCost(const Product& p) {
    return p.unitCost * (G.infl / std::max(0.01, p.inflAt)) * factoryMult() * evCostEff() * SUPS[G.supplier].cm;
}
double volDisc(double n) { return n >= 20000 ? 0.94 : (n >= 5000 ? 0.97 : 1.0); }  // скидка за объём партии
double fairNow(const Product& p) { return fairPrice(p.unitCost * (G.infl / std::max(0.01, p.inflAt)), p.rating); }

// ===== Контроль качества =====
struct QA { const char* n; double cost; double mult; };
const QA QAS[3] = {{"Без проверки", 0, 1.6}, {"Выборочный", 1500, 1.0}, {"Полный тест", 6000, 0.45}};
double qaCost(int q) { return ic(QAS[q].cost * std::sqrt(marketSize())); }
double baseDefect(bool crunch, int qa) {
    double d = clampd(0.07 - 0.01 * std::sqrt(skillSum(2)), 0.02, 0.07);  // инженеры снижают брак
    d *= QAS[qa].mult * (1.0 - 0.06 * G.factory);
    if (crunch) d *= 1.35;
    d += SUPS[G.supplier].defAdd;
    return clampd(d, 0.005, 0.25);
}

// ===== Сегменты покупателей =====
const char* const SEG_NAME[3] = {"Бюджет", "Средний", "Премиум"};
const double SEG_SHARE[3] = {0.50, 0.35, 0.15};  // доля покупателей
const double SEG_WTP[3] = {0.75, 1.0, 1.5};      // готовность платить относительно «справедливой» цены
const double SEG_EL[3] = {3.2, 2.5, 1.6};        // чувствительность к цене
const double SEG_RT[3] = {1.0, 1.6, 2.6};        // чувствительность к оценке
double segDemand(double rating, double fair, int price, double* out) {
    double tot = 0;
    for (int s = 0; s < 3; ++s) {
        double pf = clampd(std::pow(fair * SEG_WTP[s] / std::max(1, price), SEG_EL[s]), 0.02, 3.0);
        double rf = std::pow(std::max(rating, 0.5) / 5.0, SEG_RT[s]);
        double v = SEG_SHARE[s] * pf * rf;
        if (s == 2) v *= 0.5 + G.rep / 100.0;  // премиум покупает бренд
        out[s] = v;
        tot += v;
    }
    return tot / 0.72;
}

// ===== Страны: достаток, пошлины и вкусы =====
// priceLevel — цена относительно справедливой: дорогие товары хуже идут в бедных странах
double effMarket(int cat, double priceLevel, double* tariffOut) {
    double s = 0, t = 0;
    double ex = clampd(priceLevel, 0.5, 2.0) * 1.2;
    for (int i = 0; i < NMARKET; ++i) {
        if (!marketOn(i)) continue;
        double e = marketBaseSize(i) * MARKETS[i].pref[cat] * std::pow(MARKETS[i].wealth, ex);
        s += e;
        t += e * MARKETS[i].tariff;
    }
    if (tariffOut) *tariffOut = s > 0 ? t / s : 0;
    return s;
}

// ===== Конкуренты: реальные модели на рынке =====
std::string rprodName(const RProd& r) {
    return std::string(COMP_NAMES[r.comp]) + " " + CATS[r.cat].series + "-" + (r.num < 10 ? "0" : "") + std::to_string(r.num);
}
double rivalPressure(int cat, int price) {
    double a = 0;
    for (size_t i = 0; i < G.rivals.size(); ++i) {
        const RProd& r = G.rivals[i];
        if (r.cat != cat) continue;
        double fresh = std::exp(-(G.day - r.launchDay) / 150.0);
        a += std::pow(r.rating / 5.0, 1.6) * fresh * clampd((double)price / std::max(1, r.price), 0.6, 1.6);
    }
    return a;
}
double compFactor(const Product& p) {
    double ap = std::pow(std::max(p.rating, 0.5) / 5.0, 1.6);
    return clampd(1.6 * ap / (ap + 0.25 * rivalPressure(p.cat, p.price)), 0.3, 1.4);
}

// Потенциальный спрос на товар в день (до ограничения складом)
double productDemand(Product& p, int sameCat) {
    int age = G.day - p.launchDay;
    p.lifeNow = std::exp(-age / (60.0 + p.rating * 15.0 + p.lifeBonus)) * (age < 10 ? 1.6 : 1.0);
    double fair = fairNow(p);
    double seg[3];
    double sf = segDemand(p.rating, fair, p.price, seg);
    double tot = seg[0] + seg[1] + seg[2];
    for (int s = 0; s < 3; ++s) p.seg[s] = tot > 0 ? seg[s] / tot : 0;
    double ms = effMarket(p.cat, p.price / std::max(1.0, fair), &p.tariffNow);
    p.compF = compFactor(p);
    double repF = 0.6 + G.rep / 100.0 * 0.8;
    double d = CATS[p.cat].demand * ms * sf * repF * (1.0 + 0.5 * G.hype) * p.lifeNow * p.compF * seasonNow() /
               (double)std::max(1, sameCat);
    if (p.adDays > 0) d *= p.adBoost;
    if (G.evDays > 0) d *= G.evDemand;
    return d;
}

// Заказ партии: оплата вперёд, выпуск идёт по мощности завода
bool orderBatch(Product& p, double n, bool autoMode) {
    n = std::floor(n);
    if (n < 1) return false;
    double unit = unitProdCost(p);
    double cost = n * unit * volDisc(n);
    double budget = autoMode ? G.money * 0.6 : G.money;
    if (cost > budget) {
        if (!autoMode) return false;
        n = std::floor(std::max(0.0, budget) / std::max(0.01, unit));
        if (n < 1) return false;
        cost = n * unit * volDisc(n);
    }
    G.money -= cost;
    G.monExp += cost;
    p.profit -= cost;
    p.queue += n;
    return true;
}

// Снятие с продаж: остатки уходят оптом за 40% себестоимости
void discontinue(Product& p, const char* why) {
    p.onSale = false;
    double left = p.stock + p.queue;
    if (left >= 1) {
        double v = left * unitProdCost(p) * 0.4;
        G.money += v;
        G.monInc += v;
        p.profit += v;
        logMsg("«" + p.name + "» " + why + ". Остатки (" + fmtNum(left) + " шт.) распроданы за " + fmtMoney(v));
    } else {
        logMsg("«" + p.name + "» " + why + ".");
    }
    p.stock = 0;
    p.queue = 0;
}

// ===== Финансы: кредит, налог, инвесторы =====
const double TAX_RATE = 0.20;
double annualSum(bool profit) {
    double s = 0;
    int n = (int)G.histInc.size();
    for (int i = std::max(0, n - 12); i < n; ++i) s += G.histInc[(size_t)i] - (profit ? G.histExp[(size_t)i] : 0.0);
    return s;
}
double loanRate() {
    double r = 0.10;
    if (G.rep < 20) r += 0.04;
    if (G.evDays > 0 && G.evDemand < 1.0) r += 0.03;  // в кризис деньги дороже
    return r;
}
double loanLimit() { return std::max(ic(50000), 0.5 * annualSum(false)); }
double valuation() {
    return std::max(ic(100000), 4.0 * std::max(0.0, annualSum(true)) + std::max(0.0, G.money) + G.rep * ic(3000) - G.loan);
}

// ===== Разработка: слоты =====
int maxSlots() { return G.office >= 4 ? 3 : (G.office >= 2 ? 2 : 1); }
int freeSlot() {
    for (int i = 0; i < maxSlots(); ++i) if (!G.slot[i].active) return i;
    return -1;
}
int readySlot() {
    for (int i = 0; i < NSLOT; ++i) if (G.slot[i].active && G.slot[i].ready) return i;
    return -1;
}
int devCount() {
    int n = 0;
    for (int i = 0; i < NSLOT; ++i) if (G.slot[i].active && !G.slot[i].ready) ++n;
    return n;
}
bool anyDev() {
    for (int i = 0; i < NSLOT; ++i) if (G.slot[i].active) return true;
    return false;
}
// Скорость одного проекта: при параллельной работе команда делится, но с небольшой потерей
double slotSpeed() { return devSpeed() / std::pow((double)std::max(1, devCount()), 0.8); }

// ===== Обновления после выпуска =====
const int FW_MAX = 4;
double updateCost(const Product& p) { return ic((3000 + 2500 * p.fw) * std::sqrt(marketSize())); }
bool canUpdate(const Product& p) {
    return p.onSale && p.fw < FW_MAX && G.day - std::max(p.launchDay, p.fwDay) >= 45;
}
void releaseUpdate(Product& p) {
    double c = updateCost(p);
    G.money -= c;
    G.monExp += c;
    p.profit -= c;
    p.fw++;
    p.fwDay = G.day;
    p.rating = clampd(std::round((p.rating + 0.2) * 10.0) / 10.0, 1.0, 10.0);
    p.lifeBonus += 25;
    p.defect *= 0.85;
    p.warned = false;
    G.hype = std::min(4.0, G.hype + 0.05);
    logMsg("Обновление №" + std::to_string(p.fw) + " для «" + p.name + "»: оценка " + fmt1(p.rating));
    toast("Обновление выпущено");
    sfx(SFX_DONE);
}

// ===== Исторические события (эффекты — игровой баланс, не реальные данные) =====
struct Ev { int year, month; const char* title; const char* text; double dm, cm; int days; };
const int NEV = 16;
const Ev EVS[NEV + 3] = {
    {1962, 9, "Карибский кризис", "Мировая тревога снижает потребительские расходы.", 0.90, 1.00, 90},
    {1969, 6, "Высадка на Луне", "Всплеск интереса к электронике и технологиям.", 1.30, 1.00, 120},
    {1973, 9, "Нефтяной кризис", "Дорожают сырьё и логистика, покупатели экономят.", 0.80, 1.15, 360},
    {1979, 2, "Энергетический кризис", "Новый скачок цен на энергию.", 0.85, 1.10, 240},
    {1984, 0, "Бум персональных компьютеров", "Домашняя электроника входит в моду.", 1.25, 1.00, 300},
    {1987, 9, "Чёрный понедельник", "Обвал бирж: спрос падает.", 0.80, 1.00, 120},
    {1991, 11, "Распад СССР: новые рынки", "Падают торговые барьеры, открываются новые покупатели.", 1.25, 1.00, 360},
    {1997, 6, "Азиатский финансовый кризис", "Валюты падают: комплектующие дешевеют, спрос слабеет.", 0.90, 0.92, 240},
    {2000, 2, "Крах доткомов", "Пузырь лопнул, инвесторы бегут, спрос падает.", 0.80, 1.00, 360},
    {2007, 5, "Эра сенсорных смартфонов", "Новая мода на устройства с сенсорным экраном.", 1.35, 1.00, 360},
    {2008, 8, "Мировой финансовый кризис", "Банки в кризисе, покупатели экономят.", 0.75, 1.05, 360},
    {2011, 9, "Наводнение в Таиланде", "Сбои у производителей комплектующих.", 1.00, 1.20, 180},
    {2020, 2, "Пандемия", "Удалёнка поднимает спрос, но логистика ломается.", 1.20, 1.25, 240},
    {2021, 5, "Дефицит микросхем", "Чипов не хватает, цены растут.", 0.95, 1.30, 360},
    {2022, 2, "Инфляция и энергокризис", "Дорожает всё, покупатели экономят.", 0.85, 1.15, 300},
    {2023, 5, "Бум нейросетей", "Спрос на умные устройства взлетел.", 1.30, 1.00, 360},
    // Случайные события для поздних лет (year = 0)
    {0, 0, "Технологический бум", "Новый прорыв в индустрии.", 1.20, 1.00, 180},
    {0, 0, "Торговая война", "Пошлины на комплектующие.", 0.85, 1.15, 240},
    {0, 0, "Сбой поставок", "Перебои у поставщиков.", 1.00, 1.20, 120},
};
Screen evRet = SC_MAIN;
Screen evBack() { return (evRet == SC_EVENT || evRet == SC_SUIT) ? SC_MAIN : evRet; }
void triggerEvent(int idx) {
    const Ev& e = EVS[idx];
    G.pendEv = idx;
    G.evActive = idx;
    G.evDemand = e.dm;
    G.evCost = e.cm;
    G.evDays = e.days;
    // Удар по цепочкам поставок останавливает завод, если нет надёжного поставщика
    if (e.cm >= 1.2 && G.supplier < 2) G.supHalt = std::max(G.supHalt, 10);
    logMsg(std::string("Событие: ") + e.title);
    sfx(e.dm < 1.0 ? SFX_BAD : SFX_DONE);
}

// ===== Патенты и суды =====
struct Pat { const char* n; const char* need; double cost; double roy; };
const int NPAT = 6;
const Pat PATS[NPAT] = {
    {"Патент на корпус и дизайн", "нужно выпустить устройство", 15000, 500},
    {"Патент: архитектура процессора", "нужен готовый свой процессор", 40000, 2500},
    {"Патент: интерфейс ОС", "нужна готовая своя ОС", 35000, 2200},
    {"Патент: камера", "нужно устройство с камерой", 30000, 1800},
    {"Патент: батарея", "нужна литий-ионная батарея или лучше", 25000, 1500},
    {"Патент: нейросетевой модуль", "нужно изучить «Нейросети»", 60000, 4000},
};
const char* const SUIT_REASON[4] = {"нарушение патента на корпус", "использование похожего интерфейса",
                                    "спорную схему питания", "копирование рекламного слогана"};
bool patReq(int i) {
    switch (i) {
        case 0: return !G.products.empty();
        case 1: return PRJ_CPU.st == 3;
        case 2: return PRJ_OS.st == 3;
        case 3:
            for (size_t k = 0; k < G.products.size(); ++k) if (G.products[k].p[P_CAMN] > 0) return true;
            return false;
        case 4:
            for (size_t k = 0; k < G.products.size(); ++k) if (G.products[k].p[P_BATT] >= 3) return true;
            return false;
        default:
            for (size_t t = 0; t < TECHS.size(); ++t)
                if (TECHS[t].name.rfind("Нейросети", 0) == 0 && techDone((int)t)) return true;
            return false;
    }
}
int patentCount() { int n = 0; for (int i = 0; i < NPAT; ++i) if ((G.patents >> i) & 1) ++n; return n; }
double patentRoyalty() {
    double r = 0;
    for (int i = 0; i < NPAT; ++i) if ((G.patents >> i) & 1) r += PATS[i].roy;
    return r * G.infl;
}
double suitWinChance() { return clampd(0.30 + 0.12 * patentCount(), 0.30, 0.85); }

void newGame(const std::string& name, int capIdx) {
    G = Game();
    PRJ_CPU = Proj();
    PRJ_OS = Proj();
    for (int i = 0; i < NCOMP; ++i) G.comp[i] = 900.0 * std::pow(0.85, i) * (0.9 + 0.2 * frand());
    OS_OPT[1].q = 0;
    CPUSRC_OPT[1].q = 0;
    G.company = name.empty() ? "Девайс-Корп" : name;
    const double caps[3] = {500000, 150000, 50000};
    G.money = caps[clampi(capIdx, 0, 2)];
    G.rep = capIdx == 2 ? 0 : 5;
    G.techDone.assign(TECHS.size(), 0);
    G.trend = irand(0, 7);
    refreshCandidates();
    G.draft.cat = 0;
    G.draft.p[P_SIZE] = CATS[0].idealSize;
    G.draft.price = CATS[0].defPrice;
    batchChoice = -1;
    logMsg("Компания «" + G.company + "» основана!");
    logMsg("Модный цвет года: " + std::string(COLOR_OPT[G.trend].name));
    selProduct = -1;
    resIdx = -1;
    dayTimer = 0;
}

// ---------------------------------------------------------------------------
//  Симуляция
// ---------------------------------------------------------------------------
// Конкурент выпускает новую модель
void rivalRelease() {
    int open[NCAT], no = 0;
    for (int c = 0; c < NCAT; ++c) if (c < 2 || calYear() >= TECHS[(size_t)techCat[c]].year) open[no++] = c;
    if (no == 0) return;
    RProd r;
    // сильные компании выпускают новинки чаще
    double tot = 0;
    for (int i = 0; i < NCOMP; ++i) tot += std::sqrt(G.comp[i]);
    double pick = frand() * tot;
    for (int i = 0; i < NCOMP; ++i) {
        pick -= std::sqrt(G.comp[i]);
        r.comp = i;
        if (pick <= 0) break;
    }
    r.cat = open[irand(0, no - 1)];
    double top = 1.0;
    for (int i = 0; i < NCOMP; ++i) top = std::max(top, G.comp[i]);
    // сильные компании делают устройства лучше, но разброс велик
    r.rating = std::round(clampd(3.2 + 3.6 * frand() + 2.2 * std::sqrt(G.comp[r.comp] / top), 1.0, 9.5) * 10.0) / 10.0;
    double typ = CATS[r.cat].defPrice * G.infl * std::pow(1.035, years());
    r.price = std::max(5, (int)(typ * (0.6 + 0.08 * r.rating) * (0.9 + 0.2 * frand()) / 5.0) * 5);
    r.launchDay = G.day;
    r.num = calYear() % 100;  // модель называется по году выпуска
    G.rivals.push_back(r);
    G.comp[r.comp] *= 1.03;
    logMsg("Новинка: " + rprodName(r) + " (оценка " + fmt1(r.rating) + ", $" + fmtNum(r.price) + ")");
}

void monthEnd() {
    double cost = salaries() + officeRent() + factUpkeep() + supFee(G.supplier);
    // Проценты по кредиту
    cost += G.loan * loanRate() / 12.0;
    // Хранение склада: 1% себестоимости остатков в месяц
    for (size_t i = 0; i < G.products.size(); ++i)
        cost += G.products[i].stock * unitProdCost(G.products[i]) * 0.01;
    G.money -= cost;
    G.monExp += cost;
    double roy = patentRoyalty();
    if (roy > 0) { G.money += roy; G.monInc += roy; }
    G.histInc.push_back(G.monInc);
    G.histExp.push_back(G.monExp);
    if (G.histInc.size() > 36) { G.histInc.erase(G.histInc.begin()); G.histExp.erase(G.histExp.begin()); }
    G.monInc = 0; G.monExp = 0;
    refreshCandidates();
    G.rep = std::max(0.0, G.rep * 0.985);
    for (int i = 0; i < NCOMP; ++i) G.comp[i] *= 1.0 + 0.01 + 0.02 * frand();
    for (int i = 0; i < NCOMP; ++i) G.rivalProfit[i] += G.comp[i] * 6.0 * G.infl * (0.6 + 0.8 * frand());

    // Распад СССР: Казахстан и Беларусь становятся отдельными рынками
    if (!G.ussrSplit && calYear() >= 1992) {
        G.ussrSplit = true;
        G.market[1] = true;
        G.market[2] = true;
        logMsg("СССР больше нет: Россия, Казахстан и Беларусь — отдельные рынки");
    }

    // Сотрудники: опыт, настроение, просьбы о прибавке, переманивание
    bool busy = G.researching >= 0 || devCount() > 0;
    for (size_t i = 0; i < G.staff.size();) {
        Staff& st = G.staff[i];
        int drop = std::max(2, 6 - G.office) + (G.crunch ? 8 : 0) + (st.ask > 0 ? 8 : 0);
        st.mood = std::max(10, st.mood - drop);
        st.xp += busy ? 2 : 1;
        if (st.skill < 10 && st.xp >= 10 + 6 * st.skill) {
            st.skill++;
            st.xp = 0;
            logMsg(st.name + " набрался опыта: навык " + std::to_string(st.skill));
        }
        int fairSal = marketSalary(st.skill);
        if (st.ask == 0 && st.salary < fairSal * 0.9 && frand() < 0.3) {
            st.ask = fairSal;
            logMsg(st.name + " просит поднять оклад до " + fmtMoney(st.ask));
        }
        if (st.ask > 0 && st.salary >= st.ask) st.ask = 0;
        double pc = st.skill >= 5 ? (st.mood < 40 ? 0.12 : 0.04) : (st.mood < 25 ? 0.08 : 0.0);
        if (frand() < pc) {
            logMsg(st.name + " ушёл к конкурентам");
            toast(st.name + " ушёл к конкурентам");
            sfx(SFX_BAD);
            G.staff.erase(G.staff.begin() + (long)i);
        } else {
            ++i;
        }
    }

    // Товары: отзыв бракованной партии, недовольство без обновлений
    for (size_t i = 0; i < G.products.size(); ++i) {
        Product& p = G.products[i];
        if (!p.onSale) continue;
        if (p.defect > 0.09 && p.units > 100 && frand() < 0.12) {
            double refund = p.units * 0.08 * p.price;
            G.money -= refund;
            G.monExp += refund;
            p.profit -= refund;
            p.stock = 0;
            p.defect *= 0.6;
            G.rep = clampd(G.rep - 4, 0, 100);
            logMsg("Отзыв партии «" + p.name + "» из-за брака: -" + fmtMoney(refund));
            toast("Отзыв партии: «" + p.name + "»");
            sfx(SFX_BAD);
        }
        if (!p.warned && CATS[p.cat].w[P_OS] > 0 && calYear() >= 1985 &&
            G.day - std::max(p.launchDay, p.fwDay) > 240) {
            p.warned = true;
            G.rep = clampd(G.rep - 1, 0, 100);
            logMsg("«" + p.name + "» давно без обновлений: покупатели недовольны");
        }
    }

    // Ценовые войны: конкуренты отвечают на более дешёвые и не худшие модели игрока
    for (size_t i = 0; i < G.rivals.size(); ++i) {
        RProd& r = G.rivals[i];
        if (G.day - r.launchDay > 240) continue;
        for (size_t k = 0; k < G.products.size(); ++k) {
            const Product& p = G.products[k];
            if (!p.onSale || p.cat != r.cat || p.price >= r.price || p.rating < r.rating - 0.5) continue;
            if (frand() < 0.3) {
                r.price = std::max(5, (int)(r.price * 0.88 / 5.0) * 5);
                logMsg(rprodName(r) + ": цена снижена до $" + fmtNum(r.price) + " в ответ на «" + p.name + "»");
            }
            break;
        }
    }
    // Старые модели конкурентов уходят с рынка
    for (size_t i = 0; i < G.rivals.size();)
        if (G.day - G.rivals[i].launchDay > 400) G.rivals.erase(G.rivals.begin() + (long)i);
        else ++i;

    // Срыв поставок
    if (G.supHalt == 0 && frand() < SUPS[G.supplier].fail) {
        G.supHalt = 8;
        logMsg("Срыв поставок комплектующих: завод стоит 8 дней");
        toast("Срыв поставок: завод стоит");
        sfx(SFX_BAD);
    }

    // Исторические события
    {
        int mon = (G.day / 30) % 12, yr = calYear();
        for (int i = 0; i < NEV; ++i)
            if (EVS[i].year == yr && EVS[i].month == mon && G.pendEv < 0) triggerEvent(i);
        if (yr >= 2024 && G.pendEv < 0 && frand() < 0.08) triggerEvent(NEV + irand(0, 2));
    }

    // Иск от конкурента
    if (!G.suit.active && !G.products.empty() && G.pendEv < 0 && frand() < 0.03 / (1.0 + 0.15 * patentCount())) {
        G.suit.active = true;
        G.suit.rival = irand(0, NCOMP - 1);
        G.suit.claim = clampd(G.money * 0.1 * (0.5 + frand()), ic(8000), ic(400000));
        G.suit.reason = irand(0, 3);
        logMsg(std::string(COMP_NAMES[G.suit.rival]) + " подал на вас в суд");
        sfx(SFX_BAD);
    }
    if (screen != SC_AWARDS && screen != SC_OVER) {
        if (G.pendEv >= 0) { evRet = screen; setScreen(SC_EVENT); }
        else if (G.suit.active) { evRet = screen; setScreen(SC_SUIT); }
    }

    if (G.money < 0) {
        G.negMonths++;
        if (G.negMonths >= 3) {
            logMsg("Компания обанкротилась.");
            sfx(SFX_BAD);
            setScreen(SC_OVER);
        } else {
            logMsg("Деньги в минусе! До банкротства: " + std::to_string(3 - G.negMonths) + " мес.");
            toast("Баланс в минусе: возьмите кредит в банке");
            sfx(SFX_ERR);
        }
    } else {
        G.negMonths = 0;
    }
}

// Итоги года: налог на прибыль, дивиденды инвесторам, инфляция
void yearEnd() {
    double profit = annualSum(true);
    G.lastTax = 0;
    G.lastDiv = 0;
    if (profit > 0) {
        G.lastTax = profit * TAX_RATE;
        G.money -= G.lastTax;
        G.monExp += G.lastTax;
        logMsg("Налог на прибыль за год: " + fmtMoney(G.lastTax));
        if (G.stake < 0.999) {
            G.lastDiv = profit * (1.0 - TAX_RATE) * (1.0 - G.stake) * 0.5;
            G.money -= G.lastDiv;
            G.monExp += G.lastDiv;
            logMsg("Дивиденды инвесторам: " + fmtMoney(G.lastDiv));
        }
    }
    double rate = 0.02 + 0.025 * frand();
    if (G.evDays > 0 && G.evCost > 1.05) rate += 0.04;  // кризисы разгоняют цены
    G.lastInfl = rate;
    G.infl *= 1.0 + rate;
    logMsg("Инфляция за год: " + fmt1(rate * 100.0) + "%. Цены и зарплаты растут.");
}


std::vector<Award> yearAwards;
int awardYearNo = 0;
float ceremonyT = 0.0f;  // время церемонии награждения, сек

// Оценка лучшей новинки конкурента i в категории c
bool catOpenNow(int c) { return c < 2 || calYear() >= TECHS[(size_t)techCat[c]].year; }

void computeAwards(int y) {  // y — номер нового года от старта; награждаем прошедший
    yearAwards.clear();
    ceremonyT = 0.0f;
    awardYearNo = START_YEAR + y - 1;
    const int from = (y - 1) * 360, to = y * 360;
    const char* titles[NCAT] = {"Лучший радиоприёмник года", "Лучший телефон года", "Лучшие часы года",
                                "Лучший калькулятор года", "Лучший компьютер года"};
    auto add = [&](const char* title, const std::string& winner, int owner, const std::string& detail) {
        Award a;
        a.title = title; a.winner = winner; a.detail = detail; a.year = awardYearNo; a.owner = owner;
        yearAwards.push_back(a);
        G.awardLog.push_back(a);
    };

    // Лучшая компания: прибыль за последние 12 месяцев у игрока и за год у конкурентов
    double profit = 0;
    int n = (int)G.histInc.size();
    for (int i = std::max(0, n - 12); i < n; ++i) profit += G.histInc[(size_t)i] - G.histExp[(size_t)i];
    double top = profit;
    std::string topName = G.company;
    int topOwner = -1;
    for (int i = 0; i < NCOMP; ++i)
        if (G.rivalProfit[i] > top) { top = G.rivalProfit[i]; topName = COMP_NAMES[i]; topOwner = i; }
    if (top > 0)
        add("Лучшая компания года", topName, topOwner, "Прибыль за год " + fmtMoney(top));

    // Лучшее устройство в каждой открытой категории: игрок против конкурентов
    for (int c = 0; c < NCAT; ++c) {
        if (!catOpenNow(c)) continue;
        double bestR = -1;
        std::string bn, bd;
        int owner = -1;
        for (size_t i = 0; i < G.products.size(); ++i) {
            const Product& p = G.products[i];
            if (p.cat != c || p.launchDay < from || p.launchDay >= to) continue;
            if (p.rating > bestR) {
                bestR = p.rating; bn = p.name; owner = -1;
                bd = "Оценка " + fmt1(p.rating) + ", продано " + fmtNum(p.units) + " шт.";
            }
        }
        for (size_t i = 0; i < G.rivals.size(); ++i) {
            const RProd& rp = G.rivals[i];
            if (rp.cat != c || rp.launchDay < from || rp.launchDay >= to) continue;
            if (rp.rating > bestR) {
                bestR = rp.rating; owner = rp.comp;
                bn = rprodName(rp);
                bd = "Оценка " + fmt1(rp.rating) + ", цена $" + fmtNum(rp.price);
            }
        }
        if (bestR >= 0) add(titles[c], bn, owner, bd);
    }

    // Самое продаваемое устройство: игрок против конкурентов
    double bu = -1;
    std::string bsn, bsd;
    int bsOwner = -1;
    for (size_t i = 0; i < G.products.size(); ++i) {
        const Product& p = G.products[i];
        if (p.launchDay < from || p.launchDay >= to) continue;
        if (p.units > bu) { bu = p.units; bsn = p.name; bsOwner = -1; bsd = "Продано " + fmtNum(p.units) + " шт."; }
    }
    for (int i = 0; i < NCOMP; ++i) {
        double u = G.comp[i] * 200.0 * (0.5 + frand());
        if (u > bu) {
            bu = u; bsOwner = i;
            bsn = std::string(COMP_NAMES[i]) + " " + CATS[0].series + " " + SUFFIX[i % 10];
            bsd = "Продано " + fmtNum(u) + " шт.";
        }
    }
    if (bu >= 0) add("Самое продаваемое устройство года", bsn, bsOwner, bsd);

    // Годовая прибыль конкурентов начинается заново
    for (int i = 0; i < NCOMP; ++i) G.rivalProfit[i] = 0;
}

void announceYear(int y) {
    std::string first;
    int cnt = 0;
    for (size_t i = 0; i < TECHS.size(); ++i)
        if (TECHS[i].year == START_YEAR + y) { if (cnt == 0) first = TECHS[i].name; ++cnt; }
    logMsg("Наступил " + std::to_string(START_YEAR + y) + " год. Новых технологий: " + std::to_string(cnt));
    computeAwards(y);
    setScreen(SC_AWARDS);
    if (cnt > 0) toast("Новое в мире: " + first);
    sfx(SFX_DONE);
}

void advanceProj(Proj& pr, const PO (&st)[3][3], Opt& own, const char* const* names, const char* what) {
    if (pr.st >= 3 || pr.pick < 0) return;
    pr.prog += devSpeed();
    const PO& o = st[pr.st][pr.pick];
    if (pr.prog < o.days) return;
    pr.q += o.q;
    own.q = pr.q;
    pr.st++;
    pr.pick = -1;
    pr.prog = 0;
    if (pr.st == 3) {
        logMsg(std::string(what) + " готов. Качество: " + std::to_string(pr.q));
        toast(std::string(what) + " готов!");
        sfx(SFX_DONE);
    } else {
        logMsg(std::string(what) + ": этап «" + names[pr.st - 1] + "» завершён");
    }
}

void tickProjects() {
    advanceProj(PRJ_CPU, CPU_STAGES, CPUSRC_OPT[1], CPU_NAMES, "Процессор");
    advanceProj(PRJ_OS, OS_STAGES, OS_OPT[1], OS_NAMES, "Операционная система");
}

void tickDay() {
    G.day++;
    if (G.evDays > 0) G.evDays--;
    if (G.day > 0 && G.day % 360 == 0) announceYear(G.day / 360);
    if (G.researching >= 0) {
        G.researchProg += researchRate();
        const Tech& t = TECHS[(size_t)G.researching];
        if (G.researchProg >= t.cost) {
            G.techDone[(size_t)G.researching] = 1;
            logMsg("Исследование завершено: " + t.name);
            toast("Изучено: " + t.name);
            sfx(SFX_DONE);
            G.researching = -1;
            G.researchProg = 0;
            fixDraft(G.draft);
        }
    }
    tickProjects();
    // Разработка устройств: несколько проектов делят команду
    {
        double sp = slotSpeed();
        for (int i = 0; i < NSLOT; ++i) {
            DevSlot& s = G.slot[i];
            if (!s.active || s.ready) continue;
            s.prog += sp;
            if (G.crunch) s.crunch = true;
            if (s.prog >= s.total) {
                s.ready = true;
                logMsg("Разработка «" + s.name + "» завершена. Пора запускать!");
                toast("«" + s.name + "» готов к запуску!");
                sfx(SFX_DONE);
            }
        }
    }
    // Производство: завод выпускает заказанные партии по очереди
    {
        double capLeft = factoryCap();
        if (G.supHalt > 0) { G.supHalt--; capLeft = 0; }
        G.prodToday = 0;
        for (size_t i = 0; i < G.products.size() && capLeft > 0; ++i) {
            Product& p = G.products[i];
            if (p.queue <= 0) continue;
            double m = std::min(p.queue, capLeft);
            p.queue -= m;
            p.stock += m;
            capLeft -= m;
            G.prodToday += m;
        }
    }
    G.lastDemand = 0;
    G.todayUnits = 0;
    G.todayRev = 0;
    int sameCat[NCAT] = {0, 0, 0, 0, 0};
    for (size_t i = 0; i < G.products.size(); ++i) if (G.products[i].onSale) sameCat[G.products[i].cat]++;
    for (size_t i = 0; i < G.products.size(); ++i) {
        Product& p = G.products[i];
        if (!p.onSale) continue;
        int age = G.day - p.launchDay;
        double demand = productDemand(p, sameCat[p.cat]);
        if (p.adDays > 0) p.adDays--;
        G.lastDemand += demand;
        p.avgD = p.avgD <= 0 ? demand : p.avgD * 0.85 + demand * 0.15;
        demand *= 0.85 + 0.3 * frand();
        double u = p.carry + demand;
        double want = std::floor(u);
        p.carry = u - want;
        // Продать можно только то, что есть на складе
        double whole = std::min(want, std::floor(p.stock));
        p.lost = want - whole;
        p.stock -= whole;
        double rev = whole * p.price;
        double duty = rev * p.tariffNow;                          // пошлины стран
        double ret = whole * p.defect;                            // гарантийные возвраты
        double warr = ret * unitProdCost(p) * 0.8;
        p.returns += ret;
        p.units += whole; p.revenue += rev; p.profit += rev - duty - warr; p.lastDay = whole; p.weekAcc += whole;
        G.money += rev - duty - warr;
        G.monInc += rev;
        G.monExp += duty + warr;
        G.todayUnits += whole;
        G.todayRev += rev;
        G.fans += whole * 0.3;
        if (age % 7 == 0) {
            p.weekly.push_back((int)p.weekAcc);
            p.weekAcc = 0;
            if (p.weekly.size() > 104) p.weekly.erase(p.weekly.begin());
        }
        // Отзывы покупателей через месяц после старта: брак бьёт по оценке
        if (!p.fbDone && age >= 30) {
            p.fbDone = true;
            if (p.defect > 0.07) {
                double drop = std::min(1.0, std::round((p.defect - 0.05) * 120.0) / 10.0);
                p.rating = clampd(p.rating - drop, 1.0, 10.0);
                G.rep = clampd(G.rep - 2, 0, 100);
                logMsg("Покупатели жалуются на брак «" + p.name + "»: оценка упала до " + fmt1(p.rating));
                toast("Жалобы на брак: «" + p.name + "»");
                sfx(SFX_BAD);
            }
        }
        if (age > 40 && p.lifeNow < 0.04) {
            discontinue(p, "снят с продаж: спрос иссяк");
            continue;
        }
        // Автозаказ: держим запас на две недели, пока товар ещё покупают
        if (p.autoProd && p.lifeNow > 0.08) {
            double have = p.stock + p.queue;
            if (have < 7.0 * p.avgD) orderBatch(p, std::ceil(14.0 * p.avgD - have), true);
        }
    }
    G.hype *= 0.98;
    // Конкуренты выпускают новинки: чем больше открытых категорий, тем чаще
    {
        int no = 0;
        for (int c = 0; c < NCAT; ++c) if (catOpenNow(c)) ++no;
        if (frand() < no / 60.0) rivalRelease();
    }
    if (G.day % 30 == 0) monthEnd();
    if (G.day % 360 == 0) {
        G.trend = irand(0, 7);
        logMsg("Новый год! Модный цвет: " + std::string(COLOR_OPT[G.trend].name));
        toast("Новый год! Ожидания рынка выросли.");
        yearEnd();
    }
}

// Собирает товар из готового проекта (без побочных эффектов): нужен и для оценки спроса перед запуском
Product makeProduct(const DevSlot& s, int pres, int qa) {
    Product p;
    p.name = s.name;
    p.cat = s.d.cat;
    for (int i = 0; i < NPARAM; ++i) p.p[i] = s.d.p[i];
    p.price = s.d.price;
    p.unitCost = draftUnitCost(s.d);
    p.quality = draftQuality(s.d);
    p.pres = pres;
    p.qa = qa;
    p.inflAt = G.infl;
    p.defect = baseDefect(s.crunch, qa);
    p.rating = clampd(scoreFromQ(p.quality) + PRES[pres].bonus, 1.0, 10.0);
    p.launchDay = G.day;
    return p;
}
double presCost(int pres) { return ic(PRES[pres].cost * (0.7 + 0.3 * marketSize())); }
// Размеры стартовой партии
const int NBATCH = 12;
const double BATCHES[NBATCH] = {0, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000};
// Ожидаемый спрос в первый день продаж
double estDemand(const DevSlot& s, int pres, int qa) {
    Product p = makeProduct(s, pres, qa);
    int same = 1;
    for (size_t i = 0; i < G.products.size(); ++i) if (G.products[i].onSale && G.products[i].cat == p.cat) ++same;
    double saveHype = G.hype;
    G.hype = std::min(4.0, G.hype + PRES[pres].hype);
    double d = productDemand(p, same);
    G.hype = saveHype;
    return d;
}

void launchProduct(int si, int batchIdx) {
    DevSlot& s = G.slot[si];
    Product p = makeProduct(s, presChoice, qaChoice);
    double r = scoreFromQ(p.quality) + PRES[presChoice].bonus + (frand() - 0.5) * 0.4;
    if (p.price > fairPrice(p.unitCost, r) * 1.6) r -= 0.5;
    p.rating = clampd(std::round(r * 10.0) / 10.0, 1.0, 10.0);
    double pc = presCost(presChoice) + qaCost(qaChoice);
    G.money -= pc;
    G.monExp += pc;
    p.profit -= pc;
    // Стартовая партия изготовлена заранее и сразу попадает на склад
    double n = BATCHES[clampi(batchIdx, 0, NBATCH - 1)];
    if (n > 0) {
        double bc = n * unitProdCost(p) * volDisc(n);
        G.money -= bc;
        G.monExp += bc;
        p.profit -= bc;
        p.stock = n;
    }
    G.hype = std::min(4.0, G.hype + PRES[presChoice].hype);
    double oldRep = G.rep;
    double dRep = (p.rating - 5.0) * 1.5;
    if (dRep > 0) dRep *= 1.0 - G.rep / 110.0;
    G.rep = clampd(G.rep + dRep, 0, 100);
    resRepDelta = G.rep - oldRep;
    G.fans += p.rating * 60.0;
    for (int i = 0; i < 4; ++i) resOutlet[i] = clampd(std::round((p.rating + (frand() - 0.5) * 1.2) * 10) / 10, 1, 10);
    G.products.push_back(p);
    s = DevSlot();
    resIdx = (int)G.products.size() - 1;
    selProduct = resIdx;
    resT = 0;
    logMsg("Запуск «" + p.name + "»: оценка " + fmt1(p.rating) + "/10, брак " + fmt1(p.defect * 100.0) + "%");
    sfx(p.rating >= 5 ? SFX_LAUNCH : SFX_BAD);
}

// ---------------------------------------------------------------------------
//  Сохранение
// ---------------------------------------------------------------------------
void wstr(std::ostringstream& o, const std::string& s) { o << '\n' << s << '\n'; }
bool rstr(std::istringstream& is, std::string& s) {
    is >> std::ws;
    return (bool)std::getline(is, s);
}

bool saveGame() {
    if (savePath.empty()) return false;
    std::ostringstream o;
    o.precision(17);
    o << "DEVCORP3";
    wstr(o, G.company);
    o << G.money << ' ' << G.rep << ' ' << G.fans << ' ' << G.day << ' ' << G.speed << ' ' << G.office << ' '
      << G.hype << ' ' << G.trend << ' ' << G.negMonths << ' ' << G.monInc << ' ' << G.monExp << ' '
      << G.researching << ' ' << G.researchProg << ' ' << A.vol << '\n';
    o << G.techDone.size();
    for (size_t i = 0; i < G.techDone.size(); ++i) o << ' ' << (int)G.techDone[i];
    o << '\n';
    for (int i = 0; i < NMARKET; ++i) o << (G.market[i] ? 1 : 0) << ' ';
    for (int i = 0; i < NCAT; ++i) o << G.gen[i] << ' ';
    for (int i = 0; i < NCOMP; ++i) o << G.comp[i] << ' ';
    o << '\n';
    const std::vector<Staff>* lists[2] = {&G.staff, &G.cand};
    for (int l = 0; l < 2; ++l) {
        o << lists[l]->size() << '\n';
        for (size_t i = 0; i < lists[l]->size(); ++i) {
            const Staff& s = (*lists[l])[i];
            o << s.role << ' ' << s.skill << ' ' << s.salary << ' ' << s.mood;
            wstr(o, s.name);
        }
    }
    const Draft* ds[2] = {&G.draft, &G.slot[0].d};
    for (int k = 0; k < 2; ++k) {
        o << ds[k]->cat << ' ' << ds[k]->price << ' ' << ds[k]->suffix;
        for (int p = 0; p < NPARAM; ++p) o << ' ' << ds[k]->p[p];
        o << '\n';
    }
    o << (G.slot[0].active ? 1 : 0) << ' ' << (G.slot[0].ready ? 1 : 0) << ' ' << G.slot[0].prog << ' ' << G.slot[0].total;
    wstr(o, G.slot[0].name.empty() ? std::string("-") : G.slot[0].name);
    o << G.histInc.size();
    for (size_t i = 0; i < G.histInc.size(); ++i) o << ' ' << G.histInc[i] << ' ' << G.histExp[i];
    o << '\n' << G.products.size() << '\n';
    for (size_t i = 0; i < G.products.size(); ++i) {
        const Product& p = G.products[i];
        o << p.cat << ' ' << p.price << ' ' << p.unitCost << ' ' << p.quality << ' ' << p.rating << ' ' << p.pres
          << ' ' << p.launchDay << ' ' << (p.onSale ? 1 : 0) << ' ' << p.units << ' ' << p.revenue << ' '
          << p.profit << ' ' << p.weekAcc << ' ' << p.carry << ' ' << p.lastDay << ' ' << p.adDays << ' ' << p.adBoost;
        for (int k = 0; k < NPARAM; ++k) o << ' ' << p.p[k];
        o << ' ' << p.weekly.size();
        for (size_t k = 0; k < p.weekly.size(); ++k) o << ' ' << p.weekly[k];
        wstr(o, p.name);
    }
    o << G.log.size() << '\n';
    for (size_t i = 0; i < G.log.size(); ++i) o << G.log[i] << '\n';
    for (int i = 0; i < NCOMP; ++i) o << G.rivalProfit[i] << ' ';
    o << '\n' << G.awardLog.size() << '\n';
    for (size_t i = 0; i < G.awardLog.size(); ++i) {
        const Award& a = G.awardLog[i];
        o << a.year << ' ' << a.owner;
        wstr(o, a.title);
        wstr(o, a.winner);
        wstr(o, a.detail);
    }
    o << G.factory << ' ' << G.capFactor << ' ' << G.lastDemand << ' ' << G.patents << ' ' << G.evDemand << ' '
      << G.evCost << ' ' << G.evDays << ' ' << G.evActive << ' ' << G.pendEv << ' ' << (G.suit.active ? 1 : 0) << ' '
      << G.suit.rival << ' ' << G.suit.claim << ' ' << G.suit.reason << '\n';
    const Proj* prs[2] = {&PRJ_CPU, &PRJ_OS};
    for (int k = 0; k < 2; ++k)
        o << prs[k]->st << ' ' << prs[k]->pick << ' ' << prs[k]->prog << ' ' << prs[k]->q << ' ';
    o << '\n';
    // ---- Блок версии 3: склад, брак, финансы, снабжение, слоты, конкуренты
    o << G.infl << ' ' << G.lastInfl << ' ' << G.loan << ' ' << G.stake << ' ' << (G.ipo ? 1 : 0) << ' ' << G.lastTax
      << ' ' << G.lastDiv << ' ' << G.supplier << ' ' << G.supUntil << ' ' << G.supHalt << ' ' << G.prodToday << ' '
      << (G.crunch ? 1 : 0) << ' ' << (G.ussrSplit ? 1 : 0) << ' ' << (G.slot[0].crunch ? 1 : 0) << '\n';
    for (int l = 0; l < 2; ++l)
        for (size_t i = 0; i < lists[l]->size(); ++i) o << (*lists[l])[i].xp << ' ' << (*lists[l])[i].ask << ' ';
    o << '\n';
    for (size_t i = 0; i < G.products.size(); ++i) {
        const Product& p = G.products[i];
        o << p.stock << ' ' << p.queue << ' ' << (p.autoProd ? 1 : 0) << ' ' << p.avgD << ' ' << p.lost << ' '
          << p.defect << ' ' << p.returns << ' ' << p.qa << ' ' << (p.fbDone ? 1 : 0) << ' ' << p.inflAt << ' ' << p.fw
          << ' ' << p.fwDay << ' ' << p.lifeBonus << ' ' << (p.warned ? 1 : 0) << '\n';
    }
    for (int k = 1; k < NSLOT; ++k) {
        const DevSlot& sl = G.slot[k];
        o << (sl.active ? 1 : 0) << ' ' << (sl.ready ? 1 : 0) << ' ' << (sl.crunch ? 1 : 0) << ' ' << sl.prog << ' '
          << sl.total << ' ' << sl.d.cat << ' ' << sl.d.price << ' ' << sl.d.suffix;
        for (int q = 0; q < NPARAM; ++q) o << ' ' << sl.d.p[q];
        wstr(o, sl.name.empty() ? std::string("-") : sl.name);
    }
    o << G.rivals.size() << '\n';
    for (size_t i = 0; i < G.rivals.size(); ++i) {
        const RProd& r = G.rivals[i];
        o << r.comp << ' ' << r.cat << ' ' << r.num << ' ' << r.rating << ' ' << r.price << ' ' << r.launchDay << '\n';
    }
    std::string data = o.str();
    SDL_RWops* f = SDL_RWFromFile(savePath.c_str(), "wb");
    if (!f) return false;
    size_t wr = SDL_RWwrite(f, data.data(), 1, data.size());
    SDL_RWclose(f);
    if (wr == data.size()) saveExists = true;
    return wr == data.size();
}

bool loadGame() {
    if (savePath.empty()) return false;
    SDL_RWops* f = SDL_RWFromFile(savePath.c_str(), "rb");
    if (!f) return false;
    Sint64 sz = SDL_RWsize(f);
    if (sz <= 0 || sz > 50 * 1024 * 1024) { SDL_RWclose(f); return false; }
    std::string data((size_t)sz, '\0');
    size_t rd = SDL_RWread(f, &data[0], 1, (size_t)sz);
    SDL_RWclose(f);
    if (rd != (size_t)sz) return false;
    std::istringstream is(data);
    std::string magic;
    is >> magic;
    if (magic != "DEVCORP2" && magic != "DEVCORP3") return false;  // старые сохранения тоже читаются
    Game g;
    float vol = A.vol;
    if (!rstr(is, g.company)) return false;
    is >> g.money >> g.rep >> g.fans >> g.day >> g.speed >> g.office >> g.hype >> g.trend >> g.negMonths >> g.monInc >>
        g.monExp >> g.researching >> g.researchProg >> vol;
    size_t nt = 0;
    is >> nt;
    if (!is || nt != TECHS.size()) return false;
    g.techDone.assign(nt, 0);
    for (size_t i = 0; i < nt; ++i) { int v = 0; is >> v; g.techDone[i] = (char)(v ? 1 : 0); }
    for (int i = 0; i < NMARKET; ++i) { int v = 0; is >> v; g.market[i] = v != 0; }
    for (int i = 0; i < NCAT; ++i) is >> g.gen[i];
    for (int i = 0; i < NCOMP; ++i) is >> g.comp[i];
    std::vector<Staff>* lists[2] = {&g.staff, &g.cand};
    for (int l = 0; l < 2; ++l) {
        size_t n = 0;
        is >> n;
        if (!is || n > 1000) return false;
        for (size_t i = 0; i < n; ++i) {
            Staff s;
            is >> s.role >> s.skill >> s.salary >> s.mood;
            if (!rstr(is, s.name)) return false;
            s.role = clampi(s.role, 0, 2);
            s.mood = clampi(s.mood, 0, 100);
            lists[l]->push_back(s);
        }
    }
    Draft* ds[2] = {&g.draft, &g.slot[0].d};
    for (int k = 0; k < 2; ++k) {
        is >> ds[k]->cat >> ds[k]->price >> ds[k]->suffix;
        for (int p = 0; p < NPARAM; ++p) is >> ds[k]->p[p];
        ds[k]->cat = clampi(ds[k]->cat, 0, NCAT - 1);
        ds[k]->suffix = clampi(ds[k]->suffix, 0, 9);
        for (int p = 0; p < NPARAM; ++p) ds[k]->p[p] = clampi(ds[k]->p[p], 0, optCount(p) - 1);
    }
    int inDev = 0, devReady = 0;
    is >> inDev >> devReady >> g.slot[0].prog >> g.slot[0].total;
    g.slot[0].active = inDev != 0; g.slot[0].ready = devReady != 0;
    if (!rstr(is, g.slot[0].name)) return false;
    size_t nh = 0;
    is >> nh;
    if (!is || nh > 1000) return false;
    for (size_t i = 0; i < nh; ++i) {
        double a = 0, b = 0;
        is >> a >> b;
        g.histInc.push_back(a); g.histExp.push_back(b);
    }
    size_t np = 0;
    is >> np;
    if (!is || np > 100000) return false;
    for (size_t i = 0; i < np; ++i) {
        Product p;
        int onSale = 0;
        is >> p.cat >> p.price >> p.unitCost >> p.quality >> p.rating >> p.pres >> p.launchDay >> onSale >> p.units >>
            p.revenue >> p.profit >> p.weekAcc >> p.carry >> p.lastDay >> p.adDays >> p.adBoost;
        p.onSale = onSale != 0;
        p.cat = clampi(p.cat, 0, NCAT - 1);
        p.pres = clampi(p.pres, 0, 3);
        for (int k = 0; k < NPARAM; ++k) { is >> p.p[k]; p.p[k] = clampi(p.p[k], 0, optCount(k) - 1); }
        size_t nw = 0;
        is >> nw;
        if (!is || nw > 10000) return false;
        for (size_t k = 0; k < nw; ++k) { int v = 0; is >> v; p.weekly.push_back(v); }
        if (!rstr(is, p.name)) return false;
        g.products.push_back(p);
    }
    size_t nl = 0;
    is >> nl;
    if (!is || nl > 1000) return false;
    for (size_t i = 0; i < nl; ++i) {
        std::string s;
        if (!rstr(is, s)) break;
        g.log.push_back(s);
    }
    for (int i = 0; i < NCOMP; ++i) is >> g.rivalProfit[i];
    size_t na = 0;
    is >> na;
    for (size_t i = 0; i < na && i < 100000; ++i) {
        Award a;
        is >> a.year >> a.owner;
        if (!rstr(is, a.title) || !rstr(is, a.winner) || !rstr(is, a.detail)) break;
        g.awardLog.push_back(a);
    }
    int sa = 0;
    is >> g.factory >> g.capFactor >> g.lastDemand >> g.patents >> g.evDemand >> g.evCost >> g.evDays >> g.evActive >>
        g.pendEv >> sa >> g.suit.rival >> g.suit.claim >> g.suit.reason;
    g.suit.active = sa != 0;
    Proj lp[2];
    for (int k = 0; k < 2; ++k) is >> lp[k].st >> lp[k].pick >> lp[k].prog >> lp[k].q;
    if (!is) return false;
    if (magic == "DEVCORP3") {
        int ipo = 0, crunch = 0, split = 0, c0 = 0;
        is >> g.infl >> g.lastInfl >> g.loan >> g.stake >> ipo >> g.lastTax >> g.lastDiv >> g.supplier >> g.supUntil >>
            g.supHalt >> g.prodToday >> crunch >> split >> c0;
        g.ipo = ipo != 0; g.crunch = crunch != 0; g.ussrSplit = split != 0; g.slot[0].crunch = c0 != 0;
        for (int l = 0; l < 2; ++l)
            for (size_t i = 0; i < lists[l]->size(); ++i) is >> (*lists[l])[i].xp >> (*lists[l])[i].ask;
        for (size_t i = 0; i < g.products.size(); ++i) {
            Product& p = g.products[i];
            int ap = 1, fb = 0, wn = 0;
            is >> p.stock >> p.queue >> ap >> p.avgD >> p.lost >> p.defect >> p.returns >> p.qa >> fb >> p.inflAt >>
                p.fw >> p.fwDay >> p.lifeBonus >> wn;
            p.autoProd = ap != 0; p.fbDone = fb != 0; p.warned = wn != 0;
            p.qa = clampi(p.qa, 0, 2);
            p.fw = clampi(p.fw, 0, FW_MAX);
            p.defect = clampd(p.defect, 0.0, 0.5);
            p.stock = std::max(0.0, p.stock);
            p.queue = std::max(0.0, p.queue);
            if (p.inflAt < 0.01) p.inflAt = 1.0;
        }
        for (int k = 1; k < NSLOT; ++k) {
            DevSlot& sl = g.slot[k];
            int a = 0, r = 0, c = 0;
            is >> a >> r >> c >> sl.prog >> sl.total >> sl.d.cat >> sl.d.price >> sl.d.suffix;
            for (int q = 0; q < NPARAM; ++q) { is >> sl.d.p[q]; sl.d.p[q] = clampi(sl.d.p[q], 0, optCount(q) - 1); }
            sl.active = a != 0; sl.ready = r != 0; sl.crunch = c != 0;
            sl.d.cat = clampi(sl.d.cat, 0, NCAT - 1);
            sl.d.suffix = clampi(sl.d.suffix, 0, 9);
            if (sl.total < 1) sl.total = 1;
            if (!rstr(is, sl.name)) return false;
            if (!sl.active) sl.name.clear();
        }
        size_t nr = 0;
        is >> nr;
        if (!is || nr > 10000) return false;
        for (size_t i = 0; i < nr; ++i) {
            RProd r;
            is >> r.comp >> r.cat >> r.num >> r.rating >> r.price >> r.launchDay;
            r.comp = clampi(r.comp, 0, NCOMP - 1);
            r.cat = clampi(r.cat, 0, NCAT - 1);
            g.rivals.push_back(r);
        }
        if (!is) return false;
        g.supplier = clampi(g.supplier, 0, NSUP - 1);
        g.stake = clampd(g.stake, 0.3, 1.0);
        g.infl = clampd(g.infl, 0.1, 1000.0);
        g.loan = std::max(0.0, g.loan);
        g.supHalt = clampi(g.supHalt, 0, 60);
    } else {
        // Сохранение прежней версии: цены приводим к индексу, соответствующему прожитым годам
        g.infl = std::pow(1.03, g.day / 360.0);
        for (size_t i = 0; i < g.products.size(); ++i) g.products[i].inflAt = g.infl;
        g.ussrSplit = START_YEAR + g.day / 360 >= 1992;
    }
    if (g.slot[0].total < 1) g.slot[0].total = 1;
    if (!g.slot[0].active) g.slot[0].name.clear();
    for (int k = 0; k < 2; ++k) { lp[k].st = clampi(lp[k].st, 0, 3); lp[k].pick = clampi(lp[k].pick, -1, 2); }
    g.factory = clampi(g.factory, 0, NFACT - 1);
    g.capFactor = clampd(g.capFactor, 0.01, 1.0);
    if (g.evActive < 0 || g.evActive >= NEV + 3) g.evActive = -1;
    if (g.pendEv >= NEV + 3) g.pendEv = -1;
    g.suit.rival = clampi(g.suit.rival, 0, NCOMP - 1);
    g.suit.reason = clampi(g.suit.reason, 0, 3);
    g.office = clampi(g.office, 0, NOFFICE - 1);
    g.trend = clampi(g.trend, 0, 7);
    g.speed = clampi(g.speed, 0, 3);
    if (g.researching >= (int)TECHS.size()) g.researching = -1;
    G = g;
    PRJ_CPU = lp[0];
    PRJ_OS = lp[1];
    OS_OPT[1].q = PRJ_OS.q;
    CPUSRC_OPT[1].q = PRJ_CPU.q;
    A.vol = (float)clampd(vol, 0, 1);
    selProduct = G.products.empty() ? -1 : 0;
    resIdx = -1;
    batchChoice = -1;
    dayTimer = 0;
    return true;
}

// ============================================================================
//  Отрисовка устройства
// ============================================================================
void drawDevice(int cx, int cy, int cat, int size, int color, int scrTier, int camN = 0, int camSz = 0) {
    Col body = COLOR_RGB[color];
    Col edge = shade(body, 0.7);
    Col dark = {18, 22, 30, 255};
    Col scrC = {(Uint8)(20 + scrTier * 10), (Uint8)(40 + scrTier * 22), (Uint8)(80 + scrTier * 28), 255};
    double s = 0.85 + 0.12 * size;
    static const int catShape[5] = {3, 0, 2, 3, 4};  // радио и калькулятор — короб, телефон — вертикальный
    switch (catShape[cat]) {
        case 0: {
            int w = (int)(80 * s), h = (int)(160 * s);
            SDL_Rect b = {cx - w / 2, cy - h / 2, w, h};
            fillRound({b.x - 2, b.y - 2, b.w + 4, b.h + 4}, 14, edge);
            fillRound(b, 12, body);
            fillRound({b.x + 6, b.y + 16, w - 12, h - 32}, 6, scrC);
            fillRect(b.x + 10, b.y + 22, (w - 20) / 2, 3, shade(scrC, 1.5));
            fillCircle(cx, b.y + 8, 3, dark);
            fillRound({cx - 12, b.y + h - 11, 24, 5}, 2, edge);
            break;
        }
        case 1: {
            int rad = (int)(70 * s);
            for (int a = 180; a <= 360; a += 4) {
                double ang = a * 3.14159265 / 180.0;
                fillCircle(cx + (int)(std::cos(ang) * rad), cy + (int)(std::sin(ang) * rad), 7, edge);
            }
            for (int a = 180; a <= 360; a += 4) {
                double ang = a * 3.14159265 / 180.0;
                fillCircle(cx + (int)(std::cos(ang) * rad), cy + (int)(std::sin(ang) * rad), 5, body);
            }
            int cw = (int)(38 * s), ch = (int)(74 * s);
            fillRound({cx - rad - cw / 2, cy - 8, cw, ch}, 14, body);
            fillRound({cx + rad - cw / 2, cy - 8, cw, ch}, 14, body);
            fillRound({cx - rad + cw / 2 - 6, cy + 2, 8, ch - 20}, 3, dark);
            fillRound({cx + rad - cw / 2 - 2, cy + 2, 8, ch - 20}, 3, dark);
            break;
        }
        case 2: {
            int w = (int)(96 * s), h = (int)(112 * s);
            fillRect(cx - w / 3, cy - 120, w * 2 / 3, 240, edge);
            for (int i = 0; i < 5; ++i) fillRect(cx - 3, cy + h / 2 + 10 + i * 12, 6, 6, dark);
            fillRound({cx - w / 2, cy - h / 2, w, h}, 18, body);
            fillRound({cx - w / 2 + 8, cy - h / 2 + 8, w - 16, h - 16}, 12, scrC);
            fillRect(cx + w / 2, cy - 10, 6, 20, edge);
            drawTextC("12:00", cx, cy - 7, 2, C_TEXT);
            break;
        }
        case 3: {
            int w = (int)(240 * s), h = (int)(160 * s);
            SDL_Rect b = {cx - w / 2, cy - h / 2, w, h};
            fillRound({b.x - 2, b.y - 2, b.w + 4, b.h + 4}, 16, edge);
            fillRound(b, 14, body);
            fillRound({b.x + 12, b.y + 12, w - 24, h - 24}, 6, scrC);
            fillRect(b.x + 18, b.y + 18, w / 3, 3, shade(scrC, 1.5));
            fillCircle(b.x + w / 2, b.y + 6, 2, dark);
            break;
        }
        default: {
            int w = (int)(250 * s), h = (int)(150 * s);
            SDL_Rect lid = {cx - w / 2, cy - h / 2 - 20, w, h};
            fillRound(lid, 10, body);
            fillRect(lid.x + 10, lid.y + 10, w - 20, h - 20, scrC);
            fillRect(lid.x + 16, lid.y + 16, w / 3, 3, shade(scrC, 1.5));
            int by = lid.y + h;
            fillRect(lid.x - 20, by, w + 40, (int)(14 * s), edge);
            fillRect(lid.x - 20, by, w + 40, 4, body);
            fillRect(cx - 30, by + 4, 60, 4, shade(edge, 0.8));
            break;
        }
    }
    // Камеры: количество и размер линз зависят от настроек
    if (camN > 0) {
        int rad = 4 + camSz * 2;
        for (int i = 0; i < camN; ++i) {
            int x = cx - 30 + i * (2 * rad + 8), y = cy - 62;
            fillCircle(x, y, rad + 2, {60, 60, 70, 255});
            fillCircle(x, y, rad, {20, 40, 70, 255});
            fillCircle(x - rad / 3, y - rad / 3, std::max(1, rad / 4), {120, 170, 230, 255});
        }
    }
}

// ============================================================================
//  Экраны
// ============================================================================
// Пока у компании нет ни одного устройства, нужно создать первое в конструкторе
bool mustDesignFirst() { return G.products.empty() && !anyDev(); }

void goBack() {
    if (textActive) { textActive = false; SDL_StopTextInput(); return; }
    if (screen == SC_DESIGN && mustDesignFirst()) { toast("Сначала создайте первое устройство"); sfx(SFX_ERR); return; }
    switch (screen) {
        case SC_NEW: case SC_OVER: return;
        case SC_MAIN: setScreen(SC_PAUSE); break;
        case SC_PAUSE: setScreen(retScreen); break;
        case SC_PLATFORM: setScreen(retScreen); break;
        case SC_AWARDS: case SC_HALL: setScreen(SC_MAIN); break;
        case SC_ADS: setScreen(SC_LAUNCH); break;
        case SC_FACTORY: case SC_PATENT: case SC_BANK: setScreen(SC_MAIN); break;
        case SC_SUPPLY: setScreen(SC_FACTORY); break;
        case SC_EVENT: G.pendEv = -1; setScreen(evBack()); break;
        case SC_SUIT: return;
        case SC_LAUNCH:
            if (resIdx >= 0) resIdx = -1;
            else setScreen(SC_MAIN);
            break;
        default: setScreen(SC_MAIN); break;
    }
    sfx(SFX_BACK);
}

void topBar() {
    fillRect(0, 0, W, 64, C_PANEL2);
    fillRect(0, 63, W, 1, C_BORDER);
    if (screen != SC_MAIN) {
        if (button({8, 8, 130, 48}, "< Назад")) goBack();
        const char* titles[] = {"", "", "Конструктор", "Запуск", "Офис", "Наука", "Рынки", "Статистика", "", "", "Платформа", "Итоги", "Награды", "Реклама", "Производство", "Патенты", "Событие", "Иск", "Банк", "Снабжение"};
        drawText(titles[screen], 150, 25, 2, C_DIM);
    } else {
        drawText(fitText(G.company, 2, 310), 16, 25, 2, C_ACC);
    }
    {
        std::string m = fmtMoney(G.money);
        int msc = textW(m, 3) > 236 ? 2 : 3;
        drawText(m, 340, msc == 3 ? 18 : 25, msc, G.money < 0 ? C_RED : C_GREEN);
    }
    drawText("Реп. " + std::to_string((int)G.rep), 590, 25, 2, C_YEL);
    drawText(std::string(MONTHS[(G.day / 30) % 12]) + " " + std::to_string(START_YEAR + G.day / 360), 730, 25, 2, C_TEXT);
    const char* sp[4] = {"||", "1x", "2x", "4x"};
    int x = W - 8 - 100 - 8 - 4 * 60;
    for (int i = 0; i < 4; ++i)
        if (button({x + i * 60, 8, 56, 48}, sp[i], true, G.speed == i)) G.speed = i;
    if (button({W - 108, 8, 100, 48}, "Меню", !(screen == SC_DESIGN && mustDesignFirst()))) setScreen(SC_PAUSE);
}

void scrMain() {
    if (G.pendEv >= 0) { evRet = SC_MAIN; setScreen(SC_EVENT); return; }
    if (G.suit.active) { evRet = SC_MAIN; setScreen(SC_SUIT); return; }
    // Финансы
    panel(16, 76, 600, 140, "Финансы");
    drawText("Выручка сегодня: " + fmtMoney(G.todayRev), 32, 108, 2, C_TEXT);
    drawText("Продано сегодня: " + fmtNum(G.todayUnits) + " шт.", 32, 132, 2, C_TEXT);
    double prof = G.monInc - G.monExp;
    drawText("Итог месяца: " + fmtMoney(prof), 32, 156, 2, prof >= 0 ? C_GREEN : C_RED);
    drawText("Расходы: " + fmtMoney(salaries() + officeRent() + factUpkeep() + supFee(G.supplier) +
                                    G.loan * loanRate() / 12.0) + "/мес", 32, 180, 2, C_DIM);
    {
        double sn = seasonNow();
        drawTextR("Сезон: спрос x" + fmt1(sn), 600, 180, 2, sn > 1.05 ? C_GREEN : (sn < 0.95 ? C_ORANGE : C_DIM));
    }
    if (G.evDays > 0 && G.evActive >= 0) {
        drawText(fitText(EVS[G.evActive].title, 2, 270), 330, 108, 2, C_YEL);
        drawText("Спрос " + pct(G.evDemand) + ", расходы " + pct(G.evCost), 330, 132, 2, C_DIM);
        drawText("Осталось " + std::to_string(G.evDays) + " дн.", 330, 156, 2, C_DIM);
    } else {
        double stock = 0;
        for (size_t i = 0; i < G.products.size(); ++i) stock += G.products[i].stock;
        drawTextR("Склад: " + fmtShort(stock) + " шт.", 600, 108, 2, C_DIM);
        if (G.supHalt > 0) drawTextR("Завод стоит: " + std::to_string(G.supHalt) + " дн.", 600, 132, 2, C_RED);
        else drawTextR("Выпуск: " + fmtShort(G.prodToday) + "/день", 600, 132, 2, C_DIM);
        if (G.loan > 0) drawTextR("Долг: " + fmtShort(G.loan), 600, 156, 2, C_ORANGE);
    }
    // Компания
    panel(16, 226, 600, 110, "Компания");
    drawText("Репутация", 32, 258, 2, C_TEXT);
    progressBar({170, 256, 300, 18}, G.rep / 100.0, C_YEL);
    drawText(std::to_string((int)G.rep) + "/100", 484, 258, 2, C_YEL);
    drawText("Фанаты: " + fmtNum(G.fans), 32, 284, 2, C_TEXT);
    drawText("Рынки: " + std::to_string(marketsOpen()) + "/" + std::to_string(NMARKET) + "  (объём x" +
                 fmt1(marketSize()) + ")", 32, 308, 2, C_TEXT);
    // Разработка
    panel(16, 346, 600, 100, "Разработка");
    {
        int row = 0, act = 0;
        for (int i = 0; i < NSLOT; ++i) {
            const DevSlot& sl = G.slot[i];
            if (!sl.active) continue;
            ++act;
            int yy = 376 + row * 22;
            drawText(fitText(sl.name, 2, 140), 32, yy, 2, sl.ready ? C_GREEN : C_TEXT);
            if (sl.ready) {
                drawText("готов к запуску", 180, yy, 2, C_GREEN);
            } else {
                progressBar({180, yy, 100, 14}, sl.prog / sl.total, C_ACC);
                double left = std::max(0.0, (sl.total - sl.prog) / slotSpeed());
                drawText(std::to_string((int)std::ceil(left)) + " д.", 288, yy, 2, C_DIM);
            }
            ++row;
        }
        if (row == 0) drawText("Нет активного проекта", 32, 392, 2, C_DIM);
        drawTextR("Проектов: " + std::to_string(act) + "/" + std::to_string(maxSlots()) + (G.crunch ? "  АВРАЛ" : ""),
                  600, 356, 2, G.crunch ? C_ORANGE : C_DIM);
        if (readySlot() >= 0) {
            if (button({370, 380, 230, 54}, "К запуску >", true, true)) setScreen(SC_LAUNCH);
        } else if (button({370, 380, 230, 54}, "Создать", freeSlot() >= 0)) {
            setScreen(SC_DESIGN);
        }
    }
    // Исследования
    panel(16, 456, 600, 96, "Исследование");
    if (G.researching >= 0) {
        const Tech& t = TECHS[(size_t)G.researching];
        drawText(fitText(t.name, 2, 400), 32, 486, 2, C_TEXT);
        double left = std::max(0.0, (t.cost - G.researchProg) / researchRate());
        drawTextR("≈ " + std::to_string((int)std::ceil(left)) + " дн.", 600, 486, 2, C_DIM);
        progressBar({32, 514, 568, 20}, G.researchProg / t.cost, C_GREEN);
    } else {
        drawText("Ничего не изучается", 32, 500, 2, C_DIM);
        if (button({370, 486, 230, 54}, "Выбрать")) setScreen(SC_RESEARCH);
    }
    // Активные продукты
    panel(16, 562, 600, 150, "В продаже");
    int y = 592, shown = 0;
    for (int i = (int)G.products.size() - 1; i >= 0 && shown < 4; --i) {
        const Product& p = G.products[(size_t)i];
        if (!p.onSale) continue;
        drawText(fitText(p.name, 2, 260), 32, y, 2, C_TEXT);
        drawText(fmt1(p.rating), 310, y, 2, ratingCol(p.rating));
        if (p.stock < 1) drawText("нет на складе", 370, y, 2, C_RED);
        drawTextR(fmtNum(p.lastDay) + " шт/день", 600, y, 2, C_DIM);
        y += 28; ++shown;
    }
    if (!shown) drawText("Пока ничего не продаётся", 32, y, 2, C_DIM);

    // Навигация
    const char* labels[12] = {"Конструктор", "Запуск и продукты", "Офис и штат", "Исследования",
                              "Маркетинг и рынки", "Статистика", "Производство", "Поставщики",
                              "Банк и инвесторы", "Патенты и суды", "Сохранить", "Пауза и настройки"};
    const Screen targets[12] = {SC_DESIGN, SC_LAUNCH, SC_OFFICE, SC_RESEARCH, SC_MARKET, SC_STATS, SC_FACTORY, SC_SUPPLY,
                                SC_BANK, SC_PATENT, SC_MAIN, SC_PAUSE};
    for (int i = 0; i < 12; ++i) {
        SDL_Rect r = {640 + (i % 2) * 316, 76 + (i / 2) * 55, 304, 49};
        if (button(r, labels[i], true, i == 1 && readySlot() >= 0)) {
            if (i == 10) toast(saveGame() ? "Игра сохранена" : "Не удалось сохранить");
            else setScreen(targets[i]);
        }
    }
    panel(640, 410, 624, 302, "Новости");
    int ly = 442;
    int start = std::max(0, (int)G.log.size() - 9);
    for (int i = (int)G.log.size() - 1; i >= start; --i) {
        drawText(fitText(G.log[(size_t)i], 2, 592), 656, ly, 2, i == (int)G.log.size() - 1 ? C_TEXT : C_DIM);
        ly += 29;
    }
}

// ----- Иконки: рисуются примитивами, без ассетов -----
// kind 0..4  — категории (телефон, наушники, часы, планшет, ноутбук)
// kind 10..16 — параметры (размер, цвет, экран, процессор, батарея, динамики, упаковка)
// kind 17 — цена, 18 — название
void drawIcon(int kind, int cx, int cy, int S, Col c) {
    static const int catIcon[5] = {20, 0, 2, 19, 4};  // категории -> иконки
    if (kind >= 0 && kind < 5) kind = catIcon[kind];
    const double k = S / 40.0;
    const Col d = shade(c, 0.55);
    const Col scrC = {36, 84, 140, 255};
    auto px = [&](double x) { return cx - S / 2 + (int)(x * k); };
    auto py = [&](double y) { return cy - S / 2 + (int)(y * k); };
    auto R2 = [&](double x, double y, double w, double h, double rad, Col col) {
        SDL_Rect r = {px(x), py(y), std::max(1, (int)(w * k)), std::max(1, (int)(h * k))};
        fillRound(r, std::max(0, (int)(rad * k)), col);
    };
    auto CI = [&](double x, double y, double rad, Col col) {
        fillCircle(px(x), py(y), std::max(1, (int)(rad * k)), col);
    };
    switch (kind) {
        case 0:  // смартфон
            R2(10, 1, 20, 38, 4, c);
            R2(12, 6, 16, 28, 2, scrC);
            R2(17, 2, 6, 2, 1, d);
            CI(20, 37, 1.6, d);
            break;
        case 1:  // наушники
            CI(12, 22, 8, c);
            CI(28, 22, 8, c);
            R2(9, 22, 6, 14, 3, c);
            R2(25, 22, 6, 14, 3, c);
            CI(12, 22, 3, d);
            CI(28, 22, 3, d);
            break;
        case 2:  // часы
            R2(13, 0, 14, 8, 2, d);
            R2(13, 32, 14, 8, 2, d);
            R2(7, 6, 26, 28, 8, c);
            R2(10, 9, 20, 22, 6, scrC);
            R2(33, 16, 3, 8, 1, d);
            break;
        case 3:  // планшет
            R2(7, 1, 26, 38, 4, c);
            R2(10, 5, 20, 30, 2, scrC);
            CI(20, 37, 1.5, d);
            break;
        case 19:  // калькулятор
            R2(6, 2, 28, 36, 3, c);
            R2(9, 6, 22, 8, 1, scrC);
            for (int r = 0; r < 3; ++r)
                for (int q = 0; q < 3; ++q) R2(9 + q * 8, 18 + r * 6, 5, 4, 0, d);
            break;
        case 20:  // радиоприёмник
            R2(3, 10, 34, 22, 4, c);
            CI(12, 21, 6, d);
            R2(22, 15, 12, 12, 2, scrC);
            break;
        case 4:  // компьютер
            R2(6, 4, 28, 22, 3, c);
            R2(9, 7, 22, 16, 1, scrC);
            R2(1, 28, 38, 6, 2, c);
            R2(16, 29, 8, 1, 0, d);
            break;
        case 10:  // размер: линейка
            R2(3, 16, 34, 6, 2, c);
            for (int i = 0; i < 5; ++i) R2(4 + i * 8, 8, 2, 8, 0, c);
            R2(10, 28, 20, 2, 0, d);
            break;
        case 11:  // цвет: палитра
            CI(20, 20, 17, c);
            CI(14, 15, 4, {200, 50, 60, 255});
            CI(26, 15, 4, {50, 160, 90, 255});
            CI(20, 26, 4, {50, 100, 210, 255});
            break;
        case 12:  // экран
            R2(3, 5, 34, 24, 3, c);
            R2(6, 8, 28, 18, 1, scrC);
            R2(17, 29, 6, 4, 0, c);
            R2(11, 33, 18, 2, 1, c);
            break;
        case 13:  // процессор
            for (int i = 0; i < 3; ++i) {
                double o = 11 + i * 6;
                R2(o, 3, 2, 6, 0, d);
                R2(o, 31, 2, 6, 0, d);
                R2(3, o, 6, 2, 0, d);
                R2(31, o, 6, 2, 0, d);
            }
            R2(9, 9, 22, 22, 2, c);
            R2(14, 14, 12, 12, 1, scrC);
            break;
        case 14:  // батарея
            R2(3, 11, 32, 18, 3, c);
            R2(35, 16, 3, 8, 1, c);
            R2(6, 14, 20, 12, 1, {60, 210, 120, 255});
            break;
        case 15:  // динамики
            R2(9, 3, 22, 34, 6, c);
            CI(20, 15, 6, d);
            CI(20, 15, 3, scrC);
            CI(20, 29, 3, d);
            break;
        case 16:  // упаковка
            R2(4, 8, 32, 26, 2, c);
            R2(4, 8, 32, 8, 2, shade(c, 1.25));
            R2(18, 8, 4, 26, 0, d);
            break;
        case 34:  // источник ЦП: чип с выводами
            for (int i = 0; i < 3; ++i) {
                double o = 11 + i * 6;
                R2(o, 3, 2, 6, 0, d);
                R2(o, 31, 2, 6, 0, d);
                R2(3, o, 6, 2, 0, d);
                R2(31, o, 6, 2, 0, d);
            }
            R2(9, 9, 22, 22, 2, c);
            R2(14, 14, 12, 12, 1, scrC);
            break;
        case 33:  // ОС: окно с заголовком
            R2(4, 6, 32, 28, 3, c);
            R2(4, 6, 32, 6, 2, shade(c, 1.3));
            R2(8, 16, 24, 3, 0, scrC);
            R2(8, 22, 16, 3, 0, scrC);
            break;
        case 30:  // количество камер: три линзы
            CI(10, 20, 6, c); CI(20, 20, 6, c); CI(30, 20, 6, c);
            CI(10, 20, 3, d); CI(20, 20, 3, d); CI(30, 20, 3, d);
            break;
        case 31:  // разрешение: линза с лучами
            CI(20, 20, 14, c);
            CI(20, 20, 8, d);
            CI(20, 20, 3, scrC);
            break;
        case 32:  // сенсор: квадрат-матрица
            R2(6, 6, 28, 28, 2, c);
            for (int r = 0; r < 3; ++r)
                for (int q = 0; q < 3; ++q) R2(10 + q * 8, 10 + r * 8, 5, 5, 0, scrC);
            break;
        case 17:  // цена
            R2(4, 10, 32, 20, 4, c);
            CI(12, 20, 3, scrC);
            R2(18, 16, 14, 2, 0, scrC);
            R2(18, 22, 10, 2, 0, scrC);
            break;
        case 18:  // название
            R2(4, 8, 32, 24, 3, c);
            R2(8, 13, 24, 3, 0, scrC);
            R2(8, 20, 18, 3, 0, scrC);
            R2(8, 27, 22, 3, 0, scrC);
            break;
        default: break;
    }
}

// Плитка с иконкой слева и подписью справа (для категорий)
bool iconTile(SDL_Rect r, int kind, const std::string& label, bool enabled, bool sel) {
    bool hit = widget(r, enabled);
    bool hov = hovered(r);
    Col bg = !enabled ? C_BTND : (sel ? C_BTNSEL : (hov ? C_BTNH : C_BTN));
    fillRect(r, bg);
    fillRect(r.x, r.y, r.w, 2, shade(bg, 1.35));
    drawRect(r, shade(bg, 0.7));
    drawIcon(kind, r.x + 24, r.y + r.h / 2, 30, enabled ? C_TEXT : C_DIM);
    drawText(fitText(label, 2, r.w - 48), r.x + 44, r.y + (r.h - 14) / 2, 2, enabled ? C_TEXT : C_DIM);
    drawFocus(r);
    if (hit) sfx(SFX_CLICK);
    return hit;
}

void scrDesign() {
    Draft& d = G.draft;

    // Категории: плитки с иконками
    for (int c = 0; c < NCAT; ++c) {
        bool un = catUnlocked(c);
        SDL_Rect tr = {16 + c * 148, 66, 144, 58};
        if (iconTile(tr, c, un ? CATS[c].name : "Закрыто", un, d.cat == c)) {
            if (d.cat != c) {
                d.cat = c;
                d.p[P_SIZE] = CATS[c].idealSize;
                d.price = defPrice(c);
                fixDraft(d);
            }
        }
    }

    // Список параметров: иконка, название, управление, сегменты
    const Cat& cat = CATS[d.cat];
    SDL_Rect view = {524, 138, 740, 574};
    beginArea(0, view);
    int sy = scr(0);
    int rows = NPARAM + 2;
    for (int i = 0; i < rows; ++i) {
        int ry = view.y + i * 72 - sy;
        SDL_Rect row = {view.x, ry, view.w - 8, 64};
        fillRect(row, C_PANEL);
        drawRect(row, C_BORDER);
        if (i < NPARAM) {
            int p = i;
            drawIcon(p < 7 ? 10 + p : 23 + p, row.x + 30, ry + 32, 40, C_ACC);
            drawText(PARAM_NAMES[p], row.x + 60, ry + 25, 2, C_TEXT);
            if (cat.w[p] <= 0) {
                drawText("Не используется в этой категории", row.x + 232, ry + 25, 2, C_DIM);
                continue;
            }
            int n = optCount(p);
            bool allowed[8];
            for (int k = 0; k < n; ++k) allowed[k] = tierUnlocked(p, k);
            if (button({row.x + 170, ry + 6, 52, 52}, "-", d.p[p] > 0)) d.p[p]--;
            SDL_Rect sl = {row.x + 232, ry + 6, 370, 52};
            slider(sl, n, d.p[p], allowed);
            fillRect(sl, C_PANEL2);
            drawTextC(fitText(opt(p, d.p[p]).name, 2, 360), sl.x + sl.w / 2, sl.y + 6, 2, C_TEXT);
            int segW = sl.w / n;
            for (int k = 0; k < n; ++k) {
                SDL_Rect sg = {sl.x + k * segW + 2, sl.y + 28, segW - 4, 18};
                Col c;
                if (p == P_COLOR) c = COLOR_RGB[k];
                else if (!allowed[k]) c = {44, 48, 62, 255};
                else c = k <= d.p[p] ? C_ACC : C_BTN;
                fillRect(sg, c);
                if (k == d.p[p]) drawRect({sg.x - 2, sg.y - 2, sg.w + 4, sg.h + 4}, C_TEXT, 2);
                if (p == P_COLOR && k == G.trend) drawText("*", sg.x + sg.w / 2 - 5, sg.y + 2, 2, C_YEL);
            }
            bool canUp = d.p[p] < n - 1 && allowed[d.p[p] + 1];
            if (button({row.x + 618, ry + 6, 52, 52}, "+", canUp)) d.p[p]++;
            std::string cs;
            if (p == P_SIZE) cs = "x" + fmt1(SIZE_OPT[d.p[p]].cost);
            else cs = "+$" + fmtNum(opt(p, d.p[p]).cost * G.infl);
            drawTextR(cs, row.x + row.w - 6, ry + 46, 1, C_DIM);
        } else if (i == NPARAM) {
            drawIcon(17, row.x + 30, ry + 32, 40, C_ACC);
            drawText("Цена", row.x + 60, ry + 25, 2, C_TEXT);
            int st = priceStep(d.price);
            if (button({row.x + 170, ry + 6, 52, 52}, "-", d.price > 10)) d.price = std::max(10, d.price - st);
            double uc = draftUnitCost(d);
            double fair = fairPrice(uc, scoreFromQ(draftQuality(d)));
            fillRect(row.x + 232, ry + 6, 370, 52, C_PANEL2);
            drawTextC("$" + fmtNum(d.price), row.x + 417, ry + 10, 2, C_GREEN);
            drawTextC("рекомендуемая: $" + fmtNum(fair), row.x + 417, ry + 34, 2, C_DIM);
            if (button({row.x + 618, ry + 6, 52, 52}, "+", d.price < 99999)) d.price += st;
        } else {
            drawIcon(18, row.x + 30, ry + 32, 40, C_ACC);
            drawText("Название", row.x + 60, ry + 25, 2, C_TEXT);
            if (button({row.x + 170, ry + 6, 52, 52}, "<")) d.suffix = (d.suffix + 9) % 10;
            fillRect(row.x + 232, ry + 6, 370, 52, C_PANEL2);
            drawTextC(fitText(draftName(d), 2, 360), row.x + 417, ry + 24, 2, C_TEXT);
            if (button({row.x + 618, ry + 6, 52, 52}, ">")) d.suffix = (d.suffix + 1) % 10;
        }
    }
    endArea(rows * 72);

    // Предпросмотр слева: как будет выглядеть устройство
    panel(16, 138, 492, 574, "");
    fillRect(28, 150, 468, 236, C_PANEL2);
    fillCircle(262, 268, 104, {30, 46, 78, 255});
    drawDevice(262, 268, d.cat, d.p[P_SIZE], d.p[P_COLOR], d.p[P_SCREEN] + 1, d.p[P_CAMN], d.p[P_CAMSZ]);
    drawIcon(d.cat, 480, 166, 26, C_ACC);
    drawText(fitText(draftName(d), 2, 400), 40, 162, 2, C_DIM);

    double uc = draftUnitCost(d);
    double tool = toolingCost(d);
    double q = draftQuality(d);
    double sc = scoreFromQ(q);
    double days = draftDevTotal(d) / devSpeed();
    int y = 398;
    drawText("Себестоимость: " + fmtMoney(uc), 36, y, 2, C_TEXT); y += 26;
    double margin = d.price - uc;
    drawText("Маржа: " + fmtMoney(margin) + " (" + std::to_string((int)(margin / std::max(1, d.price) * 100)) + "%)",
             36, y, 2, margin >= 0 ? C_GREEN : C_RED);
    y += 26;
    drawText("Оснастка (разово): " + fmtMoney(tool), 36, y, 2, C_TEXT); y += 26;
    drawText("Разработка: ≈ " + std::to_string((int)std::ceil(days)) + " дн.", 36, y, 2, C_TEXT); y += 34;
    drawText("Ожидаемая оценка", 36, y + 8, 2, C_DIM);
    drawTextR(fmt1(sc), 496, y - 2, 4, ratingCol(sc));
    y += 36;
    for (int k = 0; k < 10; ++k)
        fillRect(36 + k * 46, y, 42, 12, (k + 1) <= (int)std::round(sc) ? ratingCol(sc) : C_PANEL2);
    y += 24;
    if (button({36, 586, 452, 46}, "Своя платформа: процессор и ОС")) {
        retScreen = SC_DESIGN;
        setScreen(SC_PLATFORM);
    }
    int fs = freeSlot();
    bool can = fs >= 0 && G.money >= tool;
    std::string lbl = fs < 0 ? (maxSlots() < NSLOT ? "Нет слота: нужен офис больше" : "Все слоты заняты")
                             : (G.money < tool ? "Не хватает денег" : "Начать разработку");
    if (button({36, 650, 460, 54}, lbl, can, can)) {
        G.money -= tool;
        G.monExp += tool;
        DevSlot& sl = G.slot[fs];
        sl = DevSlot();
        sl.d = d;
        sl.name = draftName(d);
        sl.active = true;
        sl.total = draftDevTotal(d);
        sl.crunch = G.crunch;
        G.gen[d.cat]++;
        logMsg("Начата разработка «" + sl.name + "»");
        toast("Разработка началась!");
        setScreen(SC_MAIN);
    }
}

void projectPanel(int x, const std::string& title, Proj& pr, const PO (&st)[3][3], const char* const* names,
                  const char* what) {
    panel(x, 96, 612, 600, "");
    drawText(title, x + 16, 108, 3, C_ACC);
    if (pr.st >= 3) {
        drawText("Готов. Качество: " + std::to_string(pr.q), x + 16, 160, 2, C_GREEN);
        return;
    }
    drawText("Этап " + std::to_string(pr.st + 1) + " из 3: " + names[pr.st], x + 16, 160, 2, C_TEXT);
    if (pr.pick >= 0) {
        const PO& o = st[pr.st][pr.pick];
        int left = (int)std::ceil(std::max(0.0, o.days - pr.prog));
        drawText("Идёт: " + std::string(o.n) + ", осталось " + std::to_string(left) + " дн.", x + 16, 200, 2, C_DIM);
        progressBar({x + 16, 232, 580, 22}, pr.prog / o.days, C_GREEN);
    } else {
        drawText("Выберите подход (оплата при старте этапа):", x + 16, 200, 2, C_DIM);
        for (int i = 0; i < 3; ++i) {
            const PO& o = st[pr.st][i];
            std::string lbl = std::string(o.n) + "  $" + fmtNum(ic(o.cost)) + "  " + std::to_string(o.days) + " дн.  +" +
                              std::to_string(o.q);
            SDL_Rect r = {x + 16, 270 + i * 74, 580, 60};
            if (button(r, lbl, G.money >= ic(o.cost))) {
                G.money -= ic(o.cost);
                G.monExp += ic(o.cost);
                pr.pick = i;
                pr.prog = 0;
                logMsg(std::string(what) + ": начат этап «" + o.n + "»");
                toast("Этап начат");
            }
        }
    }
    drawText("Накоплено качества: " + std::to_string(pr.q), x + 16, 520, 2, C_TEXT);
}

void scrPlatform() {
    drawText("Своя платформа: этапы разработки. Качество складывается из выбранных подходов.", 24, 76, 2, C_ACC);
    projectPanel(16, "Процессор", PRJ_CPU, CPU_STAGES, CPU_NAMES, "Процессор");
    projectPanel(652, "Операционная система", PRJ_OS, OS_STAGES, OS_NAMES, "ОС");
}

void scrAwards() {
    const float T = ceremonyT;
    const float listStart = 4.2f;
    const int n = (int)yearAwards.size();
    const float doneT = listStart + n * 0.6f + 0.6f;
    panel(180, 96, 920, 600, "Итоги " + std::to_string(awardYearNo) + " года");

    if (T < listStart) {
        // Церемония: топ-3 компании по прибыли поднимаются по очереди — 3-е, 2-е, 1-е
        double pp = 0;
        int cnt = (int)G.histInc.size();
        for (int i = std::max(0, cnt - 12); i < cnt; ++i) pp += G.histInc[(size_t)i] - G.histExp[(size_t)i];
        std::vector<std::pair<double, std::string>> pods;
        pods.push_back(std::make_pair(pp, G.company));
        for (int i = 0; i < NCOMP; ++i) pods.push_back(std::make_pair(G.rivalProfit[i], std::string(COMP_NAMES[i])));
        std::sort(pods.begin(), pods.end(),
                  [](const std::pair<double, std::string>& x, const std::pair<double, std::string>& y) {
                      return x.first > y.first;
                  });
        drawTextCShadow("Топ-3 компании года", W / 2, 116, 3, C_ACC);
        fillRect(260, 540, 760, 2, C_BORDER);
        const int cxOf[3] = {640, 400, 880};   // 1-е, 2-е, 3-е место
        const int tgt[3] = {240, 180, 130};
        const float st[3] = {2.2f, 1.1f, 0.0f};  // момент начала подъёма
        const Col medal[3] = {{236, 190, 60, 255}, {190, 200, 215, 255}, {200, 130, 70, 255}};
        for (int pl = 0; pl < 3 && pl < (int)pods.size(); ++pl) {
            float tt = clampd((T - st[pl]) / 0.9f, 0.0f, 1.0f);
            float e = 1.0f - (1.0f - tt) * (1.0f - tt) * (1.0f - tt);
            int h = (int)(tgt[pl] * e);
            int cx = cxOf[pl];
            if (h > 0) {
                fillRound({cx - 80, 540 - h, 160, h}, 8, medal[pl]);
                fillRect(cx - 80, 540 - h, 160, 6, shade(medal[pl], 1.25));
            }
            if (tt >= 1.0f) {
                bool me = pods[(size_t)pl].second == G.company;
                drawTextCShadow(std::to_string(pl + 1) + " место", cx, 550, 2, C_TEXT);
                drawTextC(fitText(pods[(size_t)pl].second, 2, 300), cx, 540 - tgt[pl] - 60, 2, me ? C_YEL : C_TEXT);
                double v = pods[(size_t)pl].first;
                drawTextC(fmtMoney(v), cx, 540 - tgt[pl] - 34, 2, v >= 0 ? C_GREEN : C_RED);
                if (pl == 0) {  // пульсирующая рамка у победителя
                    int pulse = (int)(3.0 + 2.0 * std::sin(uiTime * 5.0));
                    drawRect({cx - 80 - pulse, 540 - tgt[0] - pulse, 160 + 2 * pulse, tgt[0] + 2 * pulse}, C_YEL, 2);
                }
            }
        }
    } else {
        // Список наград: появляется по одной, каждая въезжает справа
        if (n == 0) drawTextC("В этом году наград нет", W / 2, 320, 3, C_DIM);
        for (int i = 0; i < n; ++i) {
            float rt = listStart + i * 0.6f;
            if (T < rt) break;
            float e = clampd((T - rt) / 0.35f, 0.0f, 1.0f);
            e = 1.0f - (1.0f - e) * (1.0f - e);
            int dx = (int)((1.0f - e) * 220.0f);
            const Award& a = yearAwards[(size_t)i];
            int y = 150 + i * 66;
            fillRound({212 + dx, y, 856, 58}, 10, C_PANEL2);
            drawText(a.title, 232 + dx, y + 8, 2, C_YEL);
            drawText(fitText(a.winner, 2, 500), 232 + dx, y + 32, 2, C_TEXT);
            drawTextR(fitText(a.detail, 2, 380), 1048 + dx, y + 32, 2, C_DIM);
        }
    }

    if (T < doneT) {
        if (button({560, 636, 200, 46}, "Пропустить")) ceremonyT = 1000.0f;
    } else {
        if (button({340, 636, 200, 46}, "История наград")) setScreen(SC_HALL);
        if (button({560, 636, 200, 46}, "Продолжить", true, true)) {
            evRet = SC_MAIN;
            setScreen(G.pendEv >= 0 ? SC_EVENT : (G.suit.active ? SC_SUIT : SC_MAIN));
        }
    }
}

void scrFactory() {
    panel(16, 76, 600, 636, "Производство");
    drawText(FACT_NAME[G.factory], 36, 118, 3, C_TEXT);
    double cap = factoryCap();
    double q = 0, stock = 0;
    for (size_t i = 0; i < G.products.size(); ++i) { q += G.products[i].queue; stock += G.products[i].stock; }
    drawText("Мощность: " + fmtNum(cap) + " шт/день", 36, 168, 2, C_TEXT);
    drawText("Выпущено вчера: " + fmtNum(G.prodToday) + " шт.", 36, 198, 2, C_TEXT);
    progressBar({36, 226, 560, 22}, G.prodToday / std::max(1.0, cap), G.prodToday >= cap * 0.95 ? C_RED : C_GREEN);
    if (G.supHalt > 0)
        drawText("Завод стоит: нет комплектующих (" + std::to_string(G.supHalt) + " дн.)", 36, 260, 2, C_RED);
    else if (q >= 1)
        drawText("В очереди: " + fmtNum(q) + " шт. (≈ " + std::to_string((int)std::ceil(q / std::max(1.0, cap))) + " дн.)",
                 36, 260, 2, q > cap * 7 ? C_RED : C_TEXT);
    else
        drawText("Очередь пуста: мощности хватает", 36, 260, 2, C_GREEN);
    drawText("Себестоимость: " + pct(factoryMult()) + " от базовой", 36, 296, 2, C_TEXT);
    drawText("Содержание: " + fmtMoney(factUpkeep()) + "/мес", 36, 324, 2, C_TEXT);
    drawText("На складе: " + fmtNum(stock) + " шт., спрос ≈ " + fmtNum(G.lastDemand) + " шт/день", 36, 352, 2, C_DIM);
    if (button({36, 610, 560, 56}, std::string("Поставщики: ") + SUPS[G.supplier].n)) setScreen(SC_SUPPLY);
    if (G.factory < NFACT - 1) {
        int n = G.factory + 1;
        bool open = calYear() >= FACT_YEAR[n];
        drawText(std::string("Дальше: ") + FACT_NAME[n], 36, 380, 2, C_ACC);
        drawText("Мощность " + fmtNum(FACT_CAP[n]) + " шт/день, себестоимость " + pct(FACT_MULT[n]), 36, 410, 2, C_DIM);
        drawText("Содержание " + fmtMoney(ic(FACT_UPKEEP[n])) + "/мес", 36, 438, 2, C_DIM);
        if (!open) drawText("Откроется в " + std::to_string(FACT_YEAR[n]) + " г.", 36, 466, 2, C_DIM);
        if (button({36, 520, 560, 64}, "Построить: " + fmtMoney(ic(FACT_COST[n])), open && G.money >= ic(FACT_COST[n]), open)) {
            G.money -= ic(FACT_COST[n]);
            G.monExp += ic(FACT_COST[n]);
            G.factory = n;
            logMsg(std::string("Новое производство: ") + FACT_NAME[n]);
            toast("Производство улучшено");
            sfx(SFX_DONE);
        }
    } else {
        drawText("Максимальный уровень", 36, 380, 2, C_GREEN);
    }

    panel(636, 76, 628, 636, "Уровни производства");
    for (int i = 0; i < NFACT; ++i) {
        int y = 124 + i * 96;
        bool cur = i == G.factory;
        fillRound({652, y, 596, 88}, 10, cur ? shade(C_BTNSEL, 0.45) : C_PANEL2);
        drawText(FACT_NAME[i], 668, y + 10, 2, cur ? C_YEL : C_TEXT);
        drawText(fmtNum(FACT_CAP[i]) + " шт/день, себестоимость " + pct(FACT_MULT[i]), 668, y + 36, 2, C_DIM);
        std::string st = i <= G.factory ? std::string("построено")
                                        : (calYear() >= FACT_YEAR[i] ? fmtMoney(ic(FACT_COST[i]))
                                                                     : "с " + std::to_string(FACT_YEAR[i]) + " г.");
        drawText(st, 668, y + 60, 2, i <= G.factory ? C_GREEN : C_DIM);
    }
}

void scrPatents() {
    panel(16, 76, 760, 636, "Патенты");
    for (int i = 0; i < NPAT; ++i) {
        int y = 124 + i * 96;
        bool own = ((G.patents >> i) & 1) != 0;
        bool req = patReq(i);
        fillRound({32, y, 728, 88}, 10, own ? shade(C_BTNSEL, 0.45) : C_PANEL2);
        drawText(PATS[i].n, 48, y + 10, 2, own ? C_YEL : C_TEXT);
        std::string info = own ? ("Роялти " + fmtMoney(ic(PATS[i].roy)) + "/мес")
                               : (req ? ("Подача " + fmtMoney(ic(PATS[i].cost)) + ", роялти " + fmtMoney(ic(PATS[i].roy)) + "/мес")
                                      : std::string(PATS[i].need));
        drawText(fitText(info, 2, 560), 48, y + 44, 2, C_DIM);
        if (button({620, y + 20, 130, 48}, own ? "Есть" : "Подать", !own && req && G.money >= ic(PATS[i].cost))) {
            G.money -= ic(PATS[i].cost);
            G.monExp += ic(PATS[i].cost);
            G.patents |= (1 << i);
            logMsg(std::string("Оформлен: ") + PATS[i].n);
            toast("Патент оформлен");
            sfx(SFX_DONE);
        }
    }

    panel(796, 76, 468, 636, "Права и суды");
    int pc = patentCount();
    drawText("Патентов: " + std::to_string(pc) + "/" + std::to_string(NPAT), 816, 124, 2, C_TEXT);
    drawText("Роялти: " + fmtMoney(patentRoyalty()) + "/мес", 816, 152, 2, C_GREEN);
    drawText("Шанс выиграть суд: " + std::to_string((int)std::round(suitWinChance() * 100)) + "%", 816, 180, 2, C_TEXT);
    drawText("Шанс иска в месяц: " + std::to_string((int)std::round(300.0 / (1.0 + 0.15 * pc)) / 100) + "%", 816, 208, 2, C_TEXT);
    drawText("Конкуренты могут подать иск.", 816, 270, 2, C_DIM);
    drawText("Чем больше патентов, тем реже иски", 816, 298, 2, C_DIM);
    drawText("и выше шанс выиграть дело.", 816, 326, 2, C_DIM);
    drawText("Роялти платят конкуренты", 816, 374, 2, C_DIM);
    drawText("за использование ваших решений.", 816, 402, 2, C_DIM);
}

void scrEvent() {
    if (G.pendEv < 0 || G.pendEv >= NEV + 3) { setScreen(evBack()); return; }
    const Ev& e = EVS[G.pendEv];
    panel(240, 150, 800, 420, "");
    drawTextCShadow("СОБЫТИЕ", W / 2, 176, 2, C_DIM);
    drawTextCShadow(fitText(e.title, 4, 740), W / 2, 214, 4, C_YEL);
    if (e.year > 0) drawTextC(std::to_string(e.year) + " год", W / 2, 268, 2, C_DIM);
    drawTextC(fitText(e.text, 2, 740), W / 2, 306, 2, C_TEXT);
    drawTextC("Спрос: " + pct(e.dm), W / 2, 358, 3, e.dm >= 1.0 ? C_GREEN : C_RED);
    drawTextC("Расходы на производство: " + pct(e.cm), W / 2, 398, 3, e.cm <= 1.0 ? C_GREEN : C_RED);
    drawTextC("Действует " + std::to_string(e.days) + " дн.", W / 2, 444, 2, C_DIM);
    if (button({540, 490, 200, 56}, "Понятно", true, true)) {
        G.pendEv = -1;
        setScreen(evBack());
    }
}

void scrSuit() {
    if (!G.suit.active) { setScreen(evBack()); return; }
    double claim = G.suit.claim, settle = claim * 0.4, fee = claim * 0.1, ch = suitWinChance();
    panel(240, 130, 800, 460, "");
    drawTextCShadow("Судебный иск", W / 2, 152, 4, C_RED);
    drawTextC(fitText(std::string(COMP_NAMES[G.suit.rival]) + " обвиняет вас: " + SUIT_REASON[G.suit.reason], 2, 740),
              W / 2, 218, 2, C_TEXT);
    drawTextC("Сумма иска: " + fmtMoney(claim), W / 2, 258, 3, C_YEL);
    drawTextC("Мировое соглашение: " + fmtMoney(settle), W / 2, 316, 2, C_DIM);
    drawTextC("Суд: адвокаты " + fmtMoney(fee) + ", победа даёт " + fmtMoney(claim * 0.3), W / 2, 344, 2, C_DIM);
    drawTextC("Поражение: " + fmtMoney(claim * 1.2), W / 2, 372, 2, C_DIM);
    drawTextC("Ваших патентов: " + std::to_string(patentCount()), W / 2, 408, 2, C_TEXT);
    if (button({290, 470, 340, 70}, "Урегулировать " + fmtMoney(settle))) {
        G.money -= settle;
        G.monExp += settle;
        logMsg("Иск урегулирован: " + fmtMoney(settle));
        toast("Иск урегулирован");
        G.suit.active = false;
        setScreen(evBack());
    }
    if (button({650, 470, 340, 70}, "В суд (шанс " + std::to_string((int)std::round(ch * 100)) + "%)", true, true)) {
        G.money -= fee;
        G.monExp += fee;
        if (frand() < ch) {
            double got = claim * 0.3;
            G.money += got;
            G.monInc += got;
            G.rep = clampd(G.rep + 3, 0, 100);
            logMsg("Суд выигран: +" + fmtMoney(got));
            toast("Суд выигран!");
            sfx(SFX_DONE);
        } else {
            double pay = claim * 1.2;
            G.money -= pay;
            G.monExp += pay;
            G.rep = clampd(G.rep - 5, 0, 100);
            logMsg("Суд проигран: -" + fmtMoney(pay));
            toast("Суд проигран");
            sfx(SFX_BAD);
        }
        G.suit.active = false;
        setScreen(evBack());
    }
}

void scrHall() {
    panel(16, 76, 760, 620, "История наград");
    SDL_Rect view = {28, 124, 736, 564};
    beginArea(9, view);
    int sy = scr(9);
    int rows = (int)G.awardLog.size();
    for (int j = 0; j < rows; ++j) {
        const Award& a = G.awardLog[(size_t)(rows - 1 - j)];  // новые сверху
        SDL_Rect r = {view.x, view.y + j * 70 - sy, view.w - 10, 62};
        fillRound(r, 8, a.owner < 0 ? shade(C_BTNSEL, 0.45) : C_PANEL2);
        drawText(std::to_string(a.year) + ": " + a.title, r.x + 12, r.y + 8, 2, a.owner < 0 ? C_YEL : C_TEXT);
        drawText(fitText(a.winner + " — " + a.detail, 2, r.w - 24), r.x + 12, r.y + 34, 2, C_DIM);
    }
    if (rows == 0) drawText("Наград пока нет. Они вручаются в конце года.", view.x + 12, view.y + 10, 2, C_DIM);
    endArea(rows * 70);

    // Рейтинг: компания и конкуренты, число наград и прибыль за текущий год
    panel(796, 76, 468, 620, "Рейтинг компаний");
    int cnt[NCOMP + 1] = {};
    for (size_t i = 0; i < G.awardLog.size(); ++i) cnt[G.awardLog[i].owner + 1]++;
    double pp = 0;
    int n = (int)G.histInc.size();
    for (int i = std::max(0, n - 12); i < n; ++i) pp += G.histInc[(size_t)i] - G.histExp[(size_t)i];
    SDL_Rect rv = {812, 124, 436, 564};
    beginArea(10, rv);
    int rsy = scr(10);
    for (int k = 0; k <= NCOMP; ++k) {
        int y = rv.y + k * 66 - rsy;
        bool me = k == 0;
        SDL_Rect rr = {rv.x, y, rv.w - 10, 60};
        fillRound(rr, 10, me ? shade(C_BTNSEL, 0.45) : C_PANEL2);
        drawText(fitText(me ? G.company : std::string(COMP_NAMES[k - 1]), 2, 280), rr.x + 12, y + 8, 2, me ? C_YEL : C_TEXT);
        drawText("Наград: " + std::to_string(cnt[k]), rr.x + 12, y + 34, 2, C_DIM);
        double pr = me ? pp : G.rivalProfit[k - 1];
        drawTextR(fmtMoney(pr), rr.x + rr.w - 12, y + 34, 2, pr >= 0 ? C_GREEN : C_RED);
    }
    endArea((NCOMP + 1) * 66);
}

void salesChart(SDL_Rect r, const Product& p) {
    fillRect(r, C_PANEL2);
    drawRect(r, C_BORDER);
    std::vector<int> w = p.weekly;
    if (p.onSale) w.push_back((int)p.weekAcc);
    int n = (int)w.size();
    int first = std::max(0, n - 26);
    int mx = 1;
    for (int i = first; i < n; ++i) mx = std::max(mx, w[(size_t)i]);
    drawText("Продажи по неделям (шт.)", r.x + 10, r.y + 8, 2, C_DIM);
    drawTextR("макс " + fmtNum(mx), r.x + r.w - 10, r.y + 8, 2, C_DIM);
    int bw = std::max(4, (r.w - 20) / 26);
    int base = r.y + r.h - 12, top = r.y + 34;
    for (int i = first; i < n; ++i) {
        int h = (int)((double)w[(size_t)i] / mx * (base - top));
        bool cur = p.onSale && i == n - 1;
        fillRect(r.x + 10 + (i - first) * bw, base - h, bw - 3, h, cur ? C_DIM : C_ACC);
    }
    if (n == 0) drawTextC("Нет данных", r.x + r.w / 2, r.y + r.h / 2, 2, C_DIM);
}

// Виды рекламы: название, цена кампании, длительность, множитель спроса, год появления
struct AdT { const char* n; double cost; int days; double boost; int year; };
const AdT ADS[9] = {
    {"Газетное объявление", 2000, 14, 1.4, 1960},
    {"Радиоролик", 5000, 21, 1.8, 1960},
    {"Журнальная реклама", 8000, 30, 1.6, 1960},
    {"Телевизионный ролик", 20000, 30, 2.2, 1962},
    {"Наружные щиты", 12000, 30, 1.7, 1965},
    {"Баннер в интернете", 15000, 30, 1.8, 1996},
    {"Таргетинг в соцсетях", 25000, 30, 2.0, 2008},
    {"Реклама через блогеров", 60000, 30, 2.6, 2015},
    {"ИИ-кампания", 90000, 30, 3.0, 2023},
};

void scrAds() {
    if (selProduct < 0 || selProduct >= (int)G.products.size()) { setScreen(SC_LAUNCH); return; }
    Product& p = G.products[(size_t)selProduct];
    panel(240, 100, 800, 600, "Реклама: " + p.name);
    if (button({900, 112, 120, 40}, "Назад")) setScreen(SC_LAUNCH);
    if (p.adDays > 0)
        drawText("Идёт кампания: осталось " + std::to_string(p.adDays) + " дн., спрос x" + fmt1(p.adBoost), 264, 150, 2, C_GREEN);
    else
        drawText("Кампании сейчас нет", 264, 150, 2, C_DIM);
    for (int i = 0; i < 9; ++i) {
        const AdT& a = ADS[i];
        bool open = calYear() >= a.year;
        int y = 186 + i * 52;
        SDL_Rect r = {264, y, 752, 46};
        fillRound(r, 8, C_PANEL2);
        drawText(fitText(a.n, 2, 296), r.x + 12, r.y + 13, 2, open ? C_TEXT : C_DIM);
        std::string info = open ? ("$" + fmtNum(ic(a.cost)) + " · " + std::to_string(a.days) + " дн. · x" + fmt1(a.boost))
                                : ("откроется в " + std::to_string(a.year));
        drawText(info, 580, r.y + 13, 2, C_DIM);
        if (button({936, y + 4, 96, 38}, "Купить", open && G.money >= ic(a.cost))) {
            G.money -= ic(a.cost);
            G.monExp += ic(a.cost);
            p.profit -= ic(a.cost);
            p.adDays = a.days;
            p.adBoost = a.boost;
            logMsg("Реклама «" + std::string(a.n) + "» для «" + p.name + "» запущена");
            toast("Реклама запущена");
        }
    }
}

void scrLaunch() {
    // Результаты запуска
    if (resIdx >= 0 && resIdx < (int)G.products.size()) {
        const Product& p = G.products[(size_t)resIdx];
        panel(140, 84, 1000, 620, "");
        drawTextC("Запуск: " + p.name, W / 2, 104, 3, C_ACC);
        drawDevice(330, 300, p.cat, p.p[P_SIZE], p.p[P_COLOR], p.p[P_SCREEN] + 1);
        double shown = std::min(p.rating, p.rating * resT / 1.5);
        drawText("Средняя оценка", 560, 170, 2, C_DIM);
        drawText(fmt1(shown), 560, 196, 8, ratingCol(shown));
        drawText("/10", 560 + textW(fmt1(shown), 8) + 12, 230, 3, C_DIM);
        if (resT > 1.5) {
            for (int i = 0; i < 4; ++i) {
                if (resT < 1.5 + i * 0.3) break;
                drawText(OUTLETS[i], 560, 280 + i * 32, 2, C_TEXT);
                drawText(fmt1(resOutlet[i]), 800, 280 + i * 32, 2, ratingCol(resOutlet[i]));
            }
            const char* rv;
            double r = p.rating;
            if (r >= 9) rv = "Шедевр! Лучшее устройство года.";
            else if (r >= 7.5) rv = "Отличный девайс, рекомендуем.";
            else if (r >= 6) rv = "Хорошо, но без сюрпризов.";
            else if (r >= 4.5) rv = "Средне. Есть варианты получше.";
            else if (r >= 3) rv = "Слабо. Слишком много компромиссов.";
            else rv = "Провал. Обходите стороной.";
            drawText(std::string("«") + rv + "»", 200, 470, 2, C_TEXT);
            drawText("Репутация: " + std::string(resRepDelta >= 0 ? "+" : "") + fmt1(resRepDelta), 200, 510, 2,
                     resRepDelta >= 0 ? C_GREEN : C_RED);
            drawText("Цена: " + fmtMoney(p.price) + "   Себестоимость: " + fmtMoney(p.unitCost), 200, 540, 2, C_DIM);
        }
        if (button({440, 620, 400, 60}, "Отлично!", true, true)) resIdx = -1;
        return;
    }
    // Подготовка к запуску
    int rs = readySlot();
    if (rs >= 0) {
        DevSlot& sl = G.slot[rs];
        Draft& dv = sl.d;
        panel(16, 76, 1248, 636, "");
        drawText(fitText("Готов к запуску: " + sl.name, 3, 720), 40, 92, 3, C_ACC);
        fillRect(40, 132, 420, 250, C_PANEL2);
        drawDevice(250, 257, dv.cat, dv.p[P_SIZE], dv.p[P_COLOR], dv.p[P_SCREEN] + 1);
        if (presChoice > 0 && G.money < presCost(presChoice)) presChoice = 0;
        Product tmp = makeProduct(sl, presChoice, qaChoice);
        double unit = unitProdCost(tmp);
        double est = estDemand(sl, presChoice, qaChoice);
        if (batchChoice < 0) {
            // по умолчанию — запас примерно на 10 дней, но не дороже 70% денег
            batchChoice = NBATCH - 1;
            for (int i = 0; i < NBATCH; ++i) if (BATCHES[i] >= est * 10.0) { batchChoice = i; break; }
            while (batchChoice > 0 && BATCHES[batchChoice] * unit > G.money * 0.7) --batchChoice;
        }
        double bn = BATCHES[batchChoice];
        double bcost = bn * unit * volDisc(bn);
        double total = presCost(presChoice) + qaCost(qaChoice) + bcost;
        drawText("Категория: " + std::string(CATS[dv.cat].name), 40, 392, 2, C_TEXT);
        drawText("Цена: " + fmtMoney(dv.price) + "  (выпуск " + fmtMoney(unit) + "/шт.)", 40, 418, 2, C_TEXT);
        drawText("Ожидаемая оценка: " + fmt1(tmp.rating), 40, 444, 2, ratingCol(tmp.rating));
        drawText("Ожидаемый брак: " + fmt1(tmp.defect * 100.0) + "%", 40, 470, 2,
                 tmp.defect > 0.07 ? C_RED : (tmp.defect > 0.04 ? C_YEL : C_GREEN));
        drawText("Спрос на старте: ≈ " + fmtNum(est) + " шт/день", 40, 496, 2, C_TEXT);
        int st = priceStep(dv.price);
        if (button({40, 528, 200, 54}, "- цена", dv.price > 10)) dv.price = std::max(10, dv.price - st);
        if (button({256, 528, 200, 54}, "+ цена")) dv.price += st;
        drawText("Стартовая партия", 40, 596, 2, C_DIM);
        if (button({40, 618, 56, 54}, "-", batchChoice > 0)) batchChoice--;
        drawTextC(fmtNum(bn) + " шт. = " + fmtMoney(bcost), 250, 626, 2, C_TEXT);
        drawTextC(bn <= 0 ? std::string("склад будет пуст")
                          : ("хватит на ≈ " + fmtNum(bn / std::max(1.0, est)) + " дн."), 250, 650, 2, C_DIM);
        if (button({404, 618, 56, 54}, "+", batchChoice < NBATCH - 1)) batchChoice++;

        drawText("Презентация", 500, 132, 2, C_DIM);
        for (int i = 0; i < 4; ++i) {
            SDL_Rect r = {500, 156 + i * 62, 740, 56};
            double c = presCost(i);
            bool can = c <= 0 || G.money >= c;
            if (button(r, "", can, presChoice == i)) presChoice = i;
            drawText(PRES[i].name, r.x + 16, r.y + 8, 2, can ? C_TEXT : C_DIM);
            drawText(i == 0 ? std::string("Бесплатно") : fmtMoney(c), r.x + 16, r.y + 32, 2, C_DIM);
            if (i > 0) drawTextR("+" + fmt1(PRES[i].bonus) + " к оценке", r.x + r.w - 16, r.y + 20, 2, C_GREEN);
        }
        drawText("Контроль качества", 500, 410, 2, C_DIM);
        for (int i = 0; i < 3; ++i) {
            SDL_Rect r = {500 + i * 250, 434, 240, 56};
            double c = qaCost(i);
            bool can = c <= 0 || G.money >= c;
            if (button(r, "", can, qaChoice == i)) qaChoice = i;
            drawText(fitText(QAS[i].n, 2, 224), r.x + 10, r.y + 8, 2, can ? C_TEXT : C_DIM);
            drawText(i == 0 ? std::string("Бесплатно") : fmtMoney(c), r.x + 10, r.y + 32, 2, C_DIM);
        }
        if (qaChoice > 0 && G.money < qaCost(qaChoice)) qaChoice = 0;
        bool canPay = total <= 0.5 || total <= G.money;  // бесплатный запуск возможен и при пустом счёте
        drawText("К оплате: " + fmtMoney(total), 500, 508, 3, canPay ? C_TEXT : C_RED);
        if (sl.crunch) drawText("Проект делали в аврале: брака больше.", 500, 544, 2, C_ORANGE);
        drawText("Брак ведёт к возвратам по гарантии и бьёт по репутации.", 500, 568, 2, C_DIM);
        if (button({500, 600, 740, 80}, canPay ? "Запустить продажи!" : "Не хватает денег", canPay, true)) {
            launchProduct(rs, batchChoice);
            batchChoice = -1;
        }
        return;
    }
    // Список продуктов
    if (G.products.empty()) {
        panel(16, 76, 1248, 636, "");
        drawTextC("Пока нет выпущенных устройств.", W / 2, 300, 3, C_TEXT);
        drawTextC(anyDev() ? "Дождитесь окончания разработки." : "Создайте первое устройство в конструкторе.", W / 2,
                  350, 2, C_DIM);
        if (!anyDev() && button({440, 420, 400, 64}, "В конструктор", true, true)) setScreen(SC_DESIGN);
        return;
    }
    if (selProduct < 0 || selProduct >= (int)G.products.size()) selProduct = (int)G.products.size() - 1;
    drawText("Устройства (" + std::to_string(G.products.size()) + ")", 24, 84, 2, C_ACC);
    SDL_Rect view = {16, 110, 420, 602};
    beginArea(1, view);
    int sy = scr(1);
    int n = (int)G.products.size();
    for (int j = 0; j < n; ++j) {
        int i = n - 1 - j;
        const Product& p = G.products[(size_t)i];
        SDL_Rect r = {view.x, view.y + j * 70 - sy, view.w - 10, 62};
        if (widget(r, true)) { selProduct = i; sfx(SFX_CLICK); }
        fillRect(r, i == selProduct ? C_BTNSEL : (hovered(r) ? C_BTNH : C_PANEL));
        drawRect(r, C_BORDER);
        drawText(fitText(p.name, 2, 300), r.x + 10, r.y + 10, 2, C_TEXT);
        if (!p.onSale) drawText("Снят", r.x + 10, r.y + 36, 2, C_DIM);
        else if (p.stock < 1) drawText("Нет на складе", r.x + 10, r.y + 36, 2, C_RED);
        else drawText("Склад: " + fmtShort(p.stock), r.x + 10, r.y + 36, 2, C_GREEN);
        drawTextR(fmt1(p.rating), r.x + r.w - 10, r.y + 20, 3, ratingCol(p.rating));
        drawFocus(r);
    }
    endArea(n * 70);

    Product& p = G.products[(size_t)selProduct];
    panel(452, 76, 812, 636, "");
    drawText(fitText(p.name, 3, 640), 472, 92, 3, C_ACC);
    drawText(std::string(CATS[p.cat].name) + " / " + PRES[p.pres].name +
             (p.adDays > 0 ? "   Реклама: " + std::to_string(p.adDays) + " дн." : "") +
             (p.fw > 0 ? "   Обновлений: " + std::to_string(p.fw) : ""), 472, 124, 2, C_DIM);
    drawTextR(fmt1(p.rating), 1244, 88, 5, ratingCol(p.rating));
    int y = 156;
    drawText("Цена: " + fmtMoney(p.price), 472, y + 18, 2, C_TEXT);
    if (p.onSale) {
        int st = priceStep(p.price);
        if (button({760, y, 120, 52}, "- $" + std::to_string(st), p.price > 10)) p.price = std::max(10, p.price - st);
        if (button({890, y, 120, 52}, "+ $" + std::to_string(st))) p.price += st;
    }
    drawText("реком. " + fmtMoney(fairNow(p)), 1030, y + 18, 2, C_DIM);
    y += 66;
    double unit = unitProdCost(p);
    drawText("Выпуск: " + fmtMoney(unit) + "/шт.", 472, y, 2, C_TEXT);
    drawText("Продажи вчера: " + fmtNum(p.lastDay) + " шт.", 860, y, 2, C_TEXT);
    y += 28;
    drawText("Продано всего: " + fmtNum(p.units) + " шт.", 472, y, 2, C_TEXT);
    drawText("Выручка: " + fmtMoney(p.revenue), 860, y, 2, C_TEXT);
    y += 28;
    drawText("Прибыль: " + fmtMoney(p.profit), 472, y, 2, p.profit >= 0 ? C_GREEN : C_RED);
    drawText("В продаже: " + std::to_string(G.day - p.launchDay) + " дн.", 860, y, 2, C_TEXT);
    y += 28;
    drawText("Брак: " + fmt1(p.defect * 100.0) + "%  (возвратов " + fmtNum(p.returns) + ")", 472, y, 2,
             p.defect > 0.07 ? C_RED : (p.defect > 0.04 ? C_YEL : C_GREEN));
    if (p.onSale)
        drawText("Конкуренция: спрос x" + fmt1(p.compF), 860, y, 2, p.compF < 0.8 ? C_RED : (p.compF > 1.05 ? C_GREEN : C_TEXT));
    y += 28;
    if (p.onSale) {
        std::string sg = "Покупатели: ";
        for (int s = 0; s < 3; ++s)
            sg += std::string(SEG_NAME[s]) + " " + std::to_string((int)std::round(p.seg[s] * 100)) + "%" + (s < 2 ? ", " : "");
        drawText(sg, 472, y, 2, C_DIM);
        drawTextR("пошлины " + fmt1(p.tariffNow * 100.0) + "%", 1244, y, 2, C_DIM);
    }
    // Склад и заказ партий
    if (p.onSale) {
        drawText("Склад: " + fmtNum(p.stock) + " шт.", 472, 366, 2, p.stock < 1 ? C_RED : C_TEXT);
        if (p.lost > 0) drawText("Дефицит: -" + fmtNum(p.lost) + " шт.", 472, 390, 2, C_RED);
        else drawText("В работе: " + fmtNum(p.queue), 472, 390, 2, C_DIM);
        if (button({700, 362, 130, 52}, p.autoProd ? "Авто: вкл" : "Авто: выкл", true, p.autoProd)) p.autoProd = !p.autoProd;
        orderIdx = clampi(orderIdx, 1, NBATCH - 1);
        double on = BATCHES[orderIdx];
        double oc = on * unit * volDisc(on);
        if (button({840, 362, 52, 52}, "-", orderIdx > 1)) orderIdx--;
        drawTextC(fmtNum(on) + " шт.", 950, 366, 2, C_TEXT);
        drawTextC(fmtMoney(oc), 950, 390, 2, oc <= G.money ? C_DIM : C_RED);
        if (button({1008, 362, 52, 52}, "+", orderIdx < NBATCH - 1)) orderIdx++;
        if (button({1070, 362, 178, 52}, "Заказать", oc <= G.money)) {
            if (orderBatch(p, on, false)) {
                logMsg("Заказана партия «" + p.name + "»: " + fmtNum(on) + " шт. за " + fmtMoney(oc));
                toast("Партия заказана");
                sfx(SFX_CASH);
            }
        }
    } else {
        drawText("Продажи завершены", 472, 380, 2, C_DIM);
    }
    salesChart({468, 424, 780, 196}, p);
    if (button({468, 630, 189, 60}, "Снять с продаж", p.onSale)) discontinue(p, "снят с продаж");
    if (button({665, 630, 189, 60}, "Реклама", p.onSale)) {
        retScreen = SC_LAUNCH;
        setScreen(SC_ADS);
    }
    {
        double uc = updateCost(p);
        std::string lbl = p.fw >= FW_MAX ? std::string("Обновлён") : "Обновить " + fmtShort(uc);
        if (button({862, 630, 189, 60}, lbl, canUpdate(p) && G.money >= uc)) releaseUpdate(p);
    }
    if (button({1059, 630, 189, 60}, "Новое", freeSlot() >= 0)) setScreen(SC_DESIGN);
}

// ----- Банк, налоги, инвесторы -----
void scrBank() {
    panel(16, 76, 610, 636, "Банк и налоги");
    double rate = loanRate();
    drawText("Долг: " + fmtMoney(G.loan), 36, 112, 3, G.loan > 0 ? C_ORANGE : C_GREEN);
    drawText("Ставка: " + fmt1(rate * 100.0) + "% годовых  (" + fmtMoney(G.loan * rate / 12.0) + "/мес)", 36, 152, 2, C_TEXT);
    double lim = loanLimit();
    drawText("Лимит кредита: " + fmtMoney(lim), 36, 178, 2, C_TEXT);
    // Суммы растут вместе с компанией: 5%, 20% и 50% лимита, но не меньше базовых
    const double chunk[3] = {std::max(ic(10000), std::floor(lim * 0.05 / 1000.0) * 1000.0),
                             std::max(ic(50000), std::floor(lim * 0.20 / 1000.0) * 1000.0),
                             std::max(ic(250000), std::floor(lim * 0.50 / 1000.0) * 1000.0)};
    for (int i = 0; i < 3; ++i) {
        if (button({36 + i * 192, 214, 184, 56}, "Взять " + fmtShort(chunk[i]), G.loan + chunk[i] <= lim + 0.5)) {
            G.loan += chunk[i];
            G.money += chunk[i];
            logMsg("Взят кредит: " + fmtMoney(chunk[i]));
            sfx(SFX_CASH);
        }
        double pay = std::min(chunk[i], G.loan);
        if (button({36 + i * 192, 280, 184, 56}, "Вернуть " + fmtShort(chunk[i]), G.loan > 0 && G.money >= pay)) {
            G.loan -= pay;
            G.money -= pay;
            if (G.loan < 1) G.loan = 0;
            logMsg("Погашено по кредиту: " + fmtMoney(pay));
            sfx(SFX_CLICK);
        }
    }
    drawText("Проценты списываются каждый месяц.", 36, 350, 2, C_DIM);
    drawText("В кризис и при низкой репутации ставка выше.", 36, 374, 2, C_DIM);
    double profit = annualSum(true);
    drawText("Налог на прибыль", 36, 420, 2, C_ACC);
    drawText("Ставка " + pct(TAX_RATE) + ", платится в начале года.", 36, 448, 2, C_DIM);
    drawText("Прибыль за 12 мес: " + fmtMoney(profit), 36, 476, 2, profit >= 0 ? C_GREEN : C_RED);
    drawText("Ожидаемый налог: " + fmtMoney(std::max(0.0, profit) * TAX_RATE), 36, 502, 2, C_TEXT);
    drawText("Уплачено в прошлом году: " + fmtMoney(G.lastTax), 36, 528, 2, C_DIM);
    drawText("Инфляция", 36, 574, 2, C_ACC);
    drawText("За прошлый год: " + fmt1(G.lastInfl * 100.0) + "%", 36, 602, 2, C_TEXT);
    drawText("Цены к 1960 году: x" + fmt1(G.infl), 36, 628, 2, C_TEXT);
    drawText("Растут зарплаты, аренда и комплектующие.", 36, 654, 2, C_DIM);

    panel(642, 76, 622, 636, "Инвесторы");
    double v = valuation();
    drawText("Ваша доля: " + pct(G.stake), 662, 112, 3, C_TEXT);
    progressBar({662, 150, 582, 16}, G.stake, C_ACC);
    drawText("Оценка компании: " + fmtMoney(v), 662, 180, 2, C_TEXT);
    drawText("Дивиденды: " + fmt1((1.0 - G.stake) * 50.0) + "% чистой прибыли в год", 662, 206, 2, C_DIM);
    if (button({662, 244, 582, 60}, "Продать 10% за " + fmtMoney(v * 0.10), G.stake >= 0.61 - 1e-9)) {
        G.stake -= 0.10;
        G.money += v * 0.10;
        logMsg("Инвесторы купили 10% компании за " + fmtMoney(v * 0.10));
        sfx(SFX_CASH);
    }
    if (button({662, 314, 582, 60}, "Выкупить 10% за " + fmtMoney(v * 0.12), G.stake <= 0.90 + 1e-9 && G.money >= v * 0.12)) {
        G.stake += 0.10;
        G.money -= v * 0.12;
        logMsg("Выкуплено 10% компании за " + fmtMoney(v * 0.12));
        sfx(SFX_CLICK);
    }
    bool ipoOk = !G.ipo && calYear() >= 1971 && G.rep >= 50 && G.stake >= 0.71 - 1e-9;
    if (button({662, 384, 582, 60}, G.ipo ? std::string("Компания уже на бирже")
                                          : "Выход на биржу: 20% за " + fmtMoney(v * 0.30), ipoOk, ipoOk)) {
        G.ipo = true;
        G.stake -= 0.20;
        G.money += v * 0.30;
        G.rep = clampd(G.rep + 5, 0, 100);
        logMsg("Компания вышла на биржу: привлечено " + fmtMoney(v * 0.30));
        toast("Акции размещены на бирже!");
        sfx(SFX_LAUNCH);
    }
    if (!G.ipo) drawText("Нужно: 1971 год, репутация от 50, доля от 71%.", 662, 456, 2, C_DIM);
    else drawText("Цена акции: $" + fmt1(v / 1000000.0), 662, 456, 2, C_GREEN);
    drawText("Деньги инвесторов не нужно возвращать,", 662, 510, 2, C_DIM);
    drawText("но они получают часть прибыли каждый год.", 662, 536, 2, C_DIM);
    drawText("Продать можно не больше 49% компании.", 662, 562, 2, C_DIM);
    if (G.lastDiv > 0) drawText("Дивиденды за прошлый год: " + fmtMoney(G.lastDiv), 662, 610, 2, C_TEXT);
}

// ----- Поставщики комплектующих -----
void scrSupply() {
    panel(16, 76, 1248, 636, "Поставщики комплектующих");
    bool locked = G.supplier != 0 && G.day < G.supUntil;
    if (G.supHalt > 0)
        drawTextR("Завод стоит: нет комплектующих (" + std::to_string(G.supHalt) + " дн.)", 1244, 86, 2, C_RED);
    else if (locked)
        drawTextR("Контракт действует ещё " + std::to_string(G.supUntil - G.day) + " дн.", 1244, 86, 2, C_YEL);
    for (int i = 0; i < NSUP; ++i) {
        const Sup& s = SUPS[i];
        int y = 120 + i * 118;
        bool cur = G.supplier == i;
        fillRound({32, y, 1216, 108}, 10, cur ? shade(C_BTNSEL, 0.45) : C_PANEL2);
        drawText(s.n, 52, y + 12, 3, cur ? C_YEL : C_TEXT);
        drawText(s.d, 52, y + 46, 2, C_DIM);
        std::string st = "Себестоимость " + pct(s.cm) + "   Брак " + (s.defAdd >= 0 ? "+" : "") + fmt1(s.defAdd * 100.0) +
                         " п.п.   Плата " + fmtMoney(supFee(i)) + "/мес   Срывы " + fmt1(s.fail * 100.0) + "%/мес";
        drawText(fitText(st, 2, 960), 52, y + 76, 2, C_TEXT);
        bool can = !cur && !locked;
        if (button({1040, y + 26, 196, 56}, cur ? "Текущий" : (i == 0 ? "Выбрать" : "Контракт на год"), can)) {
            G.supplier = i;
            G.supUntil = i == 0 ? 0 : G.day + 360;
            logMsg(std::string("Снабжение: ") + s.n);
            toast("Поставщик выбран");
            sfx(SFX_DONE);
        }
    }
    drawText("Скидка за объём партии: от 5 000 шт. -3%, от 20 000 шт. -6%.", 52, 604, 2, C_DIM);
    drawText("Брак поставщика влияет на устройства, запущенные при этом контракте.", 52, 630, 2, C_DIM);
    drawText("Кризисы поставок останавливают завод, если нет надёжного партнёра.", 52, 656, 2, C_DIM);
}

void scrOffice() {
    panel(16, 74, 600, 250, "Офис");
    drawText(OFFICE_NAME[G.office], 32, 106, 3, C_TEXT);
    drawText("Сотрудники: " + std::to_string(G.staff.size()) + "/" + std::to_string(OFFICE_CAP[G.office]), 32, 146, 2,
             C_TEXT);
    drawText("Бонус скорости: +" + std::to_string((int)std::round((OFFICE_MULT[G.office] - 1) * 100)) + "%", 32, 172,
             2, C_TEXT);
    drawText("Аренда: " + fmtMoney(officeRent()) + "/мес", 32, 198, 2, C_TEXT);
    drawText("Проектов сразу: " + std::to_string(maxSlots()), 32, 224, 2, C_TEXT);
    // Здание
    int bh = 40 + G.office * 22, bx = 470, by = 230 - bh;
    fillRect(bx, by, 120, bh, C_BTN);
    for (int fl = 0; fl < bh / 22; ++fl)
        for (int wx = 0; wx < 4; ++wx) fillRect(bx + 10 + wx * 27, by + 8 + fl * 22, 18, 12, C_YEL);
    if (G.office < NOFFICE - 1) {
        double c = ic(OFFICE_COST[G.office + 1]);
        if (button({32, 254, 568, 56}, "Переезд: " + std::string(OFFICE_NAME[G.office + 1]) + " за " + fmtMoney(c),
                   G.money >= c)) {
            G.money -= c;
            G.monExp += c;
            G.office++;
            logMsg(std::string("Компания переехала: ") + OFFICE_NAME[G.office]);
            sfx(SFX_DONE);
        }
    } else {
        drawText("Максимальный уровень офиса", 32, 274, 2, C_GREEN);
    }
    panel(16, 334, 600, 120, "Показатели");
    drawText("Очки исследований: " + fmt1(researchRate()) + "/день", 32, 366, 2, C_TEXT);
    drawText("Скорость разработки: x" + fmt1(devSpeed()), 32, 392, 2, C_TEXT);
    drawText("Зарплаты: " + fmtMoney(salaries()) + "/мес", 32, 418, 2, C_TEXT);
    // Аврал: быстрее разработка, но падает настроение и растёт брак
    if (button({396, 344, 208, 48}, G.crunch ? "Аврал: вкл" : "Аврал: выкл", true, G.crunch)) {
        G.crunch = !G.crunch;
        toast(G.crunch ? "Аврал: +35% скорости, люди устают, брака больше" : "Аврал отменён");
    }
    {
        double pc = ic(600) * (double)G.staff.size();
        if (button({396, 398, 208, 48}, "Корпоратив " + fmtShort(pc), !G.staff.empty() && G.money >= pc)) {
            G.money -= pc;
            G.monExp += pc;
            for (size_t i = 0; i < G.staff.size(); ++i) G.staff[i].mood = std::min(100, G.staff[i].mood + 15);
            logMsg("Корпоратив: настроение команды выросло");
            toast("Настроение команды выросло");
        }
    }

    panel(16, 464, 600, 248, "Кандидаты");
    if (button({400, 470, 200, 48}, "Новые (" + fmtShort(ic(800)) + ")", G.money >= ic(800))) {
        G.money -= ic(800);
        G.monExp += ic(800);
        refreshCandidates();
    }
    bool full = (int)G.staff.size() >= OFFICE_CAP[G.office];
    for (size_t i = 0; i < G.cand.size(); ++i) {
        const Staff& s = G.cand[i];
        int y = 526 + (int)i * 62;
        drawText(s.name + ", " + ROLE_NAME[s.role], 32, y + 4, 2, C_TEXT);
        drawText("Навык " + std::to_string(s.skill) + "/10   " + fmtMoney(s.salary) + "/мес", 32, y + 28, 2, C_DIM);
        if (button({430, y, 170, 52}, full ? "Нет мест" : "Нанять", !full && G.money >= s.salary)) {
            G.money -= s.salary;
            G.monExp += s.salary;
            G.staff.push_back(s);
            logMsg("Нанят: " + s.name + " (" + ROLE_NAME[s.role] + ")");
            G.cand.erase(G.cand.begin() + (long)i);
            break;
        }
    }
    if (G.cand.empty()) drawText("Кандидатов нет. Ждите начала месяца.", 32, 560, 2, C_DIM);

    drawText("Штат (" + std::to_string(G.staff.size()) + ")", 640, 84, 2, C_ACC);
    drawText("Навык растёт с опытом. Обучение ускоряет рост. Премия = 1 оклад.", 800, 90, 1, C_DIM);
    SDL_Rect view = {632, 110, 632, 602};
    beginArea(2, view);
    int sy = scr(2);
    if (G.staff.empty()) drawText("Пока вы работаете в одиночку.", 648, 130 - sy, 2, C_DIM);
    for (size_t i = 0; i < G.staff.size(); ++i) {
        Staff& s = G.staff[i];
        SDL_Rect r = {view.x, view.y + (int)i * 66 - sy, view.w - 10, 60};
        fillRect(r, C_PANEL);
        drawRect(r, C_BORDER);
        drawText(fitText(s.name, 2, 130), r.x + 12, r.y + 8, 2, C_TEXT);
        drawText(ROLE_NAME[s.role], r.x + 12, r.y + 34, 2, s.role == 0 ? C_ORANGE : (s.role == 1 ? C_ACC : C_GREEN));
        for (int k = 0; k < 10; ++k) fillRect(r.x + 150 + k * 12, r.y + 8, 9, 12, k < s.skill ? C_YEL : C_PANEL2);
        if (s.ask > 0) drawText("просит " + fmtShort(s.ask), r.x + 150, r.y + 26, 2, C_ORANGE);
        else drawText(fmtMoney(s.salary) + "/мес", r.x + 150, r.y + 26, 2, C_DIM);
        progressBar({r.x + 150, r.y + 46, 120, 8}, s.mood / 100.0, s.mood < 40 ? C_RED : C_GREEN);
        int tc = (int)ic(2000 + 1500 * s.skill);
        if (button({r.x + r.w - 322, r.y + 6, 100, 48}, "Обучить", s.skill < 10 && G.money >= tc)) {
            G.money -= tc;
            G.monExp += tc;
            s.skill++;
            s.salary = ((int)(s.salary * 1.1) / 50) * 50;
            s.mood = std::min(100, s.mood + 10);
            logMsg(s.name + " прошёл обучение: навык " + std::to_string(s.skill));
            toast("Навык вырос");
        }
        if (s.ask > 0) {
            // Просьба о прибавке: согласиться — оклад растёт, настроение тоже
            if (button({r.x + r.w - 216, r.y + 6, 100, 48}, "Оклад +", true, true)) {
                logMsg(s.name + ": оклад повышен до " + fmtMoney(s.ask));
                s.salary = s.ask;
                s.ask = 0;
                s.mood = std::min(100, s.mood + 20);
            }
        } else if (button({r.x + r.w - 216, r.y + 6, 100, 48}, "Премия", s.mood < 100 && G.money >= s.salary)) {
            G.money -= s.salary;
            G.monExp += s.salary;
            s.mood = std::min(100, s.mood + 30);
        }
        if (button({r.x + r.w - 110, r.y + 6, 100, 48}, "Уволить")) {
            logMsg("Уволен: " + s.name);
            G.staff.erase(G.staff.begin() + (long)i);
            break;
        }
    }
    endArea((int)G.staff.size() * 66);
}

void scrResearch() {
    panel(16, 74, 1248, 82, "");
    double rate = researchRate();
    drawText("Очки исследований: " + fmt1(rate) + " в день", 32, 86, 2, C_ACC);
    if (G.researching >= 0) {
        const Tech& t = TECHS[(size_t)G.researching];
        drawText(fitText("Изучается: " + t.name, 2, 700), 32, 116, 2, C_TEXT);
        progressBar({760, 112, 480, 22}, G.researchProg / t.cost, C_GREEN);
    } else {
        drawText("Выберите технологию для изучения", 32, 116, 2, C_DIM);
    }
    SDL_Rect view = {16, 166, 1248, 546};
    beginArea(3, view);
    int sy = scr(3);
    for (size_t i = 0; i < TECHS.size(); ++i) {
        const Tech& t = TECHS[i];
        SDL_Rect r = {view.x, view.y + (int)i * 66 - sy, view.w - 10, 60};
        bool done = techDone((int)i);
        bool rel = calYear() >= t.year;
        bool avail = !done && rel && (t.prereq < 0 || techDone(t.prereq));
        bool cur = G.researching == (int)i;
        fillRect(r, cur ? shade(C_BTNSEL, 0.6) : C_PANEL);
        drawRect(r, C_BORDER);
        drawText(t.name, r.x + 12, r.y + 8, 2, done ? C_GREEN : (avail ? C_TEXT : C_DIM));
        const char* desc = t.kind == 0 ? "Новый вариант компонента"
                         : t.kind == 1 ? "Открывает новую категорию устройств"
                         : t.kind == 2 ? "-8% к себестоимости производства"
                         : t.kind == 3 ? "+15% к скорости разработки"
                                       : "+8% к скорости разработки (нейросети)";
        std::string descS = desc;
        if (!rel) descS = "Появится в " + std::to_string(t.year) + " г.";
        drawText(descS, r.x + 12, r.y + 34, 2, C_DIM);
        if (!done) {
            double left = std::max(0.0, t.cost - (cur ? G.researchProg : 0.0));
            drawText(fmtNum(t.cost) + " очк.", r.x + 600, r.y + 8, 2, C_TEXT);
            drawText("≈ " + std::to_string((int)std::ceil(left / rate)) + " дн.", r.x + 600, r.y + 34, 2, C_DIM);
        }
        std::string lbl;
        bool en;
        if (done) { lbl = "Изучено"; en = false; }
        else if (cur) { lbl = "Отменить"; en = true; }
        else if (!rel) { lbl = "Скоро"; en = false; }
        else if (!avail) { lbl = "Нужна пред."; en = false; }
        else if (G.researching >= 0) { lbl = "Занято"; en = false; }
        else { lbl = "Изучить"; en = true; }
        if (cur) progressBar({r.x + 800, r.y + 22, 200, 16}, G.researchProg / t.cost, C_GREEN);
        if (button({r.x + r.w - 220, r.y + 6, 210, 48}, lbl, en, !done && en && !cur)) {
            if (cur) { G.researching = -1; G.researchProg = 0; }
            else { G.researching = (int)i; G.researchProg = 0; }
        }
    }
    endArea((int)TECHS.size() * 66);
}

void scrMarket() {
    drawText("Рынки: объём x" + fmt1(marketSize()), 24, 84, 2, C_ACC);
    SDL_Rect view = {16, 110, 700, 602};
    beginArea(4, view);
    int sy = scr(4);
    for (int i = 0; i < NMARKET; ++i) {
        const Market& m = MARKETS[i];
        SDL_Rect r = {view.x, view.y + i * 66 - sy, view.w - 10, 60};
        fillRect(r, marketOn(i) ? shade(C_GREEN, 0.3) : C_PANEL);
        drawRect(r, C_BORDER);
        bool avail = marketAvail(i);
        std::string mn = marketName(i);
        drawText(mn, r.x + 12, r.y + 8, 2, avail ? C_TEXT : C_DIM);
        int best = 0;
        for (int c = 1; c < NCAT; ++c) if (m.pref[c] > m.pref[best]) best = c;
        drawText(fitText(std::string("любят: ") + CATS[best].name, 2, 300), r.x + 28 + textW(mn, 2), r.y + 8, 2, C_DIM);
        drawText("x" + fmt1(marketBaseSize(i)) + ", достаток " + pct(m.wealth) + ", пошлина " + pct(m.tariff),
                 r.x + 12, r.y + 34, 2, C_DIM);
        if (marketOn(i)) {
            drawTextR("Открыт", r.x + r.w - 20, r.y + 22, 2, C_GREEN);
        } else if (!avail) {
            drawTextR(i == 1 || i == 2 ? "В составе СССР до 1992" : "Закрыт до " + std::to_string(m.year) + " г.",
                      r.x + r.w - 20, r.y + 22, 2, C_DIM);
        } else {
            bool repOk = G.rep >= m.rep;
            double mc = ic(m.cost);
            bool can = repOk && G.money >= mc;
            if (button({r.x + r.w - 230, r.y + 6, 220, 48},
                       repOk ? "Открыть " + fmtShort(mc) : "Репутация " + std::to_string((int)m.rep), can)) {
                G.money -= mc;
                G.monExp += mc;
                G.market[i] = true;
                logMsg(std::string("Открыт рынок: ") + m.name);
                sfx(SFX_CASH);
            }
        }
    }
    endArea(NMARKET * 66);

    panel(732, 74, 532, 638, "Маркетинг");
    drawText("Ажиотаж", 748, 110, 2, C_TEXT);
    progressBar({860, 108, 300, 18}, G.hype / 4.0, C_ORANGE);
    drawText("Спрос: x" + fmt1(1.0 + 0.5 * G.hype), 748, 138, 2, C_DIM);
    double k = std::sqrt(marketSize());
    for (int i = 0; i < 4; ++i) {
        SDL_Rect r = {748, 172 + i * 80, 500, 68};
        double c = ic(CAMPS[i].cost * k);
        bool can = G.money >= c && G.hype < 3.9;
        if (button(r, "", can)) {
            G.money -= c;
            G.monExp += c;
            G.hype = std::min(4.0, G.hype + CAMPS[i].hype);
            logMsg(std::string("Кампания: ") + CAMPS[i].name);
            sfx(SFX_CASH);
        }
        drawText(CAMPS[i].name, r.x + 16, r.y + 12, 2, can ? C_TEXT : C_DIM);
        drawText(fmtMoney(c), r.x + 16, r.y + 40, 2, C_DIM);
        drawTextR("+" + fmt1(CAMPS[i].hype) + " ажиотажа", r.x + r.w - 16, r.y + 26, 2, C_GREEN);
    }
    drawText("Модный цвет года:", 748, 506, 2, C_TEXT);
    fillRect(980, 500, 40, 26, COLOR_RGB[G.trend]);
    drawText(COLOR_OPT[G.trend].name, 1030, 506, 2, C_YEL);
    drawText("Ажиотаж затухает на 2% в день.", 748, 548, 2, C_DIM);
    drawText("Дорогие товары хуже идут в бедных", 748, 574, 2, C_DIM);
    drawText("странах. Пошлина берётся с выручки.", 748, 600, 2, C_DIM);
    drawText("Фанаты: " + fmtNum(G.fans), 748, 640, 2, C_TEXT);
}

void financeCharts() {
    std::vector<double> inc, exp;
    int n = (int)G.histInc.size();
    int first = std::max(0, n - 11);
    for (int i = first; i < n; ++i) { inc.push_back(G.histInc[(size_t)i]); exp.push_back(G.histExp[(size_t)i]); }
    inc.push_back(G.monInc);
    exp.push_back(G.monExp);
    int m = (int)inc.size();
    int curMonth = G.day / 30;
    double mx = 1000;
    for (int i = 0; i < m; ++i) mx = std::max(mx, std::max(inc[(size_t)i], exp[(size_t)i]));
    SDL_Rect r = {16, 134, 1248, 330};
    fillRect(r, C_PANEL);
    drawRect(r, C_BORDER);
    drawText("Доходы и расходы по месяцам", 32, 144, 2, C_ACC);
    fillRect(820, 146, 14, 14, C_GREEN); drawText("Доходы", 842, 144, 2, C_TEXT);
    fillRect(960, 146, 14, 14, C_RED); drawText("Расходы", 982, 144, 2, C_TEXT);
    int left = r.x + 90, base = r.y + r.h - 34, top = r.y + 44;
    for (int g = 0; g <= 4; ++g) {
        int yy = base - (base - top) * g / 4;
        fillRect(left, yy, r.w - 110, 1, C_BORDER);
        drawTextR(fmtShort(mx * g / 4), left - 8, yy - 6, 2, C_DIM);
    }
    int gw = (r.w - 110) / 12;
    for (int i = 0; i < m; ++i) {
        int gx = left + i * gw + 8;
        int bw = (gw - 20) / 2;
        int hi = (int)(inc[(size_t)i] / mx * (base - top));
        int he = (int)(exp[(size_t)i] / mx * (base - top));
        bool cur = i == m - 1;
        fillRect(gx, base - hi, bw, hi, cur ? shade(C_GREEN, 0.6) : C_GREEN);
        fillRect(gx + bw + 2, base - he, bw, he, cur ? shade(C_RED, 0.6) : C_RED);
        int mo = curMonth - (m - 1 - i);
        drawText(MONTHS_S[((mo % 12) + 12) % 12], gx, base + 10, 2, cur ? C_YEL : C_DIM);
    }
    // Линия прибыли
    SDL_Rect r2 = {16, 474, 1248, 160};
    fillRect(r2, C_PANEL);
    drawRect(r2, C_BORDER);
    drawText("Прибыль", 32, 484, 2, C_ACC);
    double pm = 1000;
    for (int i = 0; i < m; ++i) pm = std::max(pm, std::fabs(inc[(size_t)i] - exp[(size_t)i]));
    int zy = r2.y + r2.h / 2 + 8, amp = r2.h / 2 - 22;
    fillRect(left, zy, r2.w - 110, 1, C_DIM);
    drawTextR("0", left - 8, zy - 6, 2, C_DIM);
    drawTextR(fmtShort(pm), left - 8, zy - amp - 6, 2, C_DIM);
    int px = -1, py = -1;
    for (int i = 0; i < m; ++i) {
        double pr = inc[(size_t)i] - exp[(size_t)i];
        int x = left + i * gw + gw / 2;
        int y = zy - (int)(pr / pm * amp);
        if (px >= 0) {
            setc(C_YEL);
            SDL_RenderDrawLine(R, px, py, x, y);
            SDL_RenderDrawLine(R, px, py + 1, x, y + 1);
        }
        fillCircle(x, y, 4, pr >= 0 ? C_GREEN : C_RED);
        px = x; py = y;
    }
    double tInc = G.monInc, tExp = G.monExp;
    for (int i = first; i < n; ++i) { tInc += G.histInc[(size_t)i]; tExp += G.histExp[(size_t)i]; }
    drawText("За 12 мес: доходы " + fmtMoney(tInc) + ",  расходы " + fmtMoney(tExp) + ",  итог " +
                 fmtMoney(tInc - tExp), 24, 650, 2, tInc >= tExp ? C_GREEN : C_RED);
    drawText("Зарплаты " + fmtMoney(salaries()) + "/мес,  аренда " + fmtMoney(officeRent()) + "/мес,  цены к 1960 г.: x" +
                 fmt1(G.infl), 24, 680, 2, C_DIM);
}

void scrStats() {
    const char* tabs[4] = {"Финансы", "Рейтинги рынка", "Устройства", "Конкуренты"};
    for (int i = 0; i < 4; ++i)
        if (button({16 + i * 250, 72, 240, 52}, tabs[i], true, statsTab == i) && statsTab != i) {
            statsTab = i;
            U.scroll[5] = 0;
        }
    if (statsTab == 0) { financeCharts(); return; }
    if (statsTab == 1) {
        drawText("Доля рынка (по выручке за месяц). Планка качества сейчас: " + fmt1(expectation()), 24, 138, 2, C_ACC);
        double last = G.histInc.empty() ? 0 : G.histInc.back();
        double cur = G.monInc * 30.0 / ((G.day % 30) + 1);
        double pl = std::max(last, cur) / 1000.0 / G.infl;
        struct Row { std::string n; double s; bool me; };
        std::vector<Row> rows;
        rows.push_back({G.company, pl, true});
        for (int i = 0; i < NCOMP; ++i) rows.push_back({COMP_NAMES[i], G.comp[i], false});
        double tot = 0;
        for (size_t i = 0; i < rows.size(); ++i) tot += rows[i].s;
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.s > b.s; });
        SDL_Rect view = {16, 166, 1248, 546};
        beginArea(7, view);
        int sy = scr(7);
        for (size_t i = 0; i < rows.size(); ++i) {
            int y = view.y + (int)i * 44 - sy;
            double sh = tot > 0 ? rows[i].s / tot : 0;
            fillRect(view.x, y, view.w - 10, 40, C_PANEL);
            drawText(std::to_string(i + 1) + ". " + fitText(rows[i].n, 2, 300), 40, y + 13, 2,
                     rows[i].me ? C_YEL : C_TEXT);
            progressBar({400, y + 8, 700, 24}, sh / std::max(0.01, rows[0].s / std::max(1e-9, tot)), rows[i].me ? C_YEL : C_ACC);
            drawText(fmt1(sh * 100) + "%", 1120, y + 13, 2, C_TEXT);
        }
        endArea((int)rows.size() * 44);
        return;
    }
    if (statsTab == 3) {
        // Модели конкурентов, которые сейчас на рынке
        drawText("Модели конкурентов на рынке: " + std::to_string(G.rivals.size()) +
                     ". Сильные и дешёвые соперники отнимают ваш спрос.", 24, 138, 2, C_ACC);
        SDL_Rect view = {16, 166, 1248, 546};
        beginArea(6, view);
        int sy = scr(6);
        int n = (int)G.rivals.size();
        if (n == 0) drawText("Конкуренты пока ничего не выпустили.", 32, 180 - sy, 2, C_DIM);
        for (int j = 0; j < n; ++j) {
            const RProd& r = G.rivals[(size_t)(n - 1 - j)];  // новые сверху
            int y = view.y + j * 48 - sy;
            int age = G.day - r.launchDay;
            fillRect(view.x, y, view.w - 10, 44, C_PANEL);
            drawText(fitText(rprodName(r), 2, 420), 32, y + 15, 2, age <= 240 ? C_TEXT : C_DIM);
            drawText(CATS[r.cat].name, 480, y + 15, 2, C_DIM);
            drawText(fmt1(r.rating), 720, y + 11, 3, ratingCol(r.rating));
            drawText("$" + fmtNum(r.price), 820, y + 15, 2, C_TEXT);
            drawTextR(age <= 240 ? std::to_string(age) + " дн. на рынке" : std::string("уходит с рынка"), 1236, y + 15, 2, C_DIM);
        }
        endArea(n * 48);
        return;
    }
    // Устройства по рейтингу
    std::vector<int> idx;
    for (size_t i = 0; i < G.products.size(); ++i) idx.push_back((int)i);
    std::sort(idx.begin(), idx.end(),
              [](int a, int b) { return G.products[(size_t)a].rating > G.products[(size_t)b].rating; });
    SDL_Rect view = {16, 134, 1248, 578};
    beginArea(5, view);
    int sy = scr(5);
    if (idx.empty()) drawText("Выпущенных устройств пока нет.", 32, 160 - sy, 2, C_DIM);
    for (size_t k = 0; k < idx.size(); ++k) {
        const Product& p = G.products[(size_t)idx[k]];
        SDL_Rect r = {view.x, view.y + (int)k * 64 - sy, view.w - 10, 58};
        if (widget(r, true)) { selProduct = idx[k]; setScreen(SC_LAUNCH); sfx(SFX_CLICK); }
        fillRect(r, hovered(r) ? C_BTNH : C_PANEL);
        drawRect(r, C_BORDER);
        drawText(std::to_string(k + 1) + ".", r.x + 12, r.y + 20, 2, C_DIM);
        drawText(fitText(p.name, 2, 300), r.x + 60, r.y + 8, 2, C_TEXT);
        drawText(CATS[p.cat].name, r.x + 60, r.y + 32, 2, C_DIM);
        drawText(fmt1(p.rating), r.x + 400, r.y + 14, 3, ratingCol(p.rating));
        drawText("Продано " + fmtNum(p.units) + " шт.", r.x + 520, r.y + 8, 2, C_TEXT);
        drawText("Прибыль " + fmtMoney(p.profit), r.x + 520, r.y + 32, 2, p.profit >= 0 ? C_GREEN : C_RED);
        drawText(p.onSale ? "В продаже" : "Архив", r.x + 1000, r.y + 20, 2, p.onSale ? C_GREEN : C_DIM);
        drawFocus(r);
    }
    endArea((int)idx.size() * 64);
}

void scrPause() {
    fillRect(0, 0, W, H, {0, 0, 0, 150});
    panel(340, 70, 600, 600, "");
    drawTextCShadow("Пауза", W / 2, 92, 4, C_ACC);
    drawText("Громкость", 380, 172, 2, C_TEXT);
    if (button({540, 156, 56, 52}, "-", A.vol > 0.001f)) A.vol = std::max(0.0f, A.vol - 0.1f);
    progressBar({606, 172, 200, 20}, A.vol, C_ACC);
    if (button({816, 156, 56, 52}, "+", A.vol < 0.999f)) { A.vol = std::min(1.0f, A.vol + 0.1f); }
    if (!A.dev) drawTextC("(звук недоступен)", W / 2, 216, 2, C_DIM);
    int y = 240;
    if (button({440, y, 400, 60}, "Продолжить", true, true)) setScreen(retScreen == SC_PAUSE ? SC_MAIN : retScreen);
    y += 72;
    if (button({440, y, 400, 60}, "Сохранить", !savePath.empty()))
        toast(saveGame() ? "Игра сохранена" : "Не удалось сохранить");
    y += 72;
    if (button({440, y, 400, 60}, "Загрузить", saveExists)) {
        if (loadGame()) { toast("Игра загружена"); setScreen(SC_MAIN); }
        else toast("Не удалось загрузить");
    }
    y += 72;
    if (button({440, y, 400, 60}, "Новая игра")) { newName = ""; setScreen(SC_NEW); }
    y += 72;
    if (button({440, y, 400, 60}, "Выход")) running = false;
    y += 72;
    if (button({440, y, 400, 60}, "Награды и рейтинг")) setScreen(SC_HALL);
    if (savePath.empty()) drawTextC("Сохранение недоступно на этом устройстве", W / 2, 640, 2, C_RED);
    else drawTextC("A - выбор, B - назад", W / 2, 640, 2, C_DIM);
}

void scrNew() {
    fillGradV({0, 0, W, H}, {26, 32, 52, 255}, {12, 15, 24, 255});
    for (int i = 0; i < 24; ++i) fillRect((i * 97 + (int)(uiTime * 20)) % W, (i * 53) % H, 3, 3, C_BORDER);
    drawTextCShadow("ДЕВАЙС-КОРП", W / 2, 40, 7, C_ACC);
    drawTextC("Симулятор компании по разработке электроники", W / 2, 110, 2, C_DIM);
    drawText("Название компании", 340, 168, 2, C_TEXT);
    SDL_Rect f = {340, 192, 600, 60};
    if (widget(f, true)) {
        textActive = true;
        SDL_StartTextInput();
        sfx(SFX_CLICK);
    }
    fillRect(f, C_PANEL2);
    drawRect(f, textActive ? C_ACC : C_BORDER, 2);
    std::string shown = newName.empty() && !textActive ? std::string("Нажмите, чтобы ввести") : newName;
    drawText(fitText(shown, 3, 570), 356, 211, 3, newName.empty() ? C_DIM : C_TEXT);
    if (textActive && ((int)(uiTime * 2) % 2 == 0)) fillRect(356 + textW(newName, 3) + 4, 208, 3, 28, C_ACC);
    drawFocus(f);
    if (button({340, 264, 290, 54}, "Случайное имя")) newName = COMPANY_NAMES[irand(0, 9)];
    if (button({650, 264, 290, 54}, "Стереть", !newName.empty())) newName.clear();
    drawText("Стартовый капитал", 340, 338, 2, C_TEXT);
    const char* dn[3] = {"Лёгкий", "Обычный", "Хардкор"};
    const char* dm[3] = {"$500 000", "$150 000", "$50 000"};
    for (int i = 0; i < 3; ++i) {
        SDL_Rect r = {340 + i * 205, 364, 190, 80};
        if (button(r, "", true, capChoice == i)) capChoice = i;
        drawTextC(dn[i], r.x + r.w / 2, r.y + 16, 2, C_TEXT);
        drawTextC(dm[i], r.x + r.w / 2, r.y + 46, 2, C_GREEN);
    }
    if (button({340, 470, 600, 66}, "Основать компанию", true, true)) {
        if (textActive) { textActive = false; SDL_StopTextInput(); }
        newGame(newName, capChoice);
        setScreen(SC_DESIGN);  // сначала создаём первое устройство
        toast("Создайте первое устройство");
        sfx(SFX_DONE);
    }
    if (saveExists && button({340, 552, 600, 56}, "Продолжить сохранённую игру")) {
        if (textActive) { textActive = false; SDL_StopTextInput(); }
        if (loadGame()) setScreen(SC_MAIN);
        else toast("Сохранение повреждено");
    }
    drawTextC("Тач: тап и перетаскивание.  Геймпад: крестовина, A - выбор, B - назад, стики - прокрутка.", W / 2,
              650, 2, C_DIM);
    drawTextC("LB/RB - скорость времени, Start - меню.", W / 2, 676, 2, C_DIM);
}

void scrOver() {
    fillGradV({0, 0, W, H}, {26, 32, 52, 255}, {12, 15, 24, 255});
    drawTextC("БАНКРОТСТВО", W / 2, 120, 6, C_RED);
    drawTextC("Компания «" + G.company + "» не смогла расплатиться с долгами.", W / 2, 230, 2, C_TEXT);
    drawTextC("Прожито дней: " + std::to_string(G.day) + ",  устройств: " + std::to_string(G.products.size()) +
                  ",  фанатов: " + fmtNum(G.fans), W / 2, 270, 2, C_DIM);
    double best = 0;
    for (size_t i = 0; i < G.products.size(); ++i) best = std::max(best, G.products[i].rating);
    drawTextC("Лучшая оценка: " + fmt1(best), W / 2, 300, 2, C_DIM);
    if (button({440, 400, 400, 66}, "Новая игра", true, true)) { newName = ""; setScreen(SC_NEW); }
    if (button({440, 486, 400, 60}, "Выход")) running = false;
}

void renderFrame() {
    setc(C_BG);
    SDL_RenderClear(R);
    if (screen == SC_NEW) scrNew();
    else if (screen == SC_OVER) scrOver();
    else {
        Screen base = screen == SC_PAUSE ? retScreen : screen;
        if (screen != SC_PAUSE) {
            topBar();
            switch (base) {
                case SC_MAIN: scrMain(); break;
                case SC_DESIGN: scrDesign(); break;
                case SC_PLATFORM: scrPlatform(); break;
                case SC_AWARDS: scrAwards(); break;
                case SC_HALL: scrHall(); break;
                case SC_ADS: scrAds(); break;
                case SC_FACTORY: scrFactory(); break;
                case SC_PATENT: scrPatents(); break;
                case SC_EVENT: scrEvent(); break;
                case SC_SUIT: scrSuit(); break;
                case SC_BANK: scrBank(); break;
                case SC_SUPPLY: scrSupply(); break;
                case SC_LAUNCH: scrLaunch(); break;
                case SC_OFFICE: scrOffice(); break;
                case SC_RESEARCH: scrResearch(); break;
                case SC_MARKET: scrMarket(); break;
                case SC_STATS: scrStats(); break;
                default: break;
            }
        } else {
            // фон под паузой: без интерактивных элементов
            fillRect(0, 0, W, 64, C_PANEL2);
            drawText(fmtMoney(G.money), 340, 18, 3, G.money < 0 ? C_RED : C_GREEN);
            scrPause();
        }
    }
    if (toastT > 0) {
        int w = textW(toastMsg, 2) + 40;
        SDL_Rect r = {W / 2 - w / 2, H - 70, w, 44};
        fillRect(r, {10, 12, 20, 230});
        drawRect(r, C_ACC);
        drawTextC(toastMsg, W / 2, H - 55, 2, C_TEXT);
    }
    SDL_RenderPresent(R);
}

}  // namespace

// ============================================================================
//  main
// ============================================================================
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    bool haveAudio = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
    bool havePad = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;

    SDL_Window* win = SDL_CreateWindow("Девайс-Корп", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    R = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!R) R = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!R) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(R, W, H);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    SDL_StopTextInput();

    rng.seed((unsigned)SDL_GetTicks() ^ 0x9E3779B9u ^ (unsigned)std::time(nullptr));
    initFont();
    buildTechs();
    G.techDone.assign(TECHS.size(), 0);

    char* pref = SDL_GetPrefPath("DeviceCorp", "DeviceCorp");
    if (pref) {
        savePath = std::string(pref) + "save.txt";
        SDL_free(pref);
        SDL_RWops* f = SDL_RWFromFile(savePath.c_str(), "rb");
        if (f) { saveExists = true; SDL_RWclose(f); }
    }

    if (haveAudio) {
        SDL_AudioSpec want, got;
        SDL_zero(want);
        SDL_zero(got);
        want.freq = 44100;
        want.format = AUDIO_S16SYS;
        want.channels = 1;
        want.samples = 1024;
        want.callback = audioCb;
        want.userdata = &A;
        A.dev = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
        if (A.dev) {
            A.rate = got.freq > 0 ? got.freq : 44100;
            SDL_PauseAudioDevice(A.dev, 0);
        }
    }

    SDL_GameController* pad = nullptr;
    if (havePad) {
        for (int i = 0; i < SDL_NumJoysticks() && !pad; ++i)
            if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);
    }

    Uint32 last = SDL_GetTicks();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running = false; break;
                case SDL_APP_WILLENTERBACKGROUND:
                    if (screen != SC_NEW && screen != SC_OVER) saveGame();
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                    if (!pad && havePad) pad = SDL_GameControllerOpen(e.cdevice.which);
                    break;
                case SDL_CONTROLLERDEVICEREMOVED:
                    if (pad && !SDL_GameControllerGetAttached(pad)) {
                        SDL_GameControllerClose(pad);
                        pad = nullptr;
                        U.pad = false;
                    }
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    switch (e.cbutton.button) {
                        case SDL_CONTROLLER_BUTTON_DPAD_UP: uiNav(0); break;
                        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: uiNav(1); break;
                        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: uiNav(2); break;
                        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: uiNav(3); break;
                        case SDL_CONTROLLER_BUTTON_A: uiActivate(); break;
                        case SDL_CONTROLLER_BUTTON_B: U.pad = true; goBack(); break;
                        case SDL_CONTROLLER_BUTTON_START:
                            U.pad = true;
                            if (screen == SC_PAUSE) goBack();
                            else if (screen != SC_NEW && screen != SC_OVER && !(screen == SC_DESIGN && mustDesignFirst())) setScreen(SC_PAUSE);
                            break;
                        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: G.speed = std::max(0, G.speed - 1); sfx(SFX_NAV); break;
                        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: G.speed = std::min(3, G.speed + 1); sfx(SFX_NAV); break;
                        default: break;
                    }
                    break;
                case SDL_KEYDOWN: {
                    SDL_Keycode k = e.key.keysym.sym;
                    if (textActive) {
                        if (k == SDLK_BACKSPACE) {
                            while (!newName.empty()) {
                                unsigned char c = (unsigned char)newName.back();
                                newName.pop_back();
                                if ((c & 0xC0) != 0x80) break;
                            }
                        } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
                            textActive = false;
                            SDL_StopTextInput();
                        }
                        break;
                    }
                    if (k == SDLK_UP) uiNav(0);
                    else if (k == SDLK_DOWN) uiNav(1);
                    else if (k == SDLK_LEFT) uiNav(2);
                    else if (k == SDLK_RIGHT) uiNav(3);
                    else if (k == SDLK_RETURN || k == SDLK_SPACE || k == SDLK_KP_ENTER) uiActivate();
                    else if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_AC_BACK) goBack();
                    break;
                }
                case SDL_TEXTINPUT:
                    if (textActive) {
                        std::string t = e.text.text;
                        if (cpLen(newName) + cpLen(t) <= 20) newName += t;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        U.pad = false;
                        U.mdown = true;
                        U.mx = U.dx0 = e.button.x;
                        U.my = U.dy0 = U.lastY = e.button.y;
                        U.dragging = false;
                        U.dragArea = areaAt(e.button.x, e.button.y);
                        U.sliderPress = false;
                        for (size_t i = 0; i < U.psl.size(); ++i)
                            if (inRect(e.button.x, e.button.y, U.psl[i])) U.sliderPress = true;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    U.mx = e.motion.x;
                    U.my = e.motion.y;
                    if (U.mdown) {
                        U.pad = false;
                        if (!U.sliderPress && !U.dragging && U.dragArea >= 0 && std::abs(U.my - U.dy0) > 10)
                            U.dragging = true;
                        if (U.dragging) scrollBy(U.dragArea, (float)(U.lastY - U.my));
                        U.lastY = U.my;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        U.mx = e.button.x;
                        U.my = e.button.y;
                        if (U.mdown && !U.dragging && !U.sliderPress) {
                            U.click = true;
                            U.cx = e.button.x;
                            U.cy = e.button.y;
                        }
                        U.mdown = false;
                        U.dragging = false;
                        U.sliderPress = false;
                    }
                    break;
                case SDL_MOUSEWHEEL: {
                    int a = areaAt(U.mx, U.my);
                    if (a >= 0) scrollBy(a, (float)(-e.wheel.y * 60));
                    break;
                }
                default: break;
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last) / 1000.0f;
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        uiTime += dt;
        ceremonyT += dt;
        if (toastT > 0) toastT -= dt;
        if (resIdx >= 0) resT += dt;

        if (pad) {
            float ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
            float ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
            uiStick(std::fabs(ly) > std::fabs(ry) ? ly : ry, dt);
        }

        bool simRunning = screen != SC_NEW && screen != SC_OVER && screen != SC_PAUSE && screen != SC_AWARDS && screen != SC_EVENT && screen != SC_SUIT && G.speed > 0 && resIdx < 0;
        if (simRunning) {
            const float mult[4] = {0.0f, 1.0f, 2.0f, 4.0f};
            float perDay = 1.0f / (1.5f * mult[G.speed]);
            dayTimer += dt;
            int guard = 0;
            while (dayTimer >= perDay && guard < 20 && screen != SC_OVER && screen != SC_AWARDS && screen != SC_EVENT && screen != SC_SUIT) {
                dayTimer -= perDay;
                tickDay();
                ++guard;
            }
            if (guard >= 20) dayTimer = 0;
        }

        renderFrame();
        uiEndFrame();
    }

    if (pad) SDL_GameControllerClose(pad);
    if (A.dev) SDL_CloseAudioDevice(A.dev);
    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
