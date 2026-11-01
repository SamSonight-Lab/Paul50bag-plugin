#include "../Source/Engine.h"
#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>
// state file: lines "K r j v", "E a b c d e", "S ...", "C c0..c7", "CG v", "DR v", "M tempo key scale glue vol ring dirt prog arr", "X g0 g1 g2 g3 rv dl rm fb pa0 pa1 pa2 pb0 pb1 pb2"
int main (int argc, char** argv) {
  p50::Band b; b.prepare (48000);
  std::ifstream f (argv[1]); std::string line;
  while (std::getline (f, line)) { std::istringstream s (line); std::string k; s >> k;
    if (k == "K") { int r, j; double v; s >> r >> j >> v; b.K[r][j] = v; b.KS[r][j] = v; }
    else if (k == "E") for (int i = 0; i < 5; ++i) s >> b.E[i];
    else if (k == "S") for (int i = 0; i < 5; ++i) s >> b.S[i];
    else if (k == "C") for (int i = 0; i < 8; ++i) s >> b.C[i];
    else if (k == "CG") s >> b.CG; else if (k == "DR") s >> b.DR;
    else if (k == "M") s >> b.M.tempo >> b.M.key >> b.M.scale >> b.M.glue >> b.M.vol >> b.M.ring >> b.M.dirt >> b.M.prog >> b.M.arr;
    else if (k == "X") { for (int i = 0; i < 4; ++i) s >> b.X.g[i]; s >> b.X.rv >> b.X.dl >> b.X.rm >> b.X.fb; for (int i = 0; i < 3; ++i) s >> b.X.pa[i]; for (int i = 0; i < 3; ++i) s >> b.X.pb[i]; } }
  double secs = argc > 2 ? atof (argv[2]) : 12; int T = (int) (secs * 48000); std::vector<float> L (128), Rr (128); double s2 = 0, pk = 0; long nan = 0;
  FILE* raw = argc > 3 ? fopen (argv[3], "wb") : nullptr;
  for (int i = 0; i < T / 128; ++i) { b.process (L.data(), Rr.data(), 128); for (int j = 0; j < 128; ++j) { if (!(L[j] == L[j])) nan++; s2 += L[j] * L[j]; pk = std::max (pk, (double) std::fabs (L[j])); if (raw) { fwrite (&L[j], 4, 1, raw); fwrite (&Rr[j], 4, 1, raw); } } }
  if (raw) fclose (raw);
  printf ("{\"rms\":%.3f,\"pk\":%.2f,\"nan\":%ld}\n", std::sqrt (s2 / T), pk, nan); }
