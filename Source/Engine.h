// The Paul 50 Bag — sound engine (C++ port of the web AudioWorklet "band").
// Framework-free on purpose: the same file can run in a plugin, an iOS AUv3 or on a Daisy.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>

namespace p50 {

constexpr double TAU = 6.283185307179586;

// ---------- random (replaces Math.random) ----------
struct Rng { uint64_t s0 = 0x9E3779B97F4A7C15ull, s1 = 0xD1B54A32D192ED03ull;
  void seed (uint64_t x) { s0 = x ^ 0x9E3779B97F4A7C15ull; s1 = (x * 0xBF58476D1CE4E5B9ull) ^ 0x94D049BB133111EBull; if (!s0 && !s1) s1 = 1; for (int i = 0; i < 8; ++i) next(); }
  uint64_t next() { uint64_t a = s0, b = s1; s0 = b; a ^= a << 23; s1 = a ^ b ^ (a >> 17) ^ (b >> 26); return s1 + b; }
  double operator()() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); } };
inline Rng& rng() { static thread_local Rng r; return r; }
inline double R() { return rng()(); }
inline int ri (double x) { return (int) std::floor (x); }           // Math.floor → int
inline int rnd (double x) { return (int) std::floor (x + 0.5); }    // Math.round (positive-safe enough for our uses)
inline double jsRound (double x) { return std::floor (x + 0.5); }

inline double BPMF (double t) { return t < .03 ? 0 : t < .8 ? 10 * std::pow (30.0, (t - .03) / .77) : 300 * std::pow (200.0, (t - .8) / .2); }
inline double SLIM (double x) { double a = std::fabs (x); return a < .9 ? x : (x < 0 ? -1 : 1) * (.9 + .1 * std::tanh ((a - .9) / .1)); }
inline float FRD (const float* b, int m, double p) { int i = ri (p); double f = p - i; float a = b[i & m]; return (float) (a + (b[(i + 1) & m] - a) * f); }
inline float FRDN (const float* b, int N, double p) { p = std::fmod (std::fmod (p, (double) N) + N, (double) N); int i = (int) p; double f = p - i; float a = b[i]; return (float) (a + (b[(i + 1) % N] - a) * f); }
inline double blep (double t, double dt) { if (t < dt) { double x = t / dt; return x + x - x * x - 1; } if (t > 1 - dt) { double x = (t - 1) / dt; return x * x + x + x + 1; } return 0; }
inline double tri (double p) { return p < .5 ? 4 * p - 1 : 3 - 4 * p; }

// ---------- tables ----------
struct Groove { const char *n, *k, *s, *c, *h, *o, *t; };
static const Groove GRV[8] = {
  {"MINIMAL","X...x...X...x...","................","....x.......x...","..x...x...x...x.","................","..........1....."},
  {"TECHNO","X...x...X...x...","....x.......x...","................","gxgxgxgxgxgxgxgx","..x...x...x...x.","................"},
  {"ELECTRO","X.....x...x.....","....X.......X...","....x.......x...","x.x.x.x.x.xxx.x.","................",".............32."},
  {"HOUSE","X...x...X...x...","................","....X.......X...","g.g.g.g.g.g.g.g.","..x...x...x...x.","................"},
  {"BREAKBEAT","X.........x.....","....X..g.g..X..g","................","x.x.x.x.x.x.x.x.","..............x.","................"},
  {"BROKEN","X..x......x..x..","....X.....g.X...","................","x.gxx.gxx.gxx.gx",".......x........","...........2...."},
  {"DUB","X...x...X...x...","................","....g.......x...","..x...x...x...x.","......x.......x.","................"},
  {"BERLIN","X.......x.......","................","................","x.g.x.g.x.g.x.g.","................","......1.......2."} };
static const char* BASSG[8] = {"..0...0...0...0.","0..0..0.0..3..4.","0.o.0.o.0.o.0.o.","0..0.0..2..0.4..","0000o000000o0000","0...0...4...3...","0.0.o.0.3.0.o.4.","o.0..0.o..0.0..."};
static const char* LEADG[8] = {"0.2.4.2.0.2.4.o.","..4...2...4...0.","0..2..4..o..4..2","4.......2.......","o.4.2.0.o.4.2.0.","0.......4...2...","2.4.o.4.2.4.o.4.","0..o..4..2..0..."};
struct Prog { int n; int d[8]; int b; };
static const Prog PROG[9] = { {0,{0},1}, {4,{0,0,0,-1},2}, {4,{0,5,3,4},1}, {4,{0,-1,-2,-1},4}, {4,{0,5,2,6},2}, {2,{0,3},2}, {4,{0,1,0,-1},1}, {4,{0,2,4,5},2}, {8,{0,0,3,3,-2,-2,4,4},1} };
static const int SC[7][7] = {{0,2,3,5,7,8,10},{0,2,4,5,7,9,11},{0,2,4,7,9},{0,3,5,7,10},{0,2,3,5,7,9,10},{0,2,3,7,8},{0,1,4,5,7,8,10}};
static const int SCL[7] = {7,7,5,5,7,5,7};
static const int DIV[5][5] = {{16,8,4,2,1},{16,8,4,2,1},{4,2,1},{128,64,32,16},{16,8,4,2,1}};
static const int DIVN[5] = {5,5,3,4,5};
static const double EGL[13] = {1,1.4,1.4,1.5,3.5,2.4,2.6,2.2,2,1.6,1.5,3.2,1.5}, EGB[13] = {1,1.4,1.2,1.4,1.5,1.15,1.6,1.5,1.1,1.3,1.3,1.8,1.3};
static const double VOW[5][2] = {{730,1090},{530,1840},{270,2290},{570,840},{300,870}};
static const double ORG[4][8] = {{0,1,0,0,0,0,0,0},{.8,1,.8,.5,0,0,0,0},{.5,1,.6,1,.7,.5,.4,.3},{0,1,0,.8,0,.6,0,.5}}, ORR[8] = {.5,1,1.5,2,3,4,6,8};
static const double MODR[3][4] = {{1,2.756,5.404,8.933},{1,2.0,2.66,3.98},{1,1.59,2.14,2.65}};
static const double SONG[25][3] = {{67,.75,0},{67,.25,0},{69,1,48},{67,1,0},{72,1,0},{71,2,43},{67,.75,0},{67,.25,0},{69,1,43},{67,1,0},{74,1,0},{72,2,48},
  {67,.75,0},{67,.25,0},{79,1,48},{76,1,0},{72,1,0},{71,1,41},{69,2,0},{77,.75,0},{77,.25,0},{76,1,48},{72,1,0},{74,1,43},{72,3,36}};
static const double RAT[10] = {.5,1,1.5,2,2.5,3,3.5,4,5,7};
static const double HF6[6] = {205.3,304.4,369.6,522.7,540,800};
inline bool isEx (int e) { return e == 6 || e == 11 || e == 12; }
inline int32_t BB (int f, int32_t t, int s) {
  switch (f) { case 0: return t * (t >> s | t >> (s + 3)); case 1: return t * ((t >> s) & (t >> (s + 2)) & 63); case 2: return (t * 5 & t >> s) | (t * 3 & t >> (s + 3));
    case 3: return t ^ (t >> s) ^ (t >> (s + 5)); case 4: return (t >> s | t | t >> (s + 4)) * 10 + 4 * (t & t >> (s + 6) | t >> 2); case 5: return t * (42 & t >> s);
    case 6: return (t & t >> s) - t * ((t >> (s + 4)) & 3); default: return ((t >> s) * t) | t >> 3; } }

constexpr int TS = 2048;
struct Tables { float WT[8][TS + 1];
  Tables() {
    auto add = [this] (int idx, std::vector<std::array<double,3>> h) { float* t = WT[idx]; double m = 0;
      for (int i = 0; i <= TS; ++i) { double x = (double) i / TS * TAU, v = 0; for (auto& q : h) v += q[1] * std::sin (q[0] * x + q[2]); t[i] = (float) v; m = std::max (m, std::fabs (v)); }
      for (int i = 0; i <= TS; ++i) t[i] = (float) (t[i] / m); };
    auto H = [] (auto f) { std::vector<std::array<double,3>> h; for (int n = 1; n < 40; ++n) { double a, ph = 0; if (f (n, a, ph)) h.push_back ({ (double) n, a, ph }); } return h; };
    add (0, {{1,1,0}});
    add (1, H ([] (int n, double& a, double&) { if (!(n % 2)) return false; a = (n % 4 == 1 ? 1.0 : -1.0) / (n * n); return true; }));
    add (2, H ([] (int n, double& a, double&) { a = 1.0 / n * std::exp (-n * .04); return true; }));
    add (3, H ([] (int n, double& a, double&) { if (!(n % 2)) return false; a = 1.0 / n; return true; }));
    add (4, H ([] (int n, double& a, double&) { a = std::sin (n * 3.14159265358979323846 * .18) / n; return a != 0; }));
    add (5, H ([] (int n, double& a, double&) { double f = n * 120.0; a = (std::exp (-std::pow ((f - 650) / 180, 2)) + .7 * std::exp (-std::pow ((f - 1100) / 220, 2)) + .25 * std::exp (-std::pow ((f - 2500) / 400, 2)) + .02) / std::sqrt ((double) n); return true; }));
    add (6, {{1,1,0},{3,.55,0},{5,.4,0},{7,.3,0},{9,.25,1},{11,.2,2},{13,.12,1}});
    add (7, H ([] (int n, double& a, double& ph) { a = (n % 3 == 0 ? .9 : .3) / std::sqrt ((double) n); ph = n * 1.3; return true; })); } };
inline const Tables& tables() { static Tables t; return t; }

// ---------- building blocks ----------
struct SVF { double a1 = 0, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0, bp = 0, lp = 0, hp = 0;
  void set (double fc, double q, double sr) { fc = std::min (std::max (fc, 20.0), sr * .45); double g = std::tan (3.14159265358979323846 * fc / sr); k = 1 / q; a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2; }
  double run (double x) { double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3; ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; lp = v2; bp = v1; hp = x - k * v1 - v2; return v2; }
  void reset() { ic1 = ic2 = 0; } };
struct Ladder { double y[4] = {0,0,0,0};
  double run (double x, double g, double k) { double xi = std::tanh (x - k * y[3]); y[0] += g * (xi - y[0]); y[1] += g * (y[0] - y[1]); y[2] += g * (y[1] - y[2]); y[3] += g * (y[2] - y[3]);
    if (!(y[3] == y[3])) y[0] = y[1] = y[2] = y[3] = 0; return y[3] * (1 + k * .45); } };

struct Sample { std::vector<float> d; double rf = 440; bool loop = false; };
using SampleP = std::shared_ptr<const Sample>;

struct Voice {
  double sr; double p[8]; int reg; int prev = 0; double cv = .5, fbm = 0, t = 0, lp = 0;
  float ks[4096] = {}; double exc = 0; SampleP sm; double sp = 1e12; int son = 0, ksi = 0, ksl = 200; SVF nf; double crk = 0;
  SVF md[4]; double mf = 0, mq = -1, ma = -1;
  explicit Voice (double s = 48000) : sr (s) { for (auto& x : p) x = R(); reg = (int) (R() * 255); }
  void excite (double f, double B) { exc = 1; sp = -1; int N = std::min (4095, std::max (4, rnd (sr / f))); ksl = N; ksi = 0; double pos = .1 + B * .4;
    for (int i = 0; i < N; ++i) { double x = (double) i / N; ks[i] = (float) ((R() * 2 - 1) * .7 + (x < pos ? x / pos : (1 - x) / (1 - pos)) * .8 - .4); } }
  double run (int e, double f, double A, double B, double env) {
    const auto& T = tables();
    if (e == 0) { double t2 = tri (p[1]), t1 = tri (p[0]);
      double f1 = f * std::pow (2.0, A * 2.2 * (cv - .5) * 2 + A * .9 * t2), f2 = f * std::pow (2.0, (B - .5) * 5) * (1 + A * .5 * t1);
      p[0] += std::min (.45, f1 / sr); if (p[0] >= 1) p[0] -= 1; p[1] += std::min (.45, f2 / sr); if (p[1] >= 1) p[1] -= 1;
      int pu = p[1] < .5 ? 1 : 0; if (pu && !prev) { reg = ((reg << 1) | (t1 > 0 ? 1 : 0)) & 255; cv = ((reg >> 5) & 7) / 7.0; } prev = pu;
      double cmp = t1 > t2 ? 1 : -1; lp += (cmp - lp) * .35; return lp * .75 + t1 * .35 + (cv - .5) * B * .6; }
    if (e == 1) { double r = RAT[std::min (9, ri (A * 10))]; p[2] += f * r / sr; if (p[2] >= 1) p[2] -= 1; double m = std::sin (TAU * p[2] + fbm * B * 1.2); fbm = m;
      double idx = B * 7 * (.25 + .75 * env); p[0] += f / sr; if (p[0] >= 1) p[0] -= 1; p[1] += f * 1.003 / sr; if (p[1] >= 1) p[1] -= 1;
      return (std::sin (TAU * p[0] + idx * m) + std::sin (TAU * p[1] + idx * m * .9)) * .5; }
    if (e == 2) { double pos = std::min (6.999, A * 7 + env * B * .6); int i0 = (int) pos; double fr = pos - i0; const float *TA = T.WT[i0], *TB = T.WT[i0 + 1]; double w = .5 - B * .42, o = 0;
      for (int k = 0; k < 2; ++k) { p[k] += f * (k ? 1.0045 : 1) / sr; if (p[k] >= 1) p[k] -= 1; double ph = p[k]; ph = ph < w ? ph * .5 / w : .5 + (ph - w) * .5 / (1 - w);
        double x = ph * TS; int j = (int) x; double q = x - j; double a = TA[j] + (TA[j + 1] - TA[j]) * q, b = TB[j] + (TB[j + 1] - TB[j]) * q; o += a + (b - a) * fr; }
      return o * .55; }
    if (e == 3) { t += f * 64 / sr; if (t > 1e9) t = std::fmod (t, 1e9); int32_t Ti = (int32_t) std::floor (t); int s = 1 + ri (B * 7); int v = BB (std::min (7, ri (A * 8)), Ti, s) & 255;
      double x = v / 127.5 - 1; lp += (x - lp) * .5; return lp * .8; }
    if (e == 4) { nf.set (f * (1 + env * .5), 2 + B * B * 70, sr); double n = R() * 2 - 1; nf.run (n); if (R() < A * A * .004) crk = 1; crk *= .92;
      return nf.bp * nf.k * (2 + B * 6) + (R() * 2 - 1) * crk * A; }
    if (e == 5) { double dt = A * A * .035, c = 0, sd = 0;
      for (int k = 0; k < 7; ++k) { double d = (k - 3) / 3.0, ff = f * (1 + d * dt), q = ff / sr; p[k] += q; if (p[k] >= 1) p[k] -= 1; double s = 2 * p[k] - 1 - blep (p[k], q); if (k == 3) c += s; else sd += s; }
      return c * (1 - B * .6) * .6 + sd * (.15 + B * .2); }
    if (e == 7) { p[0] += f / sr; if (p[0] >= 1) p[0] -= 1; double ph = p[0], vp = std::min (3.999, A * 4); int vi = (int) vp; double vf = vp - vi, up = 1 + env * .15;
      double F1 = (VOW[vi][0] + (VOW[vi + 1][0] - VOW[vi][0]) * vf) * up, F2 = (VOW[vi][1] + (VOW[vi + 1][1] - VOW[vi][1]) * vf) * up; double w = 1 - ph; w *= w; double w2 = w * (B * w + (1 - B));
      return (std::sin (TAU * ph * std::max (1.0, F1 / f)) * w2 + .55 * std::sin (TAU * ph * std::max (1.0, F2 / f)) * w2 * w) * 1.1; }
    if (e == 8) { double rp = std::min (2.999, A * 3); int r0 = (int) rp; double rf = rp - r0, o = 0; const float* S = T.WT[0];
      for (int k = 0; k < 8; ++k) { double ff = f * ORR[k]; if (ff > sr * .42) break; p[k] += ff / sr; if (p[k] >= 1) p[k] -= 1; double a = ORG[r0][k] + (ORG[r0 + 1][k] - ORG[r0][k]) * rf; if (k == 4) a += B * env * 1.4; if (a > 0) o += a * S[(int) (p[k] * TS)]; }
      return o * .32; }
    if (e == 9) { p[0] += f / sr; if (p[0] >= 1) p[0] -= 1; double ph = p[0], d = .5 - A * .46, pw = ph < d ? ph * .5 / d : .5 + (ph - d) * .5 / (1 - d);
      double o1 = -std::cos (TAU * pw), o2 = (1 - ph) * std::cos (TAU * ph * (1 + B * 12 + env * B * 5)); return o1 * (1 - B * .8) + o2 * B * 1.1; }
    if (e == 10) { p[0] += f / sr; if (p[0] >= 1) p[0] -= 1; p[1] += f * .5 / sr; if (p[1] >= 1) p[1] -= 1; double x = std::sin (TAU * p[0]) + .35 * std::sin (TAU * p[1]);
      double y = std::sin (x * (1 + A * 6 + env * B * 7)); lp += (y - lp) * (.04 + env * .9); return lp * .9; }
    if (e == 11) { if (std::fabs (f - mf) > f * .003 || mq != B || ma != A) { mf = f; mq = B; ma = A; double mp = std::min (1.999, A * 2); int mi = (int) mp; double mr = mp - mi;
        for (int k = 0; k < 4; ++k) { double ratio = MODR[mi][k] + (MODR[mi + 1][k] - MODR[mi][k]) * mr; md[k].set (std::min (sr * .45, f * ratio), 20 + B * B * 500, sr); } }
      double x = (R() * 2 - 1) * exc; exc *= .97; static const double G[4] = {1,.7,.45,.3}; double o = 0; for (int k = 0; k < 4; ++k) { md[k].run (x); o += md[k].bp * G[k]; }
      return std::tanh (o * (5 + B * 6)); }
    if (e == 12) { auto S = sm; if (!S || S->d.empty()) return 0; const auto& d = S->d; int N = (int) d.size(); double st = std::min (.9, B) * N; if (sp < 0) { sp = st; son = 1; } if (!son) return 0;
      int i = (int) sp; double fr = sp - i, a = d[i % N], b = d[(i + 1) % N]; sp += f / S->rf * std::pow (2.0, jsRound (A * 4) - 2);
      if (sp >= N - 1) { if (S->loop) sp = st + std::fmod (sp - st, std::max (64.0, N - 1 - st)); else son = 0; } return a + (b - a) * fr; }
    int N = ksl, i = ksi, j = (i + 1) % N; double y = ks[i], av = (y + ks[j]) * .5; ks[i] = (float) ((y * (A * .6) + av * (1 - A * .6)) * (.990 + A * .0095)); ksi = j; return y; } };

// master-bus effect: 0 = tempo delay, 1 = reverb (+ tilt filter p3)
struct Rack {
  double sr; std::vector<float> d[2]; int w = 0; double lp[2] = {0,0};
  std::vector<float> fdn[4]; int fi[4] = {0,0,0,0}; double fl[4] = {0,0,0,0};
  struct AP { std::vector<float> b; int i = 0; double g; }; AP ap[3];
  double tp3 = -1, tk = 0, tz[2] = {0,0};
  explicit Rack (double s = 48000) : sr (s) {
    d[0].assign ((size_t) (sr * 2), 0.f); d[1].assign ((size_t) (sr * 2), 0.f);
    const int fn[4] = {1427,1867,2213,2683}; for (int j = 0; j < 4; ++j) fdn[j].assign ((size_t) rnd (fn[j] * sr / 48000), 0.f);
    const int an[3] = {211,157,331}; const double ag[3] = {.6,.6,.55}; for (int j = 0; j < 3; ++j) { ap[j].b.assign ((size_t) rnd (an[j] * sr / 48000), 0.f); ap[j].g = ag[j]; } }
  void clear() { for (auto& v : d) std::fill (v.begin(), v.end(), 0.f); for (auto& v : fdn) std::fill (v.begin(), v.end(), 0.f); for (auto& a : ap) std::fill (a.b.begin(), a.b.end(), 0.f); lp[0] = lp[1] = 0; for (auto& x : fl) x = 0; tz[0] = tz[1] = 0; }
  void run (int t, double x, double p1, double p2, double step, double p3, double& L, double& Rr) {
    double l = 0, r = 0;
    if (t == 0) { static const int mult[6] = {1,2,3,4,6,8}; int N = (int) d[0].size(); double want = step * mult[std::min (5, ri (p1 * 6))]; int len = want >= N - 2 ? N - 2 : rnd (want);
      double a = d[0][(w - len + N) % N], b = d[1][(w - len + N) % N]; lp[0] += (a - lp[0]) * .45; lp[1] += (b - lp[1]) * .45;
      d[0][w] = (float) std::tanh (x + lp[1] * p2 * .88); d[1][w] = (float) std::tanh (lp[0] * p2 * .88 + x * .3); w = (w + 1) % N; l = lp[0]; r = lp[1]; }
    else { double v = x;
      for (auto& A : ap) { double q = A.b[A.i]; double ww = v + q * A.g; A.b[A.i] = (float) ww; v = q - ww * A.g; A.i = (A.i + 1) % (int) A.b.size(); }
      double s = 0; for (int j = 0; j < 4; ++j) { fl[j] += (fdn[j][fi[j]] - fl[j]) * (.25 + (1 - p2) * .6); s += fl[j]; }
      double fb = .72 + p1 * .26; for (int j = 0; j < 4; ++j) { fdn[j][fi[j]] = (float) (v * .5 + fb * (fl[j] - s * .5)); fi[j] = (fi[j] + 1) % (int) fdn[j].size(); }
      l = fl[0] + fl[2]; r = fl[1] + fl[3]; }
    if (std::fabs (p3 - .5) > .03) { if (tp3 != p3) { tp3 = p3; double fc = p3 < .5 ? 300 * std::pow (2.0, p3 * 2 * 6) : 40 * std::pow (2.0, (p3 - .5) * 2 * 6); tk = 1 - std::exp (-TAU * fc / sr); }
      tz[0] += (l - tz[0]) * tk; tz[1] += (r - tz[1]) * tk; if (p3 < .5) { l = tz[0]; r = tz[1]; } else { l -= tz[0]; r -= tz[1]; } }
    L = l; Rr = r; } };

// ---------- the band ----------
struct Mix { double g[4] = {.8,.8,.8,.8}; int mu[4] = {0,0,0,0}; double pa[3] = {.6,.45,.5}, pb[3] = {.5,.5,.5}; double rv = .35, dl = .2, rm = 0, fb = .35; double dm[4] = {.9,.9,.9,.9}; int dmu[4] = {0,0,0,0}; };
struct Master { double tempo = .573, key = 0, scale = 0, glue = .5, vol = .75; int ring = 0; double dirt = .5, prog = 0, arr = 0; };

struct Band {
  double sr = 48000;
  double K[5][10], KS[5][10]; int E[5] = {0,1,0,2,0}, S[5] = {0,0,0,0,0}, gt[5] = {1,1,1,1,1};
  struct Seq { int t, c = 1 << 8; double x = .37; int m = 7, i = 0; uint8_t h[16] = {}, hg[16] = {}; int hi = 0; int gd = 0; } sq[5];
  Master M; Mix X; int freeze = 0;
  double ag[4] = {1,1,1,1}, agT[4] = {1,1,1,1};
  int reg[5]; double pa[5], pb[5]; double st = 0; long long step = 0; double stepLen = 1e30;
  struct BassS { Voice v; double f = 55, tf = 55, ps = 0, env = 0; int acc = 0; Ladder L; } B;
  struct LeadS { Voice v; double f = 220, tf = 220, env = 0; int gate = 0; double gl = 0; SVF F; float cb[2048] = {}; int cw = 0; double lfo = 0; } Ld;
  struct DrumS { double hc=0,ho=0,st=0,spe=0,cl=0; int clb=0,clt=0; double t8=0,hz=0,kv=1,sv=1,hv=1,tv=1; SVF cF; double ka=0,kpe=0,kpe2=0,tpe=0,s8a=0,s8b=0,kp=0,ke=0,kf=0,se=0,he=0,te=0,tp=0; double tf=1;
    SVF sF,hF,rF; int crc=0; double crh=0, crh2=0; double hp[6]={0,0,0,0,0,0}; double bt=0, mp=0; double pk=1e9, ps=1e9, ph=1e9, pt=1e9; } D;
  struct PadV { Voice o; double f = 110, tf = 110; };
  struct PadS { PadV v[5]; double env = 0; SVF F, F2; float cb[2][4096] = {}; int cw = 0; double lfo = 0; } P;
  std::vector<float> mb[2]; int mw = 0; struct Rep { bool on = false; int st = 0, len = 0, i = 0, left = 0; } rep; double rp = 0, rf = 200; SVF sw; double swc = 20000, swt = 20000, drop = 1, dropT = 1; int fcr = 0; double fcrh[2] = {0,0};
  std::vector<float> dl[2]; int dw = 0; double dlp[2] = {0,0}, wow = 0;
  std::vector<float> fdn[6]; int fdi[6] = {}; double fdl[6] = {};
  struct AP { std::vector<float> b; int i = 0; double g; } ap[4];
  double cmp = 0, dc[4] = {0,0,0,0}; double DR = 0, dT = 0; double dro[50] = {}, dfw[50], dfw2[50], dfp[50], dfp2[50];
  SampleP SM[7]; // b l p dk ds dh dt
  double C[8] = {}, CR[8] = {}, CG = 0, cp = 0, cpT = 0, lf = 0, pp = 0; int flash[5] = {};
  // song
  struct Song { bool on = false; int n = -1; long left = 0, len = 0; double f = 0, tf = 0, ph = 0, ph2 = 0, bp = 0, bf = 0, env = 0, benv = 0; int st = 0, stn = 0; double hold = 0; int hc = 0; } song; double sgv = 0, dk = 1;
  // automation lanes
  struct Lane { bool on = false; std::vector<float> a; } A[5][10]; bool held[5][10] = {}; int AL = 32; double apos = 0;
  Rack RA, RB; double rmp = 0;
  // limiter
  float lb[2][64] = {}; int li = 0; double hv = 0; int hcn = 0; double lg = 1;

  void prepare (double sampleRate) {
    sr = sampleRate; rng().seed ((uint64_t) (sampleRate * 1000) ^ 0x50AU);
    for (int r = 0; r < 5; ++r) for (int j = 0; j < 10; ++j) K[r][j] = KS[r][j] = .5;
    for (int r = 0; r < 5; ++r) { sq[r].t = (int) (R() * 65535) | 1; reg[r] = (int) (R() * 255); pa[r] = R(); pb[r] = R(); }
    B.v = Voice (sr); Ld.v = Voice (sr); for (auto& v : P.v) v.o = Voice (sr);
    mb[0].assign ((size_t) (sr * 2), 0.f); mb[1].assign ((size_t) (sr * 2), 0.f);
    dl[0].assign ((size_t) (sr * 2), 0.f); dl[1].assign ((size_t) (sr * 2), 0.f);
    const int fn[6] = {1499,1733,1999,2251,2503,2797}; for (int j = 0; j < 6; ++j) fdn[j].assign ((size_t) rnd (fn[j] * sr / 48000), 0.f);
    const int an[4] = {142,107,379,277}; const double agg[4] = {.6,.6,.55,.55}; for (int j = 0; j < 4; ++j) { ap[j].b.assign ((size_t) rnd (an[j] * sr / 48000), 0.f); ap[j].g = agg[j]; }
    for (int i = 0; i < 50; ++i) { dfw[i] = TAU / (7 + R() * 30); dfw2[i] = TAU / (11 + R() * 40); dfp[i] = R() * TAU; dfp2[i] = R() * TAU; }
    RA = Rack (sr); RB = Rack (sr); }

  // ---- messages (same meaning as the web version) ----
  void reseed (int r) { auto& q = sq[r]; reg[r] = (int) (R() * 255) | 1; q.t = (int) (R() * 65535) | 1; q.c = ((int) (R() * 65535)) ? (int) (R() * 65535) : 256; q.x = .1 + R() * .8; q.m = (int) (R() * 16); }
  void startSong() { song = Song(); song.on = true; }
  void setSample (int slot, SampleP s) { if (slot < 0 || slot > 6) return; SM[slot] = s; if (slot == 0) B.v.sm = s; if (slot == 1) Ld.v.sm = s; if (slot == 2) for (auto& v : P.v) v.o.sm = s; }

  double note (int base, int deg) const { int si = rnd (M.scale * 6); const int* s = SC[si]; int L = SCL[si];
    int n = base + rnd (M.key * 11) + s[((deg % L) + L) % L] + 12 * (int) std::floor ((double) deg / L); return 440 * std::pow (2.0, (n - 69) / 12.0); }
  int idx (int r, double v) const { return rnd (v * (DIVN[r] - 1)); }
  int chord() const { const auto& p = PROG[std::min (8, std::max (0, rnd (M.prog * 8)))]; if (!p.n) return 0; return p.d[(int) ((step / 16 / p.b) % p.n)]; }
  void arrange() { double a = M.arr; if (a < .02) { for (auto& x : agT) x = 1; return; } int t[4];
    for (int guard = 0; guard < 64; ++guard) { int sum = 0; for (int i = 0; i < 4; ++i) { t[i] = R() < a * (i == 3 ? .35 : .6) ? 0 : 1; sum += t[i]; } if (sum >= 2 && (t[0] || t[3])) break; }
    for (int i = 0; i < 4; ++i) agT[i] = t[i]; }

  void clock (int r) {
    const double* Kr = KS[r]; int m = S[r]; auto& q = sq[r]; int g1 = 1;
    if (m == 0) { int nb = pa[r] < .5 ? 1 : 0; if (M.ring) nb ^= (reg[(r + 4) % 5] >> 7) & 1; double open = freeze ? 0 : Kr[1]; int bit = R() < open ? nb : (reg[r] >> 7) & 1; reg[r] = ((reg[r] << 1) | bit) & 255; }
    else if (freeze) { int i = q.hi++ & 15; reg[r] = q.h[i]; g1 = q.hg[i]; }
    else { int v = 0;
      if (m == 1) { int b = (q.t >> 15) & 1; if (R() < Kr[1] * .5) b ^= 1; if (M.ring && R() < .5) b ^= (reg[(r + 4) % 5] >> 7) & 1; q.t = ((q.t << 1) | b) & 65535; v = q.t & 255; g1 = q.t & 1; }
      else if (m == 2) { int k = 1 + rnd (Kr[1] * 11), i = q.i++ % 16; g1 = (int) std::floor ((i + 1) * k / 16.0) != (int) std::floor (i * k / 16.0) ? 1 : 0; if (g1) { int b = (q.t >> 15) & 1; if (R() < .07) b ^= 1; q.t = ((q.t << 1) | b) & 65535; } v = (q.t & 0xF0) | (g1 ? 3 : 0); }
      else if (m == 3) { static const int rules[8] = {30,90,110,150,184,45,73,105}; int rule = rules[std::min (7, ri (Kr[1] * 8))], c = q.c, nc = 0;
        for (int i = 0; i < 16; ++i) { int l = (c >> ((i + 1) & 15)) & 1, ce = (c >> i) & 1, rr = (c >> ((i + 15) & 15)) & 1; nc |= ((rule >> (l * 4 + ce * 2 + rr)) & 1) << i; }
        q.c = nc ? nc : ((int) (R() * 65535) | 1); v = (q.c >> 4) & 255; g1 = (q.c >> 8) & 1; }
      else if (m == 4) { double rr = 3.45 + Kr[1] * .55; q.x = rr * q.x * (1 - q.x); if (!(q.x > 0 && q.x < 1)) q.x = .37; v = (int) std::floor (q.x * 255); g1 = q.x > .42 ? 1 : 0; }
      else if (m == 5) { q.m += rnd ((R() - .5) * (1.5 + Kr[1] * 7)); if (q.m < 0) q.m = -q.m; if (q.m > 15) q.m = 30 - q.m; q.m = std::max (0, std::min (15, q.m)); g1 = R() < .45 + Kr[1] * .4 ? 1 : 0; v = (q.m << 4) | (g1 ? 3 : 0); }
      else if (m == 7) { int dv = DIV[r][idx (r, Kr[0])], i = (int) ((step / dv) % 16), pi = std::min (7, ri (Kr[1] * 8));
        if (r == 0 || r == 1) { char ch = (r == 0 ? BASSG : LEADG)[pi][i]; g1 = ch == '.' ? 0 : 1; q.gd = ch == 'o' ? 7 : ch == '.' ? 0 : ch - '0'; v = ((q.gd * 2) << 4) | (g1 ? 3 : 0) | (i % 4 == 0 ? 1 : 0); }
        else if (r == 3) { g1 = 1; v = 0; } else { g1 = i % 8 == 4 ? 1 : 0; v = g1 ? ((int) (R() * 15)) << 4 : 0; } }
      else { int sp = 3 + rnd (Kr[1] * 12), per = std::max (1, 2 * sp - 2); q.i = (q.i + 1) % per; int pos = q.i < sp ? q.i : per - q.i; v = (rnd ((double) pos / (sp - 1) * 15) << 4) | 3; g1 = 1; }
      reg[r] = v & 255; int i = q.hi++ & 15; q.h[i] = (uint8_t) (v & 255); q.hg[i] = (uint8_t) g1; }
    gt[r] = g1; flash[r] = 1;
    int g = reg[r]; double cv = ((g >> 4) & 15) / 15.0; int hc = chord(); bool gm = m == 7 && !freeze;
    if (r == 0) { if (!g1) return; int deg = gm ? q.gd : ri (cv * (1 + Kr[2] * 13)); B.tf = note (31, deg + hc); B.env = 1; B.acc = g & 1; if (isEx (E[0])) B.v.excite (B.tf, Kr[4]); }
    else if (r == 1) { if (m == 0 ? ((g & 2) || Kr[1] > .9) : g1) { int deg = gm ? q.gd + rnd (Kr[2] * 2) * 2 : ri (cv * (2 + Kr[2] * 12)); Ld.tf = note (60, deg + hc); Ld.gate = 1; Ld.env = std::max (Ld.env, .3); Ld.gl = stepLen * DIV[1][idx (1, Kr[0])] * .7; if (isEx (E[1])) Ld.v.excite (Ld.tf, Kr[4]); } }
    else if (r == 3) { if (m != 0 && !g1) return; int root = (M.prog > .05 || gm) ? hc : ri (cv * 7 * (.3 + Kr[2] * .7)); int cd[8] = {0,2,4}; int n = 3; if (Kr[2] > .35) cd[n++] = 6; if (Kr[2] > .7) cd[n++] = 8; while (n < 5) { cd[n] = cd[n - 3] + 7; ++n; }
      for (int i = 0; i < 5; ++i) { P.v[i].tf = note (43, root + cd[i]); if (isEx (E[3])) P.v[i].o.excite (P.v[i].tf, Kr[4]); } }
    else if (r == 4) { double L = stepLen * DIV[4][idx (4, Kr[0])];
      if (R() < Kr[2] * 1.1 && (m == 0 ? (g & 1) : g1)) { int N = (int) mb[0].size(); int len = std::max (64, rnd (std::min (1e9, stepLen * (R() < .5 ? .5 : 1)))); len = std::min (len, N - 1); rep.on = true; rep.st = (mw - len + N) % N; rep.len = len; rep.i = 0; rep.left = rnd (std::min (L, 1e9)); }
      cpT = (cv - .5) * 2; rf = 50 * std::pow (2.0, cv * 6); swt = std::max (150.0, 20000 * std::pow (2.0, -Kr[5] * cv * 7)); dropT = R() < Kr[6] * .8 ? 0 : 1; } }

  void hit (int t, double v, int x = 1) { auto& d = D; int de = E[2]; bool real = de == 6 || de == 7;
    if (t == 0) { d.ke = 1; d.ka = 1; d.kpe = 1; d.kpe2 = 1; d.kp = 0; d.mp = 0; d.kv = v; }
    else if (t == 1) { d.se = 1; d.st = 1; d.spe = 1; d.sv = v; }
    else if (t == 2) { d.he = .6 + .4 * v; d.hc = 1; d.ho = 0; d.hv = v; }
    else if (t == 3) { d.he = 1; d.ho = 1; d.hc = 0; d.hv = v; }
    else if (t == 4) { d.te = 1; d.tpe = 1; d.t8 = 1; d.tf = x ? x : 1; d.tv = v; }
    else if (t == 5) { d.cl = 1; d.clb = 3; d.clt = rnd (sr * .011); d.sv = std::max (d.se > .3 ? d.sv : 0.0, v); if (!real) d.se = std::max (d.se, .85); } }

  void drums() { const double* Kd = KS[2]; int g = reg[2]; long long s = step; double dens = Kd[2];
    auto vel = [] (char c) { return c == 'X' ? 1.0 : c == 'g' ? .42 : .8; };
    if (S[2] == 7) { const auto& G = GRV[std::min (7, ri (Kd[1] * 8))]; int i = (int) (s % 16); long long bar = s / 16;
      bool fill = bar % 8 == 7 && i >= 12 && R() < .35 + dens * .5;
      if (fill) { if (R() < .6) hit (4, .8, 4 - (i - 12)); else hit (1, .5 + R() * .4); if (i == 12 || R() < .4) hit (2, .6); return; }
      char c = G.k[i]; if (c != '.') hit (0, vel (c)); else if (bar % 4 == 3 && (i == 14 || i == 10) && R() < dens * .35) hit (0, .7);
      c = G.s[i]; if (c != '.') hit (1, vel (c)); else if ((i % 2) && R() < dens * .12) hit (1, .3);
      c = G.c[i]; if (c != '.') hit (5, vel (c));
      c = G.o[i]; if (c != '.') hit (3, vel (c)); else { c = G.h[i]; if (c != '.') hit (2, vel (c) * (.85 + R() * .15)); else if (R() < dens * .45) hit (2, .32); }
      c = G.t[i]; if (c != '.') hit (4, .8, c - '0'); return; }
    if (S[2] >= 3 && S[2] <= 5) { auto b = [g] (int k) { return (g >> k) & 1; };
      if (b (7) && R() < .35 + dens * .5) hit (0, .55 + R() * .45); if (b (5) && b (2) && R() < .3 + dens * .5) hit (1, .4 + R() * .6);
      if (b (1) && R() < .2 + dens * .7) hit (b (0) && R() < .3 ? 3 : 2, .35 + R() * .65); if (b (4) && b (3) && R() < .2 + dens * .35) hit (4, .8, 1 + ((g >> 5) & 3)); if (R() < dens * .08) hit (5, .7); return; }
    if (S[2] == 2) { auto e = [] (long long i, int k) { i = ((i % 16) + 16) % 16; return (int) std::floor ((i + 1) * k / 16.0) != (int) std::floor (i * k / 16.0); };
      int kk = 2 + rnd (dens * 4), ks = 1 + rnd (dens * 2), kh = 4 + rnd (dens * 10);
      if (e (s, kk)) hit (0, 1); if (e (s - 4, ks)) hit (1, 1); if (e (s + 1, kh)) hit (e (s, 3) && R() < .2 ? 3 : 2, e (s, 3) ? 1 : .7); if (e (s + 2, 1 + rnd (dens * 3)) && dens > .4) hit (4, .9, 1 + (int) (s & 3)); return; }
    bool kick = (((g >> 7) & 1) && (s % 4 == 0 || R() < dens * .5)) || (s % 16 == 0 && dens > .2);
    bool snare = (s % 8 == 4 && (((g >> 5) & 1) || dens > .3)) || (((g >> 3) & 1) && R() < dens * .35);
    bool hat = R() < .08 + dens * .6 && (((g >> 1) & 1) || s % 2 == 0), tom = ((g >> 2) & 1) && R() < dens * .3;
    if (kick) hit (0, 1); if (snare) hit (1, 1); if (hat) hit ((g & 1) && R() < .2 ? 3 : 2, (g & 1) ? 1 : .7); if (tom) hit (4, .9, 1 + ((g >> 4) & 3)); }

  double readSlot (int slot, double& pos, double rate, double gain) { auto Sp = SM[slot]; if (!Sp || Sp->d.empty()) return 0; const auto& dd = Sp->d; if (pos >= dd.size() - 1) return 0; int i = (int) pos; double f = pos - i; pos += rate; return (dd[i] + (dd[i + 1] - dd[i]) * f) * gain; }

  void process (float* Lo, float* Ro, int n) {
    double bpm = BPMF (M.tempo); stepLen = bpm > 0 ? sr * 60 / bpm / 4 : 1e30; double hs = std::min (1.0, bpm / 30);
    { double sl = stepLen > 1e29 ? 1 : stepLen; double pos = ((step % AL) + std::min (.999, st / sl)) / AL; apos = pos;
      for (int r = 0; r < 5; ++r) for (int j = 0; j < 10; ++j) { auto& L = A[r][j]; if (!L.on || held[r][j] || L.a.empty()) continue; int N = (int) L.a.size(); double x = pos * N; int i = ((int) std::floor (x)) % N; double f = x - std::floor (x); K[r][j] = L.a[i] + (L.a[(i + 1) % N] - L.a[i]) * f; } }
    double dr = DR; if (dr > .001 || dro[1] != 0) { dT += n / sr * (.5 + dr * 1.5); for (int i = 0; i < 50; ++i) { int j = i % 10; dro[i] = (j == 0 || j == 9) ? 0 : dr * .42 * std::sin (dT * dfw[i] + dfp[i]) * (.55 + .45 * std::sin (dT * dfw2[i] + dfp2[i])); } }
    for (int r = 0; r < 5; ++r) for (int j = 0; j < 10; ++j) { double t = std::min (1.0, std::max (0.0, K[r][j] + dro[r * 10 + j])); KS[r][j] += (t - KS[r][j]) * .02; }
    const double *KB = KS[0], *KL = KS[1], *KD = KS[2], *KP = KS[3], *KF = KS[4];
    for (int i = 0; i < 8; ++i) CR[i] += (std::max (C[i], CG * .85) - CR[i]) * .05;
    double bdec = std::exp (-hs / (sr * (.07 + KB[7] * .6))), bk = KB[6] * 3.9, bdrv = 1 + KB[8] * 7;
    Ld.F.set (150 * std::pow (2.0, std::min (7.6, KL[5] * 7 + Ld.env * 1.6 + CR[6] * D.se * 4)), .7 + KL[6] * KL[6] * 9, sr); double lgl = 1 - std::exp (-1 / (sr * (.003 + KL[7] * KL[7] * .4)));
    int de = E[2]; auto ex = [&] (double tau) { return std::exp (-hs / (sr * tau)); };
    double kdec = ex (.06 + KD[4] * .7), sdec = ex (.05 + KD[5] * .22), hdec = ex (.012 + KD[6] * .28), tdec = std::pow (.9994, hs), ldec = std::pow (.9996, hs), kf0 = 32 + KD[3] * 60, swing = KD[7] * .33,
      kpdec = ex (.022), kpdec2 = ex (.07), tpdec = ex (.045), k8dec = ex (.12 + KD[4] * 1.5), hcdec = ex (.022 + KD[6] * .09), hodec = ex (.18 + KD[6] * .55), stdec = ex (.055), spdec = ex (.009), cldec = ex (.08 + KD[5] * .16), t8dec = ex (.14 + KD[4] * .4),
      dtM = M.dirt, hpk = 1 - std::exp (-TAU * 6500 / sr);
    D.sF.set (de == 3 ? 400 + KD[5] * 2000 : 900 + KD[5] * 2600, de == 3 ? 8 : 1.2, sr); D.hF.set (de == 1 ? 9000 : 7000, .8, sr);
    if (de == 6) { D.sF.set (1700 + KD[5] * 1500, .7, sr); D.hF.set (7100, 1.3, sr); D.cF.set (1150, 1.6, sr); } else if (de == 7) { D.sF.set (2600 + KD[5] * 3500, .6, sr); D.hF.set (8800, 1.1, sr); D.cF.set (1250, 1.5, sr); }
    if (de == 4) D.hF.set (10500, 1.2, sr);
    double pat = 1 / (sr * (.05 + KP[7] * KP[7] * 5)); P.F.set (110 * std::pow (2.0, KP[5] * 7), .7 + KP[6] * 6, sr); P.F2.set (130 * std::pow (2.0, KP[5] * 7), .7 + KP[6] * 6, sr);
    double dlen = std::min (1e9, stepLen * 3), fbk = .25 + KF[7] * .6;
    for (int s = 0; s < n; ++s) {
      st++; double sl = (step % 2) ? stepLen * (1 + swing) : stepLen * (1 - swing);
      if (st >= sl) { st = 0; step++; if (step % 128 == 0) arrange();
        if (DR > .45 && step % 32 == 0 && R() < (DR - .45) * 1.4) { int rr = (int) (R() * 5); reg[rr] = (int) (R() * 255) | 1; sq[rr].t = (int) (R() * 65535) | 1; sq[rr].x = .1 + R() * .8; }
        for (int r = 0; r < 5; ++r) { int dv = DIV[r][idx (r, KS[r][0])]; if (step % dv == 0) clock (r); }
        if (step % DIV[2][idx (2, KD[0])] == 0) { double k0 = D.ke, s0 = D.se, h0 = D.he, t0 = D.te; drums(); if (D.ke > k0) { D.pk = 0; if (R() < CR[4]) clock (0); } if (D.se > s0) D.ps = 0; if (D.he > h0) D.ph = 0; if (D.te > t0) D.pt = 0; }
        if (isEx (E[3]) && step % 2 == 0) { auto& v = P.v[(step >> 1) % 5]; v.o.excite (v.f, KP[4]); } }
      for (int r = 0; r < 5; ++r) { pa[r] += (2.3 + r * 1.37) * std::pow (2.0, KS[r][3] * 3 * (pb[r] < .5 ? 1 : -1)) / sr; if (pa[r] > 1) pa[r] -= 1; pb[r] += (.9 + r * .61) / sr; if (pb[r] > 1) pb[r] -= 1; }
      // 一 BASS
      cp += (cpT - cp) * .0004; double pm = CR[5] > .002 ? std::pow (2.0, CR[5] * cp * 7 / 12) : 1;
      B.f += (B.tf - B.f) * .004; B.env *= bdec;
      double bx = B.v.run (E[0], B.f * pm, KB[3], KB[4], B.env) * EGB[E[0]]; B.ps += B.f * .5 / sr; if (B.ps >= 1) B.ps -= 1; bx += std::sin (TAU * B.ps) * .55;
      double bfc = 38 * std::pow (2.0, KB[5] * 7.5 + B.env * KB[7] * 4.5 + B.acc * B.env * 1.2 + CR[2] * lf * 12), bg = 1 - std::exp (-TAU * std::min (sr * .4, bfc) / sr);
      double by = B.L.run (bx * .4 * bdrv, bg, bk); by = std::tanh (by * (1 + KB[8] * 1.5)) * std::min (1.0, B.env * 4 + ((E[0] == 4 || E[0] == 3) ? .15 : 0)) * KB[9] * KB[9] * 1.1;
      // 二 LEAD
      Ld.f += (Ld.tf - Ld.f) * lgl; if (Ld.gate) { Ld.gl--; Ld.env += (1 - Ld.env) * .004; if (Ld.gl <= 0) Ld.gate = 0; } else Ld.env *= ldec;
      { double lu = Ld.v.run (E[1], Ld.f * pm * std::max (.05, 1 + CR[1] * 3 * by), KL[3], KL[4], Ld.env) * 1.2 * EGL[E[1]] * (1 + CR[6] * D.se * 3), lc = lu * .62; Ld.F.run (lc + (std::tanh (lu) - lc) * dtM); }
      double lx = Ld.F.lp * Ld.env * KL[9] * KL[9] * .8; lf += (std::min (1.0, std::fabs (lx) * 4) - lf) * .003;
      Ld.lfo += .7 / sr; if (Ld.lfo > 1) Ld.lfo -= 1; int cw = Ld.cw; Ld.cb[cw] = (float) lx; Ld.cw = (cw + 1) & 2047; double dA = sr * (.006 + .002 * std::sin (TAU * Ld.lfo)), dB = sr * (.008 + .002 * std::cos (TAU * Ld.lfo));
      double lyL = lx * .75 + FRD (Ld.cb, 2047, cw - dA) * .45, lyR = lx * .75 + FRD (Ld.cb, 2047, cw - dB) * .45;
      // 三 DRUMS
      double dk2 = 0, ds = 0, dh = 0, dt = 0; double nz = R() * 2 - 1; auto& d = D;
      d.ke *= kdec; d.se *= sdec; d.he *= hdec; d.te *= tdec; d.kpe *= kpdec; d.kpe2 *= kpdec2; d.tpe *= tpdec; d.hc *= hcdec; d.ho *= hodec; d.st *= stdec; d.spe *= spdec; d.cl *= cldec; d.t8 *= t8dec;
      if (d.clb > 0 && --d.clt <= 0) { d.cl = 1; d.clb--; d.clt = rnd (sr * .011); }
      auto wrap = [] (double& ph) { if (ph > 1) ph -= 1; };
      if (de == 0) { d.kf = kf0 * (1 + 2.6 * d.kpe + .25 * d.kpe2); d.kp += d.kf / sr; wrap (d.kp); dk2 = std::tanh (std::sin (TAU * d.kp) * d.ke * 2.2) * .9 + (d.ke > .985 ? nz * .5 : 0);
        d.sF.run (nz); ds = (d.sF.bp * d.sF.k * 1.8 + std::sin (TAU * d.kp * 4.3) * .3) * d.se * (.5 + KD[5] * .5);
        double m = 0; for (int i = 0; i < 6; ++i) { d.hp[i] += HF6[i] * (1 + KD[3] * .5) / sr; wrap (d.hp[i]); m += d.hp[i] < .5 ? 1 : -1; } d.hF.run (m * .17); dh = d.hF.hp * d.he * .7; }
      else if (de == 1) { d.kf = kf0 * (1 + 2.2 * d.kpe + .2 * d.kpe2); d.kp += d.kf / sr; wrap (d.kp); d.mp += d.kf * 1.5 / sr; wrap (d.mp); dk2 = std::sin (TAU * d.kp + d.kpe * 3.2 * std::sin (TAU * d.mp)) * d.ke * 1.1;
        d.bt += (220 + KD[5] * 300) / sr; wrap (d.bt); ds = std::sin (TAU * d.bt + d.se * 9 * std::sin (TAU * d.bt * 1.414) + nz * d.se * 2) * d.se * .7;
        d.hp[0] += (3200 + KD[3] * 2000) / sr; wrap (d.hp[0]); d.hp[1] += 4720 / sr; wrap (d.hp[1]); d.hF.run (std::sin (TAU * d.hp[0] + 6 * std::sin (TAU * d.hp[1]))); dh = d.hF.hp * d.he * .6; }
      else if (de == 2) { d.bt += 1; if (d.bt > 2e9) d.bt = 0; int32_t T = (int32_t) d.bt; double v = ((T * (T >> 5 | T >> 8)) & 255) / 127.5 - 1; dk2 = v * d.ke * d.ke * .9; d.kp += kf0 * (1 + 6 * d.ke) / sr; wrap (d.kp); dk2 += std::sin (TAU * d.kp) * d.ke * .7;
        if ((T & 3) == 0) d.crh = nz; ds = d.crh * d.se * .8 * (KD[5] > .5 ? ((T & 64) ? 1 : 0) : 1); dh = ((((T * T) >> 9) & 1) * 2 - 1) * d.he * .25 * ((T & 8) ? 1 : 0); }
      else if (de == 3) { d.rF.set (kf0 * (1 + 2 * d.ke) * (1 + KD[3]), 18 + KD[4] * 40, sr); d.rF.run (d.ke > .97 ? nz * 8 : 0); dk2 = std::tanh (d.rF.bp * d.rF.k * 30) * .9 * std::min (1.0, d.ke * 2);
        int bi = (int) d.bt; d.bt += 1; if (d.bt > 2e9) d.bt = 0; d.sF.run (nz * (((reg[2] >> ((bi >> 9) & 7)) & 1) ? 1 : .2)); ds = d.sF.bp * d.sF.k * 2 * d.se;
        d.hp[0] += 3100 / sr; wrap (d.hp[0]); d.hp[1] += 4377 / sr; wrap (d.hp[1]); d.hF.run (((d.hp[0] < .5) != (d.hp[1] < .5)) ? 1 : -1); dh = d.hF.hp * d.he * .5; }
      else if (de == 4) { d.kp += kf0 * (1 + 5 * d.kpe * d.kpe + .3 * d.kpe2) / sr; wrap (d.kp); dk2 = std::tanh (std::sin (TAU * d.kp) * d.ke * 1.8) + (d.ke > .97 ? nz * .6 : 0);
        d.bt += 1; if (d.bt > 2e9) d.bt = 0; d.sF.set (1800 + KD[5] * 3000, .9, sr); d.sF.run (nz); double clap = d.se > .55 ? ((((int) d.bt >> 8) & 1) ? 1 : .25) : 1;
        ds = (std::sin (TAU * d.kp * 3.1) * .35 * d.se * d.se + d.sF.hp * .9 * d.se) * clap;
        double m = 0; for (int i = 0; i < 6; ++i) { d.hp[i] += HF6[i] * 1.6 / sr; wrap (d.hp[i]); m += d.hp[i] < .5 ? 1 : -1; } d.hF.run (m * .17 + nz * .08); dh = d.hF.hp * d.he * .8; }
      else if (de == 5) { d.kp += kf0 * (1 + 3 * d.ke) / sr; wrap (d.kp); dk2 = std::tanh (std::sin (TAU * d.kp) * std::sin (TAU * d.kp * 1.47 + 1) * d.ke * 7) * .85;
        d.bt += (1250 + KD[5] * 1500) / sr; wrap (d.bt); ds = (d.bt < .5 ? 1 : -1) * nz * d.se * .75;
        d.hp[0] += (5300 + KD[3] * 3000) / sr; wrap (d.hp[0]); d.hp[1] += 7417 / sr; wrap (d.hp[1]); dh = std::sin (TAU * d.hp[0] + 4 * std::sin (TAU * d.hp[1])) * d.he * d.he * .5; }
      else if (de == 6) { d.ka *= k8dec; d.kp += (38 + KD[3] * 34) * (1 + .5 * d.kpe) / sr; wrap (d.kp); dk2 = (std::sin (TAU * d.kp) * d.ka + (d.kpe > .9 ? std::sin (TAU * d.kp * 7) * .25 * (d.kpe - .9) * 10 : 0)) * .95;
        d.s8a += 238 * (1 + .35 * d.spe) * (.85 + KD[5] * .3) / sr; wrap (d.s8a); d.s8b += 476 * (1 + .2 * d.spe) * (.85 + KD[5] * .3) / sr; wrap (d.s8b); d.sF.run (nz); d.cF.run (nz);
        ds = ((std::sin (TAU * d.s8a) * .62 + std::sin (TAU * d.s8b) * .3) * d.st * 1.1 + d.sF.hp * d.se * .8 + nz * d.spe * .25) * d.sv + d.cF.bp * d.cF.k * d.cl * 1.9;
        double m = 0; for (int i = 0; i < 6; ++i) { d.hp[i] += HF6[i] * (.9 + KD[3] * .2) / sr; wrap (d.hp[i]); m += d.hp[i] < .5 ? 1 : -1; }
        d.hF.run (m * .17); double hb = d.hF.bp * d.hF.k; d.hz += (hb - d.hz) * hpk; dh = (hb - d.hz) * (d.hc + d.ho * .85) * 6.5 * d.hv;
        if (d.t8 > .002) { d.tp += (62 + d.tf * 30) * (1 + .12 * d.tpe) * (.8 + KD[3] * .4) / sr; wrap (d.tp); dt = (std::sin (TAU * d.tp) + nz * .03) * d.t8 * .75 * d.tv; } }
      else if (de == 7) { double pe = d.kpe; d.kp += (46 + KD[3] * 26) * (1 + 4.5 * pe * pe + .4 * d.kpe2) / sr; wrap (d.kp); double ksn = std::sin (TAU * d.kp) * .8 + tri (d.kp) * .2;
        dk2 = std::tanh (ksn * d.ke * 1.9) * .92 + (pe > .88 ? nz * .5 * (pe - .88) * 8 : 0);
        d.s8a += 185 * (1 + .6 * d.spe) * (.85 + KD[5] * .3) / sr; wrap (d.s8a); d.s8b += 330 * (1 + .4 * d.spe) * (.85 + KD[5] * .3) / sr; wrap (d.s8b); d.sF.run (nz); d.cF.run (nz);
        ds = std::tanh (((tri (d.s8a) * .6 + tri (d.s8b) * .4) * d.st * 1.3 + d.sF.bp * d.sF.k * d.se * (.8 + KD[5] * .8) + nz * d.spe * .5) * 1.6) * 1.1 * d.sv + d.cF.bp * d.cF.k * d.cl * 1.9;
        double m = 0; for (int i = 0; i < 6; ++i) { d.hp[i] += HF6[i] * 1.71 * (.9 + KD[3] * .2) / sr; wrap (d.hp[i]); m += d.hp[i] < .5 ? 1 : -1; }
        d.hF.run (m * .09 + nz * .14); double hb = d.hF.bp * d.hF.k; d.hz += (hb - d.hz) * hpk; double hv2 = (hb - d.hz) * (d.hc + d.ho * .85) * 6.5 * d.hv; if (dtM > .15) hv2 = jsRound (hv2 * 64) / 64; dh = hv2;
        if (d.t8 > .002) { d.tp += (58 + d.tf * 34) * (1 + .6 * d.tpe) * (.8 + KD[3] * .4) / sr; wrap (d.tp); dt = (std::sin (TAU * d.tp) + nz * .06) * d.t8 * .7 * d.tv; } }
      else if (de == 8) { double rt = std::pow (2.0, (KD[3] - .5) * 1.6);
        dk2 = readSlot (3, d.pk, rt, std::min (1.0, d.ke * 2.5)); ds = readSlot (4, d.ps, rt, std::min (1.0, d.se * 2.5)); dh = readSlot (5, d.ph, rt, std::min (1.0, d.he * 2)); if (d.pt < 1e8) dt = readSlot (6, d.pt, rt, std::min (1.0, d.te * 2.5)); }
      if (de < 6 && d.te > .002) { d.tp += (70 * d.tf + KD[3] * 80) * (1 + .45 * d.tpe) / sr; wrap (d.tp); dt = std::sin (TAU * d.tp) * d.te * .5; }
      if (de != 6 && de != 7) { dk2 *= d.kv; ds *= d.sv; dh *= d.hv * (de < 6 ? 1.8 : 1); dt *= d.tv * (de < 6 ? 1.6 : 1); }
      auto dg = [this] (int i) { return X.dmu[i] ? 0.0 : X.dm[i] * X.dm[i] * 1.23; };
      double dsum = dk2 * dg (0) + ds * dg (1) + dh * dg (2) + dt * dg (3); double dx = dsum * .95; if (dtM > .01) dx += (std::tanh (dsum * 1.4) - dx) * std::min (1.0, dtM * 1.5);
      if (KD[8] > .02) { if (++d.crc >= 1 + rnd (KD[8] * 20)) { d.crc = 0; double lv = std::pow (2.0, 9 - KD[8] * 6); d.crh2 = jsRound (dx * lv) / lv; } dx = dx + (d.crh2 - dx) * std::min (1.0, KD[8] * 2); }
      dx *= KD[9] * KD[9] * 1.1; if (CR[3] > .003) dx = dx * (1 - CR[3]) + std::tanh (dx * pp * 14) * CR[3] * 1.2;
      // 四 PAD
      P.env += ((KP[9] > .01 ? 1 : 0) - P.env) * pat; double pl = 0, pr = 0;
      for (int i = 0; i < 5; ++i) { auto& v = P.v[i]; v.f += (v.tf - v.f) * .0008; double x = v.o.run (E[3], v.f * pm * (1 + (i - 2) * KP[3] * .004), KP[3], KP[4], .6); if (i % 2) pl += x; else pr += x; if (i == 2) { pl += x * .5; pr += x * .5; } }
      P.F.run (pl * .3 * EGL[E[3]]); P.F2.run (pr * .3 * EGL[E[3]]); double pg = P.env * KP[9] * KP[9] * .9; double pL = P.F.lp * pg, pR = P.F2.lp * pg;
      P.lfo += .23 / sr; if (P.lfo > 1) P.lfo -= 1; int pw = P.cw; P.cb[0][pw] = (float) pL; P.cb[1][pw] = (float) pR; P.cw = (pw + 1) & 4095;
      double pd1 = sr * (.012 + .004 * std::sin (TAU * P.lfo)), pd2 = sr * (.015 + .004 * std::cos (TAU * P.lfo)); pL = pL * .7 + FRD (P.cb[1], 4095, pw - pd1) * .5; pR = pR * .7 + FRD (P.cb[0], 4095, pw - pd2) * .5;
      if (CR[7] > .003) { double rm = (1 - CR[7]) + CR[7] * std::tanh (bx * 2) * 2.2; pL *= rm; pR *= rm; } pp = pL + pR;
      // mixer
      for (int i = 0; i < 4; ++i) ag[i] += (agT[i] - ag[i]) * .00003;
      auto gg = [this] (int i) { return X.mu[i] ? 0.0 : X.g[i] * X.g[i] * 1.3 * ag[i]; }; double g0 = gg (0), g1 = gg (1), g2 = gg (2), g3 = gg (3);
      double duck = 1 - CR[0] * std::min (1.0, d.ke * 1.6), cb = by * g0 * duck, cl = lyL * g1 * duck, cr = lyR * g1 * duck, cd = dx * g2, cpl = pL * g3 * duck, cpr = pR * g3 * duck;
      double dryL = cb + cl + cd + cpl, dryR = cb + cr + cd + cpr;
      if (song.on) { auto& Sg = song; double beat = sr * 60 / 112;
        if (--Sg.left <= 0) { Sg.n++; if (Sg.n >= 25) Sg.on = false; else { const double* q = SONG[Sg.n]; Sg.len = rnd (q[1] * beat); Sg.left = Sg.len; Sg.tf = 440 * std::pow (2.0, (q[0] - 69 + (R() < .12 ? 12 : 0)) / 12); if (R() < .25) Sg.f = Sg.tf * (R() < .5 ? .5 : 2); Sg.env = 1;
            Sg.stn = R() < .35 ? 2 + (int) (R() * 3) : 0; Sg.st = 0; if (q[2] > 0) { Sg.bf = 440 * std::pow (2.0, (q[2] - 69) / 12); Sg.benv = 1; } } }
        if (Sg.on) { Sg.f += (Sg.tf - Sg.f) * .02;
          if (Sg.stn > 0) { Sg.st++; if (Sg.st >= rnd (beat / 8)) { Sg.st = 0; Sg.stn--; Sg.env = 1; } }
          double a = std::min (1.0, (Sg.len - Sg.left) / 200.0) * Sg.env; Sg.env *= .99996; Sg.ph += Sg.f / sr; if (Sg.ph >= 1) Sg.ph -= 1; Sg.ph2 += Sg.f * 1.005 / sr; if (Sg.ph2 >= 1) Sg.ph2 -= 1;
          double pwv = .3 + .2 * std::sin (TAU * Sg.ph2 * .01 + Sg.n); double v = (Sg.ph < pwv ? 1 : -1) * .5 + (2 * Sg.ph2 - 1) * .3 + std::sin (TAU * Sg.ph * .5) * .35;
          if (++Sg.hc >= 3) { Sg.hc = 0; Sg.hold = jsRound (v * 12) / 12; } v = Sg.hold * a;
          Sg.bp += Sg.bf / sr; if (Sg.bp >= 1) Sg.bp -= 1; Sg.benv *= .99993; double bv = (Sg.bp < .5 ? 1 : -1) * Sg.benv * .35 + std::sin (TAU * Sg.bp) * Sg.benv * .3;
          sgv = v * .42 + bv; } }
      { double tgt = song.on ? .28 : 1; dk += (tgt - dk) * (song.on ? .0006 : .00004); double sv = song.on ? sgv : 0; dryL = dryL * dk + sv; dryR = dryR * dk + sv; }
      double mono = (dryL + dryR) * .5, rvA = X.rv * X.rv, dlA = X.dl * X.dl, raL, raR, rbL, rbR;
      RA.run (1, mono * std::min (1.0, rvA * 2.2), X.pa[0], X.pa[1], stepLen, X.pa[2], raL, raR); RB.run (0, mono * std::min (1.0, dlA * 2.5), X.pb[0], X.fb * 1.28, stepLen, X.pb[2], rbL, rbR);
      double mL = dryL + raL * 1.4 + rbL * 1.3, mR = dryR + raR * 1.4 + rbR * 1.3;
      if (X.rm > .003) { rmp += (rf * 1.5 + 60) / sr; if (rmp > 1) rmp -= 1; double car = std::sin (TAU * rmp), a = X.rm; mL = mL * (1 - a) + mL * car * a * 1.5; mR = mR * (1 - a) + mR * car * a * 1.5; }
      if (!(mL == mL && mR == mR)) { RA.clear(); RB.clear(); mL = mR = 0; }
      // 五 CHAOS FX
      int N = (int) mb[0].size(); mb[0][mw] = (float) mL; mb[1][mw] = (float) mR; mw = (mw + 1) % N;
      double wL = mL, wR = mR;
      if (rep.on) { int ri2 = rep.i % rep.len, i = (rep.st + ri2) % N; double w = std::min ({ 1.0, ri2 / 48.0, (rep.len - ri2) / 48.0 }); wL = mb[0][i] * w; wR = mb[1][i] * w; rep.i++; if (--rep.left <= 0) rep.on = false; }
      if (KF[3] > .01) { rp += rf / sr; if (rp > 1) rp -= 1; double rm = std::sin (TAU * rp); wL = wL * (1 - KF[3]) + wL * rm * KF[3] * 1.4; wR = wR * (1 - KF[3]) + wR * rm * KF[3] * 1.4; }
      if (KF[4] > .02) { if (++fcr >= 1 + rnd (KF[4] * 24)) { fcr = 0; double lv = std::pow (2.0, 10 - KF[4] * 7); fcrh[0] = jsRound (wL * lv) / lv; fcrh[1] = jsRound (wR * lv) / lv; } wL += (fcrh[0] - wL) * std::min (1.0, KF[4] * 2); wR += (fcrh[1] - wR) * std::min (1.0, KF[4] * 2); }
      swc += (swt - swc) * .002; if (KF[5] > .02) { sw.set (swc, .9 + KF[5] * 4, sr); sw.run ((wL + wR) * .5); wL = wL * (1 - KF[5]) + sw.lp * KF[5] * 1.2; wR = wR * (1 - KF[5]) + sw.lp * KF[5] * 1.2; }
      drop += (dropT - drop) * .01; wL *= drop; wR *= drop;
      double mx = KF[9], oL = mL * (1 - mx) + wL * mx, oR = mR * (1 - mx) + wR * mx;
      wow += .5 / sr; if (wow > 1) wow -= 1; int DN = (int) dl[0].size(); double wd = std::min ((double) DN - 2, dlen * (1 + .003 * std::sin (TAU * wow) * (1 + KF[7] * 3)));
      double r0 = FRDN (dl[0].data(), DN, dw - wd), r1 = FRDN (dl[1].data(), DN, dw - std::min ((double) DN - 2, wd * 1.5)); dlp[0] += (r0 - dlp[0]) * .3; dlp[1] += (r1 - dlp[1]) * .3;
      double dIn = lx * g1 * KL[8] * 1.2 + (oL + oR) * KF[7] * .25; dl[0][dw] = (float) std::tanh (dIn + dlp[1] * fbk); dl[1][dw] = (float) std::tanh (dIn * .7 + dlp[0] * fbk); dw = (dw + 1) % DN;
      double rin = (pL + pR) * g3 * KP[8] * 1.6 + (oL + oR) * KF[8] * .35 + lx * g1 * KL[8] * .3;
      for (auto& Ap : ap) { double v = Ap.b[Ap.i]; double w = rin + v * Ap.g; Ap.b[Ap.i] = (float) w; rin = v - w * Ap.g; Ap.i = (Ap.i + 1) % (int) Ap.b.size(); }
      double rv = 0; for (int j = 0; j < 6; ++j) { fdl[j] += (fdn[j][fdi[j]] - fdl[j]) * .4; rv += fdl[j]; }
      for (int j = 0; j < 6; ++j) { fdn[j][fdi[j]] = (float) (rin * .4 + .86 * (fdl[j] - rv / 3)); fdi[j] = (fdi[j] + 1) % (int) fdn[j].size(); }
      oL += dlp[0] * (.4 + KF[7] * .5) + (fdl[0] + fdl[2] + fdl[4]) * .35; oR += dlp[1] * (.4 + KF[7] * .5) + (fdl[1] + fdl[3] + fdl[5]) * .35;
      double lvl = std::max (std::fabs (oL), std::fabs (oR)); cmp += (lvl - cmp) * (lvl > cmp ? .003 : .00008);
      double thr = .32, gr = cmp > thr ? std::pow (thr / cmp, M.glue * .65) : 1, gm = gr * M.vol * (1.12 + M.glue * .4);
      { double a = oL * gm, b = oR * gm; if (dtM > .01) { double drv = 1 + dtM * 2.5, sk = 1 / std::sqrt (drv), w = std::min (1.0, dtM * 1.6); a += (std::tanh (a * drv) * sk - a) * w; b += (std::tanh (b * drv) * sk - b) * w; } oL = a * .95; oR = b * .95; }
      double yl = oL - dc[0] + .995 * dc[1]; dc[0] = oL; dc[1] = yl; double yr = oR - dc[2] + .995 * dc[3]; dc[2] = oR; dc[3] = yr;
      if (!(yl == yl && yr == yr)) { yl = yr = 0; for (auto& y : B.L.y) y = 0; Ld.F.reset(); P.F.reset(); P.F2.reset(); sw.reset(); D.rF.reset(); D.sF.reset(); std::fill (dl[0].begin(), dl[0].end(), 0.f); std::fill (dl[1].begin(), dl[1].end(), 0.f); for (auto& F : fdn) std::fill (F.begin(), F.end(), 0.f); for (auto& x : fdl) x = 0; }
      // look-ahead limiter
      float ql = lb[0][li], qr = lb[1][li]; lb[0][li] = (float) yl; lb[1][li] = (float) yr; li = (li + 1) & 63;
      double pk = std::max (std::fabs (yl), std::fabs (yr)); if (pk >= hv || --hcn <= 0) { hv = pk; hcn = 72; } pk = hv; double tg = pk > .88 ? .88 / pk : 1; lg += (tg - lg) * (tg < lg ? .06 : .00012);
      Lo[s] = (float) SLIM (ql * lg); Ro[s] = (float) SLIM (qr * lg); } } };

} // namespace p50
