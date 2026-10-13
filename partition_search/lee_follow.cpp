// Min total length among schemes whose FIRST round is a prescribed coloring,
// with every later round solved by subset DP inside each part (ambient torus L1).
// Parts larger than 16 are reported as size only; R=2 continuation is the exact
// all-to-all inside each part and does not need the DP.
#include <bits/stdc++.h>
using namespace std;

static const int INF = 1 << 29;
static const int LIM = 16;

struct Torus {
    int nx, ny, P;
    vector<int> d;
    Torus(int nx_, int ny_) : nx(nx_), ny(ny_), P(nx_ * ny_), d(P * P) {
        for (int a = 0; a < P; a++)
            for (int b = 0; b < P; b++) {
                int dx = abs(a % nx - b % nx), dy = abs(a / nx - b / nx);
                d[a * P + b] = min(dx, nx - dx) + min(dy, ny - dy);
            }
    }
    int dist(int a, int b) const { return d[a * P + b]; }
};

static long long chamfer(const Torus& T, const vector<vector<int>>& parts) {
    long long s = 0;
    for (size_t i = 0; i < parts.size(); i++)
        for (int v : parts[i])
            for (size_t j = 0; j < parts.size(); j++) if (i != j) {
                int best = INF;
                for (int u : parts[j]) best = min(best, T.dist(v, u));
                s += best;
            }
    return s;
}

// f[r][FULL] = min cost of an r-round hierarchy on this point set.
static vector<int> solveSet(const Torus& T, const vector<int>& ids, int R) {
    int n = (int)ids.size();
    vector<int> out(R + 1, 0);
    if (n <= 1) return out;
    if (n > LIM) {
        for (int r = 0; r <= R; r++) out[r] = -1;
        return out;
    }
    size_t N = size_t(1) << n;
    vector<int> dd(n * n);
    for (int a = 0; a < n; a++)
        for (int b = 0; b < n; b++) dd[a * n + b] = T.dist(ids[a], ids[b]);
    vector<uint8_t> dist(N * n, 250);
    for (uint32_t m = 1; m < N; m++) {
        int lb = __builtin_ctz(m);
        uint32_t prev = m & (m - 1);
        for (int w = 0; w < n; w++)
            dist[m * n + w] = (uint8_t)min<int>(dist[prev * n + w], dd[lb * n + w]);
    }
    vector<vector<int>> f(R + 1, vector<int>(N, INF));
    for (uint32_t m = 1; m < N; m++) f[0][m] = __builtin_popcount(m) == 1 ? 0 : INF;
    vector<int> g(N), h(N);
    auto one = [&](int r, uint32_t X) {
        for (uint32_t p = X; p; p = (p - 1) & X) {
            int fr = f[r - 1][p];
            if (fr >= INF) {
                g[p] = INF;
                continue;
            }
            int c = 0;
            for (uint32_t o = X & ~p; o; o &= o - 1) c += dist[p * n + __builtin_ctz(o)];
            g[p] = c + fr;
        }
        h[0] = 0;
        for (uint32_t Y = (0u - X) & X; Y; Y = (Y - X) & X) {
            uint32_t low = Y & (0u - Y), rest = Y ^ low;
            int best = INF;
            for (uint32_t s = rest;; s = (s - 1) & rest) {
                int gv = g[s | low];
                if (gv < INF) best = min(best, gv + h[rest ^ s]);
                if (s == 0) break;
            }
            h[Y] = best;
        }
        return h[X];
    };
    uint32_t FULL = uint32_t(N - 1);
    for (int r = 1; r <= R; r++) {
        for (uint32_t X = 1; X < N; X++)
            f[r][X] = __builtin_popcount(X) == 1 ? 0 : one(r, X);
        out[r] = f[r][FULL];
    }
    return out;
}

static long long allToAll(const Torus& T, const vector<int>& ids) {
    long long s = 0;
    for (int v : ids)
        for (int u : ids) if (u != v) s += T.dist(v, u);
    return s;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: lee_follow nx ny Rmax [lee|mod3|chess|band]\n");
        return 1;
    }
    int nx = atoi(argv[1]), ny = atoi(argv[2]), Rmax = atoi(argv[3]);
    string kind = argc > 4 ? argv[4] : "lee";
    Torus T(nx, ny);
    vector<vector<int>> parts(max(nx, 5));
    for (int y = 0; y < ny; y++)
        for (int x = 0; x < nx; x++) {
            int lab;
            if (kind == "lee") lab = (2 * x + y) % 5;
            else if (kind == "mod3") lab = (x + y) % 3;
            else if (kind == "chess") lab = (x + y) & 1;
            else if (kind == "band") lab = x < (nx + 1) / 2 ? 0 : 1;
            else {
                fprintf(stderr, "unknown kind %s\n", kind.c_str());
                return 1;
            }
            parts[lab].push_back(y * nx + x);
        }
    parts.erase(remove_if(parts.begin(), parts.end(), [](auto& p) { return p.empty(); }), parts.end());
    long long c1 = chamfer(T, parts);
    printf("torus %dx%d  first=%s  parts=%zu  sizes:", nx, ny, kind.c_str(), parts.size());
    int mx = 0;
    for (auto& p : parts) {
        printf(" %zu", p.size());
        mx = max(mx, (int)p.size());
    }
    printf("  round1=%lld\n", c1);

    vector<vector<int>> sub(parts.size());
    bool exact = mx <= LIM;
    if (exact) {
        for (size_t i = 0; i < parts.size(); i++) sub[i] = solveSet(T, parts[i], Rmax - 1);
    }
    for (int R = 1; R <= Rmax; R++) {
        if (R == 1) {
            printf("  R=%d  L=%lld  (first round only, not finished)\n", R, c1);
            continue;
        }
        if (R == 2 || exact) {
            long long rest = 0;
            if (R == 2 && !exact) {
                for (auto& p : parts) rest += allToAll(T, p);
            } else {
                for (auto& s : sub) rest += s[R - 1];
            }
            printf("  R=%d  L=%lld  (round1 %lld + rest %lld)\n", R, c1 + rest, c1, rest);
        } else {
            printf("  R=%d  part size %d exceeds DP limit\n", R, mx);
        }
    }
    return 0;
}
