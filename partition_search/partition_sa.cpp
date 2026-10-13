// Simulated annealing over recursive-partition Allreduce schemes on a 2D torus C_nx x C_ny
// (latency model, unbounded ports): minimize the total torus L1 length of all messages.
//
// Scheme with R rounds = hierarchy of partitions V = X_0 > ... > X_R (singletons).
// Round r: every depth-r set X is split into parts; each v in X sends its current value
// to its nearest vertex in every other part of X. Cost of round r in set X:
//   L_X = sum_{parts p} sum_{w in X \ X_p} d(w, X_p).
//
// Encoding: vertex v gets a code (c_0, ..., c_{R-1}); the depth-r set of v is its code prefix
// of length r, its part at round r is c_r. Codes are pairwise distinct <=> depth-R sets are singletons.
// A part equal to the whole set means an idle round for that set.

#include <bits/stdc++.h>
using namespace std;
using ll = long long;
using u128 = unsigned __int128;

static constexpr int RMAX = 16;
using Code = array<uint16_t, RMAX>;

struct Torus {
    int nx, ny, P, W, Dmax;
    vector<uint8_t> D;
    vector<ll> S;              // S[k]: sum of the k smallest distances from a vertex to the others
    vector<vector<int>> near;  // vertices at distance 1..2
    vector<uint64_t> ball;     // ball[(v*(Dmax+1) + t)*W + word]: bitset of vertices within distance t of v
    Torus(int nx_, int ny_) : nx(nx_), ny(ny_), P(nx_ * ny_), D((size_t)nx_ * ny_ * nx_ * ny_) {
        for (int a = 0; a < P; a++)
            for (int b = 0; b < P; b++) {
                int dx = abs(a % nx - b % nx), dy = abs(a / nx - b / nx);
                dx = min(dx, nx - dx);
                dy = min(dy, ny - dy);
                D[(size_t)a * P + b] = (uint8_t)(dx + dy);
            }
        vector<int> ds(D.begin() + 1, D.begin() + P);
        sort(ds.begin(), ds.end());
        S.assign(P, 0);
        for (int k = 1; k < P; k++) S[k] = S[k - 1] + ds[k - 1];
        near.resize(P);
        for (int a = 0; a < P; a++)
            for (int b = 0; b < P; b++) {
                int d = D[(size_t)a * P + b];
                if (d >= 1 && d <= 2) near[a].push_back(b);
            }
        W = (P + 63) / 64;
        Dmax = nx / 2 + ny / 2;
        ball.assign((size_t)P * (Dmax + 1) * W, 0);
        for (int a = 0; a < P; a++)
            for (int b = 0; b < P; b++)
                for (int t = D[(size_t)a * P + b]; t <= Dmax; t++)
                    ball[((size_t)a * (Dmax + 1) + t) * W + b / 64] |= 1ULL << (b % 64);
    }
    int d(int a, int b) const { return D[(size_t)a * P + b]; }
};

struct Params {
    int nx = 8, ny = 8, R = 4;
    ll iters = 2000000;
    int chains = 0, threads = 4;
    uint64_t seed = 1;
    double T0 = -1, T1 = 0.3, t0factor = 1.0;
    double pSwap = 0.30, pMove = 0.35, pTransfer = 0.15, pMerge = 0.10, pSplit = 0.10;
    double pNew = 0.15, pNewDeep = 0.10;
    string out, load, init = "mixed";
    bool check = false, polish = true, enumDigits = false, enumBlocks = false;
};

struct Change {
    int v;
    Code oldc, newc;
};

static int lcpCode(const Code& a, const Code& b, int R) {
    int l = 0;
    while (l < R && a[l] == b[l]) l++;
    return l;
}

struct State {
    const Torus* T = nullptr;
    int R = 0, P = 0, bits = 0, A = 0;
    vector<Code> c;
    vector<u128> key, skey;  // skey[i] = key[ord[i]], ord sorted by key
    vector<int> ord;
    vector<ll> lvl;
    ll total = 0;

    struct Item {
        int r;
        u128 s;
        bool full;
        vector<uint16_t> labels;
    };
    vector<Item> items;
    int nItems = 0;
    vector<ll> oldL, newL;
    mutable vector<int> ps;
    mutable vector<uint64_t> pmask;

    void init(const Torus* t, int R_, const vector<Code>& codes) {
        T = t;
        R = R_;
        P = t->P;
        bits = 1;
        while ((1 << bits) < P) bits++;
        A = P;
        if (bits * R >= 128) throw runtime_error("R too large for this P (need bits*R < 128)");
        c = codes;
        key.assign(P, 0);
        skey.assign(P, 0);
        ord.resize(P);
        iota(ord.begin(), ord.end(), 0);
        for (int v = 0; v < P; v++) {
            for (int l = 0; l < R; l++)
                if (c[v][l] >= A) throw runtime_error("label out of range");
            for (int l = R; l < RMAX; l++) c[v][l] = 0;
            key[v] = pack(c[v], R);
        }
        sort(ord.begin(), ord.end(), [&](int a, int b) { return key[a] < key[b]; });
        for (int i = 0; i < P; i++) skey[i] = key[ord[i]];
        for (int i = 1; i < P; i++)
            if (skey[i] == skey[i - 1]) throw runtime_error("codes are not distinct");
        lvl.assign(R, 0);
        oldL.assign(R, 0);
        newL.assign(R, 0);
        total = 0;
        for (int r = 0; r < R; r++) {
            lvl[r] = levelCostFull(r);
            total += lvl[r];
        }
    }

    u128 pack(const Code& x, int len) const {
        u128 k = 0;
        for (int l = 0; l < len; l++) k = (k << bits) | x[l];
        return k;
    }

    // positions [i, j) in sorted order of vertices whose code prefix of length len equals s
    pair<int, int> range(u128 s, int len) const {
        if (len == 0) return {0, P};
        int sh = bits * (R - len);
        u128 lo = s << sh, hi = (s + 1) << sh;
        int i = int(lower_bound(skey.begin(), skey.end(), lo) - skey.begin());
        int j = int(lower_bound(skey.begin() + i, skey.end(), hi) - skey.begin());
        return {i, j};
    }

    // d(w, part) = smallest t whose ball around w meets the bitset of the part
    ll ballSum(int from, int to) const {
        const int W = T->W, stride = (T->Dmax + 1) * W;
        const uint64_t* m = pmask.data();
        ll s = 0;
        for (int k = from; k < to; k++) {
            const uint64_t* B = T->ball.data() + (size_t)ord[k] * stride;
            int t = 1;
            if (W == 1) {
                while (!(B[t] & m[0])) t++;
            } else {
                for (;; t++) {
                    const uint64_t* Bt = B + (size_t)t * W;
                    bool hit = false;
                    for (int w = 0; w < W && !hit; w++) hit = (Bt[w] & m[w]) != 0;
                    if (hit) break;
                }
            }
            s += t;
        }
        return s;
    }

    // sum over w in [i,j) \ [a,b) of min_{u in [a,b)} d(w,u)
    ll colCost(int i, int j, int a, int b) const {
        ll outside = (j - i) - (b - a);
        if (outside <= 0) return 0;
        if (b - a > 2) {
            pmask.assign(T->W, 0);
            for (int t = a; t < b; t++) pmask[ord[t] / 64] |= 1ULL << (ord[t] % 64);
            return ballSum(i, a) + ballSum(b, j);
        }
        const uint8_t* D = T->D.data();
        ll s = 0;
        for (int k = i; k < j; k++) {
            if (k == a) {
                k = b - 1;
                continue;
            }
            const uint8_t* row = D + (size_t)ord[k] * P;
            int m = 255;
            for (int t = a; t < b; t++) {
                int d = row[ord[t]];
                if (d < m) {
                    m = d;
                    if (m <= 1) break;
                }
            }
            s += m;
        }
        return s;
    }

    void partStarts(int r, int i, int j) const {
        int sh = bits * (R - r - 1);
        ps.clear();
        ps.push_back(i);
        for (int k = i + 1; k < j; k++)
            if ((skey[k] >> sh) != (skey[k - 1] >> sh)) ps.push_back(k);
        ps.push_back(j);
    }

    ll setCostFull(int r, int i, int j) const {
        if (j - i < 2) return 0;
        partStarts(r, i, j);
        if (ps.size() == 2) return 0;
        ll s = 0;
        for (size_t p = 0; p + 1 < ps.size(); p++) s += colCost(i, j, ps[p], ps[p + 1]);
        return s;
    }

    template <class F>
    void forEachSet(int r, F f) const {
        if (r == 0) {
            f(0, P);
            return;
        }
        int sh = bits * (R - r);
        int i = 0;
        while (i < P) {
            u128 s = skey[i] >> sh;
            int j = i + 1;
            while (j < P && (skey[j] >> sh) == s) j++;
            f(i, j);
            i = j;
        }
    }

    ll levelCostFull(int r) const {
        ll s = 0;
        forEachSet(r, [&](int i, int j) { s += setCostFull(r, i, j); });
        return s;
    }

    ll fullCost() const {
        ll s = 0;
        for (int r = 0; r < R; r++) s += levelCostFull(r);
        return s;
    }

    void labelsIn(int i, int j, int l, int excl, vector<uint16_t>& out) const {
        out.clear();
        for (int k = i; k < j; k++) {
            int u = ord[k];
            if (u == excl) continue;
            uint16_t x = c[u][l];
            if (out.empty() || out.back() != x) out.push_back(x);
        }
    }

    void addItem(int r, u128 s, bool full, uint16_t l1, uint16_t l2) {
        auto addLab = [](vector<uint16_t>& v, uint16_t x) {
            if (find(v.begin(), v.end(), x) == v.end()) v.push_back(x);
        };
        for (int k = 0; k < nItems; k++) {
            Item& it = items[k];
            if (it.r == r && it.s == s) {
                if (full)
                    it.full = true;
                else if (!it.full) {
                    addLab(it.labels, l1);
                    addLab(it.labels, l2);
                }
                return;
            }
        }
        if (nItems == (int)items.size()) items.emplace_back();
        Item& it = items[nItems++];
        it.r = r;
        it.s = s;
        it.full = full;
        it.labels.clear();
        if (!full) {
            it.labels.push_back(l1);
            it.labels.push_back(l2);
        }
    }

    // Affected (round, set) pairs: a changed vertex with first differing coordinate l changes its part
    // at round l (partial update of that set), and changes set membership at every round r > l.
    void collect(const vector<Change>& ch) {
        nItems = 0;
        for (const auto& x : ch) {
            int l = lcpCode(x.oldc, x.newc, R);
            if (l >= R) continue;
            addItem(l, pack(x.oldc, l), false, x.oldc[l], x.newc[l]);
            for (int r = l + 1; r < R; r++) {
                addItem(r, pack(x.oldc, r), true, 0, 0);
                addItem(r, pack(x.newc, r), true, 0, 0);
            }
        }
    }

    // In a set whose membership is unchanged, only the columns of parts gaining/losing vertices change.
    void evalItems(vector<ll>& L) const {
        fill(L.begin(), L.end(), 0);
        for (int k = 0; k < nItems; k++) {
            const Item& it = items[k];
            auto [i, j] = range(it.s, it.r);
            if (j - i < 2) continue;
            if (it.full) {
                L[it.r] += setCostFull(it.r, i, j);
            } else {
                for (uint16_t p : it.labels) {
                    auto [a, b] = range((it.s << bits) | p, it.r + 1);
                    if (a < b) L[it.r] += colCost(i, j, a, b);
                }
            }
        }
    }

    void resort() {
        for (int i = 1; i < P; i++) {
            int v = ord[i];
            u128 k = key[v];
            int j = i - 1;
            while (j >= 0 && key[ord[j]] > k) {
                ord[j + 1] = ord[j];
                j--;
            }
            ord[j + 1] = v;
        }
        for (int i = 0; i < P; i++) skey[i] = key[ord[i]];
    }

    ll apply(const vector<Change>& ch) {
        collect(ch);
        evalItems(oldL);
        for (const auto& x : ch) {
            c[x.v] = x.newc;
            key[x.v] = pack(x.newc, R);
        }
        resort();
        evalItems(newL);
        ll d = 0;
        for (int r = 0; r < R; r++) {
            lvl[r] += newL[r] - oldL[r];
            d += newL[r] - oldL[r];
        }
        total += d;
        return d;
    }

    // must directly follow apply(ch)
    void revert(const vector<Change>& ch) {
        for (const auto& x : ch) {
            c[x.v] = x.oldc;
            key[x.v] = pack(x.oldc, R);
        }
        resort();
        ll d = 0;
        for (int r = 0; r < R; r++) {
            lvl[r] -= newL[r] - oldL[r];
            d += newL[r] - oldL[r];
        }
        total -= d;
    }
};

struct Mover {
    State& st;
    const Params& pr;
    mt19937_64 rng;
    vector<uint16_t> la, lb, tmpv;
    vector<char> mark;

    Mover(State& s, const Params& p, uint64_t seed) : st(s), pr(p), rng(seed), mark(s.A, 0) {}
    double U() { return (rng() >> 11) * 0x1.0p-53; }
    int rnd(int n) { return int(rng() % (uint64_t)n); }

    uint16_t fresh(const vector<uint16_t>& used) {
        int nm = 0;
        for (auto x : used)
            if (!mark[x]) {
                mark[x] = 1;
                nm++;
            }
        int k = rnd(st.A - nm);
        uint16_t res = 0;
        for (int x = 0; x < st.A; x++)
            if (!mark[x] && k-- == 0) {
                res = (uint16_t)x;
                break;
            }
        for (auto x : used) mark[x] = 0;
        return res;
    }

    bool genSwap(vector<Change>& ch) {
        int P = st.P;
        int u = rnd(P), v;
        const auto& nb = st.T->near[u];
        if (U() < 0.5 && !nb.empty())
            v = nb[rnd((int)nb.size())];
        else {
            v = rnd(P - 1);
            if (v >= u) v++;
        }
        ch.clear();
        ch.push_back({u, st.c[u], st.c[v]});
        ch.push_back({v, st.c[v], st.c[u]});
        return true;
    }

    // move one vertex to another part at round r, then re-insert it into the deeper hierarchy
    bool genMove(vector<Change>& ch) {
        int R = st.R;
        int v = rnd(st.P), r = rnd(R - 1);
        const Code cv = st.c[v];
        auto [i, j] = st.range(st.pack(cv, r), r);
        st.labelsIn(i, j, r, v, la);
        uint16_t own = cv[r];
        bool ownShared = binary_search(la.begin(), la.end(), own);
        int nOther = (int)la.size() - (ownShared ? 1 : 0);
        Code nc = cv;
        bool makeNew = nOther == 0 || (ownShared && U() < pr.pNew);
        if (makeNew) {
            if (!ownShared) return false;
            nc[r] = fresh(la);
        } else {
            do nc[r] = la[rnd((int)la.size())];
            while (nc[r] == own);
        }
        for (int l = r + 1; l < R; l++) {
            auto [a, b] = st.range(st.pack(nc, l), l);
            st.labelsIn(a, b, l, v, lb);
            if (lb.empty()) {
                for (int l2 = l; l2 < R; l2++) nc[l2] = 0;
                break;
            }
            if (l == R - 1 || U() < pr.pNewDeep) {
                nc[l] = fresh(lb);
                for (int l2 = l + 1; l2 < R; l2++) nc[l2] = 0;
                break;
            }
            nc[l] = lb[rnd((int)lb.size())];
        }
        ch.clear();
        ch.push_back({v, cv, nc});
        return true;
    }

    // move a whole child group (round r+1 part of v) into another part of round r
    bool genTransfer(vector<Change>& ch) {
        int R = st.R;
        int v = rnd(st.P), r = rnd(R - 1);
        const Code cv = st.c[v];
        auto [i, j] = st.range(st.pack(cv, r), r);
        st.labelsIn(i, j, r, -1, la);
        uint16_t own = cv[r];
        auto [pa, pb] = st.range(st.pack(cv, r + 1), r + 1);
        auto [sa, sb] = st.range(st.pack(cv, r + 2), r + 2);
        bool whole = (sa == pa && sb == pb);
        bool makeNew = la.size() < 2 || U() < pr.pNew;
        uint16_t b;
        lb.clear();
        if (makeNew) {
            if (whole) return false;
            b = fresh(la);
        } else {
            do b = la[rnd((int)la.size())];
            while (b == own);
            Code tmp = cv;
            tmp[r] = b;
            auto [ta, tb] = st.range(st.pack(tmp, r + 1), r + 1);
            st.labelsIn(ta, tb, r + 1, -1, lb);
        }
        uint16_t child = cv[r + 1];
        uint16_t nchild = binary_search(lb.begin(), lb.end(), child) ? fresh(lb) : child;
        ch.clear();
        for (int k = sa; k < sb; k++) {
            int u = st.ord[k];
            Code nc = st.c[u];
            nc[r] = b;
            nc[r + 1] = nchild;
            ch.push_back({u, st.c[u], nc});
        }
        return true;
    }

    // merge two parts of round r; children of the absorbed part become children of the other one
    bool genMerge(vector<Change>& ch) {
        int R = st.R;
        int v = rnd(st.P), r = rnd(R - 1);
        const Code cv = st.c[v];
        auto [i, j] = st.range(st.pack(cv, r), r);
        st.labelsIn(i, j, r, -1, la);
        if (la.size() < 2) return false;
        uint16_t a = cv[r], b;
        do b = la[rnd((int)la.size())];
        while (b == a);
        auto [pa, pb] = st.range(st.pack(cv, r + 1), r + 1);
        st.labelsIn(pa, pb, r + 1, -1, lb);
        Code tb = cv;
        tb[r] = b;
        auto [ba, bb] = st.range(st.pack(tb, r + 1), r + 1);
        vector<uint16_t>& used = lb;
        vector<pair<uint16_t, uint16_t>> mp;
        ch.clear();
        for (int k = ba; k < bb; k++) {
            int u = st.ord[k];
            uint16_t x = st.c[u][r + 1];
            if (mp.empty() || mp.back().first != x) {
                uint16_t to = find(used.begin(), used.end(), x) == used.end() ? x : fresh(used);
                used.push_back(to);
                mp.push_back({x, to});
            }
            Code nc = st.c[u];
            nc[r] = a;
            nc[r + 1] = mp.back().second;
            ch.push_back({u, st.c[u], nc});
        }
        return true;
    }

    // split a part of round r: a random subset of its children moves to a new part
    bool genSplit(vector<Change>& ch) {
        int R = st.R;
        int v = rnd(st.P), r = rnd(R - 1);
        const Code cv = st.c[v];
        auto [i, j] = st.range(st.pack(cv, r), r);
        st.labelsIn(i, j, r, -1, la);
        auto [pa, pb] = st.range(st.pack(cv, r + 1), r + 1);
        st.labelsIn(pa, pb, r + 1, -1, lb);
        if (lb.size() < 2) return false;
        int k = 1 + rnd((int)lb.size() - 1);
        shuffle(lb.begin(), lb.end(), rng);
        tmpv.assign(lb.begin(), lb.begin() + k);
        uint16_t b = fresh(la);
        ch.clear();
        for (int t = pa; t < pb; t++) {
            int u = st.ord[t];
            if (find(tmpv.begin(), tmpv.end(), st.c[u][r + 1]) == tmpv.end()) continue;
            Code nc = st.c[u];
            nc[r] = b;
            ch.push_back({u, st.c[u], nc});
        }
        return true;
    }

    bool gen(vector<Change>& ch) {
        if (st.R == 1) return genSwap(ch);
        for (int tries = 0; tries < 20; tries++) {
            double x = U() * (pr.pSwap + pr.pMove + pr.pTransfer + pr.pMerge + pr.pSplit);
            bool ok;
            if ((x -= pr.pSwap) < 0)
                ok = genSwap(ch);
            else if ((x -= pr.pMove) < 0)
                ok = genMove(ch);
            else if ((x -= pr.pTransfer) < 0)
                ok = genTransfer(ch);
            else if ((x -= pr.pMerge) < 0)
                ok = genMerge(ch);
            else
                ok = genSplit(ch);
            if (ok && !ch.empty()) return true;
        }
        return false;
    }
};

static vector<Code> randomInit(int P, int R, mt19937_64& rng) {
    vector<int> rad(R, 1);
    ll prod = 1;
    while (prod < P) {
        int l = int(rng() % R);
        prod = prod / rad[l] * (rad[l] + 1);
        rad[l]++;
    }
    vector<int> perm(P);
    iota(perm.begin(), perm.end(), 0);
    shuffle(perm.begin(), perm.end(), rng);
    vector<Code> c(P);
    for (int i = 0; i < P; i++) {
        c[perm[i]].fill(0);
        int idx = i;
        for (int l = R - 1; l >= 0; l--) {
            c[perm[i]][l] = (uint16_t)(idx % rad[l]);
            idx /= rad[l];
        }
    }
    return c;
}

// Geometry-free lower bound for the class: a round in set X with q parts costs >= |X| * S(q-1).
// f[r][m] = min over hierarchies of depth r on m points of that bound.
static vector<vector<ll>> packingLB(const Torus& T, int R) {
    int P = T.P;
    const ll INF = LLONG_MAX / 4;
    vector<vector<ll>> f(R + 1, vector<ll>(P + 1, INF));
    for (int m = 1; m <= P; m++) f[1][m] = (ll)m * T.S[m - 1];
    for (int r = 2; r <= R; r++) {
        const auto& g = f[r - 1];
        f[r] = g;
        vector<ll> h(g), h2(P + 1);
        for (int q = 2; q <= P; q++) {
            fill(h2.begin(), h2.end(), INF);
            for (int m = q; m <= P; m++)
                for (int a = 1; a <= m - (q - 1); a++)
                    if (h[m - a] < INF) h2[m] = min(h2[m], g[a] + h[m - a]);
            for (int m = q; m <= P; m++)
                if (h2[m] < INF) f[r][m] = min(f[r][m], (ll)m * T.S[q - 1] + h2[m]);
            swap(h, h2);
        }
    }
    return f;
}

using Rounds = vector<vector<array<int, 3>>>;

static Rounds buildMessages(const State& st) {
    Rounds rounds(st.R);
    const Torus& T = *st.T;
    for (int r = 0; r < st.R; r++) {
        st.forEachSet(r, [&](int i, int j) {
            if (j - i < 2) return;
            st.partStarts(r, i, j);
            vector<int> p(st.ps);
            for (size_t q = 0; q + 1 < p.size(); q++) {
                if (p.size() == 2) break;
                for (int k = i; k < j; k++) {
                    if (k >= p[q] && k < p[q + 1]) continue;
                    int w = st.ord[k], best = -1, bd = INT_MAX;
                    for (int t = p[q]; t < p[q + 1]; t++) {
                        int u = st.ord[t], d = T.d(w, u);
                        if (d < bd) bd = d, best = u;
                    }
                    rounds[r].push_back({w, best, bd});
                }
            }
        });
    }
    return rounds;
}

static bool verifyAllreduce(int P, const Rounds& rounds) {
    vector<vector<int>> cnt(P, vector<int>(P, 0));
    for (int v = 0; v < P; v++) cnt[v][v] = 1;
    for (const auto& rd : rounds) {
        auto nw = cnt;
        for (const auto& m : rd)
            for (int w = 0; w < P; w++) nw[m[1]][w] += cnt[m[0]][w];
        cnt.swap(nw);
    }
    for (int v = 0; v < P; v++)
        for (int w = 0; w < P; w++)
            if (cnt[v][w] != 1) return false;
    return true;
}

static void describe(const State& st, ostream& os) {
    const Torus& T = *st.T;
    for (int r = 0; r < st.R; r++) {
        map<string, int> patterns;
        ll lb = 0;
        int nsets = 0;
        st.forEachSet(r, [&](int i, int j) {
            nsets++;
            st.partStarts(r, i, j);
            vector<int> sz;
            for (size_t p = 0; p + 1 < st.ps.size(); p++) sz.push_back(st.ps[p + 1] - st.ps[p]);
            sort(sz.rbegin(), sz.rend());
            lb += (ll)(j - i) * T.S[sz.size() - 1];
            string s = to_string(j - i) + " -> {";
            for (size_t t = 0; t < sz.size(); t++) s += (t ? "," : "") + to_string(sz[t]);
            s += "}";
            patterns[s]++;
        });
        os << "round " << r + 1 << ": cost " << st.lvl[r] << "  (packing bound for this split " << lb << ")\n";
        for (auto& [s, k] : patterns) os << "    " << k << " x  " << s << "\n";
        if (r + 1 < st.R) {
            // label every depth-(r+1) set by order of first appearance in row-major order
            map<u128, int> id;
            os << "  sets after round " << r + 1 << " (row = y, column = x):\n";
            for (int y = 0; y < T.ny; y++) {
                os << "   ";
                for (int x = 0; x < T.nx; x++) {
                    int v = y * T.nx + x;
                    u128 p = st.pack(st.c[v], r + 1);
                    auto it = id.find(p);
                    int k = it == id.end() ? (id[p] = (int)id.size()) : it->second;
                    os << setw(4) << k;
                }
                os << "\n";
            }
        }
    }
}

static void writeJson(const State& st, const Rounds& rounds, const string& path) {
    ofstream f(path);
    f << "{\n  \"nx\": " << st.T->nx << ", \"ny\": " << st.T->ny << ", \"R\": " << st.R
      << ",\n  \"vertex_id\": \"v = y*nx + x\",\n  \"cost\": " << st.total << ",\n  \"round_costs\": [";
    for (int r = 0; r < st.R; r++) f << (r ? ", " : "") << st.lvl[r];
    f << "],\n  \"codes\": [";
    for (int v = 0; v < st.P; v++) {
        f << (v ? ", " : "") << "[";
        for (int l = 0; l < st.R; l++) f << (l ? "," : "") << st.c[v][l];
        f << "]";
    }
    f << "],\n  \"rounds\": [";
    for (size_t r = 0; r < rounds.size(); r++) {
        f << (r ? "," : "") << "\n    [";
        for (size_t k = 0; k < rounds[r].size(); k++)
            f << (k ? "," : "") << "[" << rounds[r][k][0] << "," << rounds[r][k][1] << "," << rounds[r][k][2]
              << "]";
        f << "]";
    }
    f << "\n  ]\n}\n";
}

// Accepts either our JSON (key "codes": [[...], ...]) or plain text with P*R integers.
static vector<Code> loadCodes(const string& path, int P, int R) {
    ifstream f(path);
    if (!f) throw runtime_error("cannot open " + path);
    string s((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    size_t pos = s.find("\"codes\"");
    vector<int> nums;
    if (pos != string::npos) {
        pos = s.find('[', pos);
        int depth = 0;
        for (size_t k = pos; k < s.size(); k++) {
            char ch = s[k];
            if (ch == '[') depth++;
            else if (ch == ']') {
                if (--depth == 0) break;
            } else if (isdigit((unsigned char)ch)) {
                size_t e = k;
                while (e < s.size() && isdigit((unsigned char)s[e])) e++;
                nums.push_back(stoi(s.substr(k, e - k)));
                k = e - 1;
            }
        }
    } else {
        istringstream in(s);
        int x;
        while (in >> x) nums.push_back(x);
    }
    if ((int)nums.size() != P * R)
        throw runtime_error("expected " + to_string(P * R) + " labels, got " + to_string(nums.size()));
    vector<Code> c(P);
    for (int v = 0; v < P; v++) {
        c[v].fill(0);
        for (int l = 0; l < R; l++) c[v][l] = (uint16_t)nums[v * R + l];
    }
    return c;
}

struct Result {
    ll cost = LLONG_MAX;
    vector<Code> codes;
    int chain = -1;
};

static vector<vector<int>> orderedFactorizations(int n) {
    if (n == 1) return {{}};
    vector<vector<int>> res;
    for (int f = 2; f <= n; f++)
        if (n % f == 0)
            for (auto& rest : orderedFactorizations(n / f)) {
                vector<int> v{f};
                v.insert(v.end(), rest.begin(), rest.end());
                res.push_back(v);
            }
    return res;
}

static double digitSchemeCount(const Torus& T, int R) {
    double s = 0;
    for (auto& fx : orderedFactorizations(T.nx))
        for (auto& fy : orderedFactorizations(T.ny)) s += pow((double)R, (double)(fx.size() + fy.size()));
    return s * T.P;
}

// Exhaustive search over "digit" schemes: write x and y in mixed radix (any ordered factorization of
// nx, ny), assign every digit to a round; a round splits by all digits assigned to it. Covers
// recursive halving/multiplying in any digit order, row/column schemes and block schemes.
// blocksOnly: per axis, a higher digit is never split after a lower one (block schemes only).
static Result enumDigits(const Torus& T, int R, string& bestDesc, bool blocksOnly = false) {
    struct Digit {
        int axis, radix, weight;
    };
    Result best;
    ll evaluated = 0;
    State st;
    for (auto& fx : orderedFactorizations(T.nx))
        for (auto& fy : orderedFactorizations(T.ny)) {
            vector<Digit> dg;
            int w = 1;
            for (int a : fx) dg.push_back({0, a, w}), w *= a;
            w = 1;
            for (int a : fy) dg.push_back({1, a, w}), w *= a;
            int nd = (int)dg.size();
            vector<int> asg(nd, 0);
            for (;;) {
                int mx = *max_element(asg.begin(), asg.end());
                vector<char> used(R, 0);
                for (int r : asg) used[r] = 1;
                bool ok = count(used.begin(), used.end(), 1) == mx + 1;
                for (int k = 0; ok && blocksOnly && k + 1 < nd; k++)
                    if (dg[k].axis == dg[k + 1].axis && asg[k] < asg[k + 1]) ok = false;
                if (ok) {
                    vector<Code> codes(T.P);
                    for (int v = 0; v < T.P; v++) {
                        codes[v].fill(0);
                        int coord[2] = {v % T.nx, v / T.nx};
                        for (int r = 0; r < R; r++) {
                            int lab = 0;
                            for (int k = 0; k < nd; k++)
                                if (asg[k] == r) lab = lab * dg[k].radix + (coord[dg[k].axis] / dg[k].weight) % dg[k].radix;
                            codes[v][r] = (uint16_t)lab;
                        }
                    }
                    st.init(&T, R, codes);
                    evaluated++;
                    if (st.total < best.cost) {
                        best.cost = st.total;
                        best.codes = codes;
                        ostringstream os;
                        for (int r = 0; r <= mx; r++) {
                            os << "  round " << r + 1 << ":";
                            for (int k = 0; k < nd; k++)
                                if (asg[k] == r)
                                    os << " " << (dg[k].axis ? 'y' : 'x') << "/" << dg[k].weight << "%" << dg[k].radix;
                            os << "\n";
                        }
                        bestDesc = os.str();
                    }
                }
                int k = 0;
                while (k < nd && ++asg[k] == R) asg[k++] = 0;
                if (k == nd) break;
            }
        }
    printf("%s schemes evaluated: %lld, best L = %lld\n%s", blocksOnly ? "block digit" : "digit", evaluated,
           best.cost, bestDesc.c_str());
    return best;
}

static void swapPolish(State& st) {
    vector<Change> ch(2);
    bool improved = true;
    while (improved) {
        improved = false;
        for (int u = 0; u < st.P; u++)
            for (int v = u + 1; v < st.P; v++) {
                ch[0] = {u, st.c[u], st.c[v]};
                ch[1] = {v, st.c[v], st.c[u]};
                if (st.apply(ch) < 0)
                    improved = true;
                else
                    st.revert(ch);
            }
    }
}

static Result runChain(const Torus& T, const Params& pr, int chainId, const vector<Code>* start) {
    mt19937_64 rng(pr.seed * 1000003ULL + (uint64_t)chainId * 7919ULL + 17);
    State st;
    st.init(&T, pr.R, start ? *start : randomInit(T.P, pr.R, rng));
    Mover mv(st, pr, rng());
    vector<Change> ch;

    double T0 = pr.T0;
    if (T0 <= 0) {
        double sum = 0;
        int cnt = 0;
        for (int k = 0; k < 2000; k++) {
            if (!mv.gen(ch)) continue;
            ll d = st.apply(ch);
            st.revert(ch);
            if (d > 0) sum += d, cnt++;
        }
        T0 = cnt ? pr.t0factor * sum / cnt : 1.0;
    }
    double T1 = min(pr.T1, T0);

    Result best;
    best.cost = st.total;
    best.codes = st.c;
    best.chain = chainId;
    double lr = pr.iters > 0 ? log(T1 / T0) / (double)pr.iters : 0;
    for (ll it = 0; it < pr.iters; it++) {
        double temp = T0 * exp(lr * (double)it);
        if (!mv.gen(ch)) continue;
        ll d = st.apply(ch);
        if (d <= 0 || mv.U() < exp(-(double)d / temp)) {
            if (st.total < best.cost) {
                best.cost = st.total;
                best.codes = st.c;
            }
        } else {
            st.revert(ch);
        }
        if (pr.check && (it & 1023) == 0) {
            ll f = st.fullCost();
            if (f != st.total) {
                fprintf(stderr, "incremental mismatch: %lld vs %lld at it %lld\n", st.total, f, it);
                exit(2);
            }
        }
    }
    if (pr.polish) {
        st.init(&T, pr.R, best.codes);
        swapPolish(st);
        if (st.total < best.cost) {
            best.cost = st.total;
            best.codes = st.c;
        }
    }
    return best;
}

static void usage() {
    fprintf(stderr,
            "usage: partition_sa [--nx 8] [--ny 8] [--R 4] [--iters 2000000] [--chains N] [--threads 4]\n"
            "                    [--seed 1] [--T0 auto] [--T1 0.3] [--t0factor 1] [--load file] [--out best.json]\n"
            "                    [--check] [--no-polish] [--enum-digits] [--enum-blocks] [--init mixed|random|digits]\n"
            "  --load: JSON produced by --out (key \"codes\") or plain text with P*R labels;\n"
            "          with --iters 0 it only evaluates/verifies, otherwise every chain starts from it.\n"
            "  --enum-digits: exhaustive search over mixed-radix digit schemes instead of annealing.\n"
            "  --enum-blocks: the same restricted to block schemes (higher digits split first); used as\n"
            "                 the seed of --init mixed when the full enumeration is too large.\n"
            "  --init mixed: even chains start from the best digit scheme, odd chains from random ones.\n");
}

int main(int argc, char** argv) {
    Params pr;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string {
            if (i + 1 >= argc) {
                usage();
                exit(1);
            }
            return argv[++i];
        };
        if (a == "--nx") pr.nx = stoi(next());
        else if (a == "--ny") pr.ny = stoi(next());
        else if (a == "--n") pr.nx = pr.ny = stoi(next());
        else if (a == "--R") pr.R = stoi(next());
        else if (a == "--iters") pr.iters = (ll)stod(next());
        else if (a == "--chains") pr.chains = stoi(next());
        else if (a == "--threads") pr.threads = stoi(next());
        else if (a == "--seed") pr.seed = stoull(next());
        else if (a == "--T0") { string s = next(); pr.T0 = s == "auto" ? -1 : stod(s); }
        else if (a == "--T1") pr.T1 = stod(next());
        else if (a == "--t0factor") pr.t0factor = stod(next());
        else if (a == "--load") pr.load = next();
        else if (a == "--out") pr.out = next();
        else if (a == "--check") pr.check = true;
        else if (a == "--no-polish") pr.polish = false;
        else if (a == "--enum-digits") pr.enumDigits = true;
        else if (a == "--enum-blocks") pr.enumBlocks = true;
        else if (a == "--init") {
            pr.init = next();
            if (pr.init != "random" && pr.init != "digits" && pr.init != "mixed") {
                usage();
                return 1;
            }
        }
        else {
            usage();
            return 1;
        }
    }
    if (pr.R < 1 || pr.R > RMAX) {
        fprintf(stderr, "R must be in [1, %d]\n", RMAX);
        return 1;
    }
    if (pr.chains <= 0) pr.chains = pr.threads;
    pr.threads = max(1, min(pr.threads, pr.chains));

    Torus T(pr.nx, pr.ny);
    printf("torus %dx%d, P=%d, R=%d\n", pr.nx, pr.ny, T.P, pr.R);
    printf("all-to-all (R=1) cost: %lld\n", (ll)T.P * T.S[T.P - 1]);
    if (T.P <= 300) {
        auto f = packingLB(T, pr.R);
        printf("geometry-free packing lower bound for this class, R=1..%d:", pr.R);
        for (int r = 1; r <= pr.R; r++) printf(" %lld", f[r][T.P]);
        printf("\n");
    }

    vector<Code> start;
    if (!pr.load.empty()) start = loadCodes(pr.load, T.P, pr.R);

    vector<Code> digitStart;
    if (!pr.enumDigits && !pr.enumBlocks && pr.load.empty() && pr.init != "random") {
        string desc;
        bool full = digitSchemeCount(T, pr.R) <= 5e7;
        if (!full) printf("full digit enumeration too large, seeding from block digit schemes\n");
        digitStart = enumDigits(T, pr.R, desc, !full).codes;
    }

    Result best;
    if (pr.enumDigits || pr.enumBlocks) {
        string desc;
        best = enumDigits(T, pr.R, desc, pr.enumBlocks);
    } else if (pr.iters == 0 && !start.empty()) {
        best.cost = 0;
        best.codes = start;
    } else {
        auto t0 = chrono::steady_clock::now();
        atomic<int> nextChain{0};
        mutex mu;
        vector<thread> pool;
        for (int t = 0; t < pr.threads; t++)
            pool.emplace_back([&]() {
                for (;;) {
                    int k = nextChain++;
                    if (k >= pr.chains) break;
                    const vector<Code>* s0 = nullptr;
                    if (!start.empty())
                        s0 = &start;
                    else if (!digitStart.empty() && (pr.init == "digits" || k % 2 == 0))
                        s0 = &digitStart;
                    Result r = runChain(T, pr, k, s0);
                    lock_guard<mutex> g(mu);
                    printf("chain %3d (%s start): L = %lld\n", k, s0 == &digitStart ? "digit" : s0 ? "loaded" : "random",
                           r.cost);
                    fflush(stdout);
                    if (r.cost < best.cost) best = r;
                }
            });
        for (auto& th : pool) th.join();
        double sec = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        printf("%d chains x %lld iters in %.1f s (%.2f M iters/s)\n", pr.chains, pr.iters, sec,
               (double)pr.chains * pr.iters / sec / 1e6);
    }

    State st;
    st.init(&T, pr.R, best.codes);
    if (st.fullCost() != st.total) fprintf(stderr, "cost mismatch!\n");
    Rounds rounds = buildMessages(st);
    ll msgLen = 0;
    size_t nmsg = 0;
    for (auto& rd : rounds)
        for (auto& m : rd) msgLen += m[2], nmsg++;
    bool ok = verifyAllreduce(T.P, rounds);
    printf("\nbest L = %lld (chain %d), round costs:", st.total, best.chain);
    for (int r = 0; r < pr.R; r++) printf(" %lld", st.lvl[r]);
    printf("\nmessages: %zu, total length %lld, Allreduce verification: %s\n\n", nmsg, msgLen, ok ? "OK" : "FAILED");
    describe(st, cout);
    if (!pr.out.empty()) {
        writeJson(st, rounds, pr.out);
        printf("\nwritten %s\n", pr.out.c_str());
    }
    return ok ? 0 : 3;
}
