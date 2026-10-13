// Exact OPT_R over recursive-partition (nearest-representative) Allreduce schemes on a tiny torus.
// f_r(X) = min over set partitions {p} of X of sum_p [col_X(p) + f_{r-1}(p)],
// col_X(p) = sum_{w in X \ p} d(w, p); the single part p = X is the idle round.
// Subset DP, O(4^P) per round: practical for P <= 16.
#include <bits/stdc++.h>
using namespace std;

static const int INF = 1 << 29;

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: exact_small nx ny R [codes_out_prefix]\n");
        return 1;
    }
    int nx = atoi(argv[1]), ny = atoi(argv[2]), R = atoi(argv[3]);
    string prefix = argc > 4 ? argv[4] : "";
    int P = nx * ny;
    if (P > 18) {
        fprintf(stderr, "P must be <= 18\n");
        return 1;
    }
    size_t N = size_t(1) << P;
    uint32_t FULL = uint32_t(N - 1);
    vector<int> d(P * P);
    for (int a = 0; a < P; a++)
        for (int b = 0; b < P; b++) {
            int dx = abs(a % nx - b % nx), dy = abs(a / nx - b / nx);
            d[a * P + b] = min(dx, nx - dx) + min(dy, ny - dy);
        }
    vector<uint8_t> dist(N * P, 100);  // dist[m*P + w] = d(w, m)
    for (uint32_t m = 1; m < N; m++) {
        int lb = __builtin_ctz(m);
        uint32_t prev = m & (m - 1);
        for (int w = 0; w < P; w++) dist[m * P + w] = (uint8_t)min<int>(dist[prev * P + w], d[lb * P + w]);
    }

    vector<vector<int>> f(R + 1, vector<int>(N, INF));
    for (uint32_t m = 1; m < N; m++) f[0][m] = __builtin_popcount(m) == 1 ? 0 : INF;
    vector<int> g(N), h(N), arg(N);

    auto solveSet = [&](int r, uint32_t X, bool keepArg) {
        for (uint32_t p = X; p; p = (p - 1) & X) {
            int fr = f[r - 1][p];
            if (fr >= INF) {
                g[p] = INF;
                continue;
            }
            int c = 0;
            for (uint32_t o = X & ~p; o; o &= o - 1) c += dist[p * P + __builtin_ctz(o)];
            g[p] = c + fr;
        }
        h[0] = 0;
        for (uint32_t Y = (0u - X) & X; Y; Y = (Y - X) & X) {
            uint32_t low = Y & (0u - Y), rest = Y ^ low;
            int best = INF;
            uint32_t barg = 0;
            for (uint32_t s = rest;; s = (s - 1) & rest) {
                int gv = g[s | low];
                if (gv < INF) {
                    int v = gv + h[rest ^ s];
                    if (v < best) best = v, barg = s | low;
                }
                if (s == 0) break;
            }
            h[Y] = best;
            if (keepArg) arg[Y] = barg;
        }
        return h[X];
    };

    auto t0 = chrono::steady_clock::now();
    for (int r = 1; r <= R; r++) {
        for (uint32_t X = 1; X < N; X++) f[r][X] = __builtin_popcount(X) == 1 ? 0 : solveSet(r, X, false);
        double sec = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        printf("torus %dx%d  R=%d  OPT = %d   (%.1f s)\n", nx, ny, r, f[r][FULL], sec);
        fflush(stdout);
    }

    if (!prefix.empty()) {
        for (int Rr = 1; Rr <= R; Rr++) {
            vector<vector<int>> code(P, vector<int>(Rr, 0));
            function<void(int, uint32_t)> recon = [&](int r, uint32_t X) {
                if (__builtin_popcount(X) == 1 || r == 0) return;
                solveSet(r, X, true);
                vector<uint32_t> parts;
                for (uint32_t Y = X; Y; Y ^= arg[Y]) parts.push_back(arg[Y]);
                int t = Rr - r;
                for (size_t k = 0; k < parts.size(); k++)
                    for (uint32_t o = parts[k]; o; o &= o - 1) code[__builtin_ctz(o)][t] = (int)k;
                for (uint32_t p : parts) recon(r - 1, p);
            };
            recon(Rr, FULL);
            ofstream fo(prefix + "_R" + to_string(Rr) + ".txt");
            for (int v = 0; v < P; v++) {
                for (int l = 0; l < Rr; l++) fo << (l ? " " : "") << code[v][l];
                fo << "\n";
            }
        }
    }
    return 0;
}
