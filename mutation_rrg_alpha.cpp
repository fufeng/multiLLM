// mutation_rrg_alpha.cpp
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <queue>
#include <cmath>
#include <stdexcept>
#include <string>
#include <limits>
#include <unordered_set>

static const int EMPTY = -1;  // state for empty sites
static const int B_TYPE = 0;  // B player
static const int A_TYPE = 1;  // A player

// -------------------------
// Basic undirected graph
// -------------------------
struct Graph {
    int N;
    std::vector<std::vector<int>> adj;
    Graph() : N(0) {}
    explicit Graph(int n) : N(n), adj(n) {}
    void add_edge(int u, int v) {
        if (u == v) return;
        if (u < 0 || u >= N || v < 0 || v >= N) return;
        auto &nu = adj[u];
        if (std::find(nu.begin(), nu.end(), v) == nu.end()) nu.push_back(v);
        auto &nv = adj[v];
        if (std::find(nv.begin(), nv.end(), u) == nv.end()) nv.push_back(u);
    }
};

bool is_connected(const Graph &G) {
    if (G.N == 0) return true;
    std::vector<char> vis(G.N, false);
    std::queue<int> q;
    vis[0] = true;
    q.push(0);
    int count = 1;
    while (!q.empty()) {
        int u = q.front(); q.pop();
        for (int v : G.adj[u]) if (!vis[v]) {
            vis[v] = true;
            q.push(v);
            ++count;
        }
    }
    return (count == G.N);
}

// -------------------------
// Connected random regular graph generator
// -------------------------

static inline long long edge_key(int u, int v, int N) {
    if (u > v) std::swap(u, v);
    // unique key for undirected edge (u,v)
    return static_cast<long long>(u) * static_cast<long long>(N) + v;
}

// Build a simple k-regular graph using a configuration-model pairing with restarts.
// Enforces: no self-loops, no multi-edges, and connectedness.
Graph make_random_regular_graph_connected(int N, int k, std::mt19937 &rng, int max_tries = 2000) {
    if (N <= 0) return Graph(0);
    if (k < 0 || k >= N) {
        throw std::runtime_error("k must satisfy 0 <= k < N.");
    }
    if ((static_cast<long long>(N) * k) % 2LL != 0LL) {
        throw std::runtime_error("N*k must be even for a k-regular graph.");
    }
    if (N == 1) {
        if (k != 0) throw std::runtime_error("For N=1, only k=0 is possible.");
        return Graph(1);
    }

    std::vector<int> stubs;
    stubs.reserve(static_cast<size_t>(N) * static_cast<size_t>(k));

    for (int attempt = 0; attempt < max_tries; ++attempt) {
        stubs.clear();
        for (int i = 0; i < N; ++i) {
            for (int t = 0; t < k; ++t) stubs.push_back(i);
        }
        std::shuffle(stubs.begin(), stubs.end(), rng);

        std::unordered_set<long long> E;
        E.reserve(static_cast<size_t>(N) * static_cast<size_t>(k));

        Graph G(N);
        bool bad = false;

        for (size_t i = 0; i < stubs.size(); i += 2) {
            int u = stubs[i];
            int v = stubs[i + 1];
            if (u == v) { bad = true; break; } // self-loop

            long long key = edge_key(u, v, N);
            if (E.find(key) != E.end()) { bad = true; break; } // multi-edge

            E.insert(key);
            G.add_edge(u, v);
        }

        if (bad) continue;

        // Degree check (paranoia)
        bool deg_ok = true;
        for (int i = 0; i < N; ++i) {
            if (static_cast<int>(G.adj[i].size()) != k) { deg_ok = false; break; }
        }
        if (!deg_ok) continue;

        // Connectivity check
        if (!is_connected(G)) continue;

        return G;
    }

    throw std::runtime_error("Failed to generate a connected simple k-regular graph after many retries.");
}

// -------------------------
// Connectivity of active (non-empty) subgraph
// -------------------------

bool active_connected(const Graph &G, const std::vector<int> &state) {
    int N = G.N;
    int start = -1;
    int nonEmptyCount = 0;
    for (int i = 0; i < N; ++i) {
        if (state[i] != EMPTY) {
            ++nonEmptyCount;
            if (start < 0) start = i;
        }
    }
    if (nonEmptyCount < 2) return false;
    std::vector<char> vis(N, false);
    std::queue<int> q;
    vis[start] = true;
    q.push(start);
    int visited = 1;
    while (!q.empty()) {
        int u = q.front(); q.pop();
        for (int v : G.adj[u]) {
            if (state[v] == EMPTY) continue;
            if (!vis[v]) {
                vis[v] = true;
                q.push(v);
                ++visited;
            }
        }
    }
    return (visited == nonEmptyCount);
}

// -------------------------
// Sample empties with connected active subgraph
// More efficient: build a connected active set via spanning-tree-like growth,
// then do local swaps of empty/active nodes while enforcing connectivity.
// -------------------------

void sample_empties_connected(const Graph &G, double pe, std::mt19937 &rng,
                              std::vector<int> &state) {
    int N = G.N;
    if (N == 0) {
        state.clear();
        return;
    }

    // Number of empty sites desired
    int numEmpty = static_cast<int>(std::round(pe * N));
    if (numEmpty < 0) numEmpty = 0;
    if (numEmpty > N - 2) numEmpty = N - 2;  // keep at least 2 active
    int N_active = N - numEmpty;
    if (N_active < 1) N_active = 1;

    // --- Step 1: build an initial connected active set of size N_active ---
    std::vector<char> isActive(N, false);
    std::vector<int> activeList;
    activeList.reserve(N_active);
    std::vector<int> frontier;
    frontier.reserve(N);

    std::uniform_int_distribution<int> distNode(0, N - 1);

    // Start from a random root and grow by adding random neighbors
    int root = distNode(rng);
    isActive[root] = true;
    activeList.push_back(root);
    frontier.push_back(root);

    while ((int)activeList.size() < N_active) {
        if (frontier.empty()) {
            // Restart if somehow stuck (should be extremely rare on connected graphs)
            std::fill(isActive.begin(), isActive.end(), false);
            activeList.clear();
            frontier.clear();

            root = distNode(rng);
            isActive[root] = true;
            activeList.push_back(root);
            frontier.push_back(root);
            continue;
        }

        // Pick a random frontier node
        std::uniform_int_distribution<int> distFront(0, (int)frontier.size() - 1);
        int fi = distFront(rng);
        int u = frontier[fi];

        // Collect neighbors of u that are not yet active
        const auto &nbrs = G.adj[u];
        std::vector<int> candidates;
        candidates.reserve(nbrs.size());
        for (int v : nbrs) {
            if (!isActive[v]) candidates.push_back(v);
        }

        if (candidates.empty()) {
            frontier[fi] = frontier.back();
            frontier.pop_back();
            continue;
        }

        // Add a random inactive neighbor as new active node
        std::uniform_int_distribution<int> distCand(0, (int)candidates.size() - 1);
        int v = candidates[distCand(rng)];
        isActive[v] = true;
        activeList.push_back(v);
        frontier.push_back(v);
    }

    // --- Step 2: initialise state: active = B_TYPE, empty = EMPTY ---
    state.assign(N, EMPTY);
    for (int v : activeList) state[v] = B_TYPE;

    if (numEmpty == 0) return;

    // --- Step 3: swaps to "uniformize" empties while keeping connectivity ---
    int mixSteps = 20 * N;
    for (int step = 0; step < mixSteps; ++step) {
        int a = -1;  // active
        int e = -1;  // empty

        for (int tries = 0; tries < 10 * N && a < 0; ++tries) {
            int cand = distNode(rng);
            if (state[cand] != EMPTY) a = cand;
        }
        for (int tries = 0; tries < 10 * N && e < 0; ++tries) {
            int cand = distNode(rng);
            if (state[cand] == EMPTY) e = cand;
        }
        if (a < 0 || e < 0) continue;

        int old_a = state[a];
        int old_e = state[e];

        state[a] = EMPTY;
        state[e] = B_TYPE;

        if (!active_connected(G, state)) {
            state[a] = old_a;
            state[e] = old_e;
        }
    }
}

// -------------------------
// Single DB update step with empties and alphaA, alphaB
// -------------------------

inline double pair_payoff(int focalType, int neighType,
                          double R, double S, double T, double P,
                          double alphaA, double alphaB) {
    if (focalType == A_TYPE) {
        if (neighType == A_TYPE) return R;
        if (neighType == B_TYPE) return S;
        return alphaA; // EMPTY
    } else if (focalType == B_TYPE) {
        if (neighType == A_TYPE) return T;
        if (neighType == B_TYPE) return P;
        return alphaB; // EMPTY
    }
    return 0.0;
}

void db_update_step_with_empties(const Graph &G,
                                 const std::vector<int> &activeNodes,
                                 std::vector<int> &state,
                                 double beta,
                                 double R,double S,double T,double P,
                                 double alphaA,double alphaB,
                                 double mu,
                                 std::mt19937 &rng,
                                 int &nA,
                                 int &nB) {
    int N_active = static_cast<int>(activeNodes.size());
    if (N_active == 0) return;

    static thread_local std::uniform_real_distribution<double> uni(0.0, 1.0);
    std::uniform_int_distribution<int> distActive(0, N_active - 1);

    int idx = distActive(rng);
    int i = activeNodes[idx];
    int oldType = state[i];
    const auto &neigh = G.adj[i];
    if (neigh.empty()) return;

    std::vector<int> parents;
    parents.reserve(neigh.size());
    std::vector<double> fitness;
    fitness.reserve(neigh.size());
    double sumFit = 0.0;

    for (int j : neigh) {
        int tj = state[j];
        if (tj == EMPTY) continue;

        double pay = 0.0;
        const auto &nbrs = G.adj[j];
        for (int nb : nbrs) {
            int tnb = state[nb];
            pay += pair_payoff(tj, tnb, R, S, T, P, alphaA, alphaB);
        }
        double fit = std::exp(beta * pay);
        parents.push_back(j);
        fitness.push_back(fit);
        sumFit += fit;
    }

    if (parents.empty() || sumFit <= 0.0) return;

    double r = uni(rng) * sumFit;
    double cum = 0.0;
    std::size_t chosenIdx = 0;
    for (; chosenIdx < fitness.size(); ++chosenIdx) {
        cum += fitness[chosenIdx];
        if (r < cum) break;
    }
    if (chosenIdx >= fitness.size()) chosenIdx = fitness.size() - 1;

    int parent = parents[chosenIdx];
    int parentType = state[parent];

    int newType = parentType;
    if (mu > 0.0 && uni(rng) < mu)
        newType = (uni(rng) < 0.5) ? A_TYPE : B_TYPE;

    if (newType != oldType) {
        if (oldType == A_TYPE) --nA; else --nB;
        if (newType == A_TYPE) ++nA; else ++nB;
        state[i] = newType;
    }
}

// -------------------------
// Mutation–selection equilibrium with empties
// -------------------------

double simulate_mutation_equilibrium_with_empties(
    const Graph &G,
    std::vector<int> &state,
    const std::vector<int> &activeNodes,
    double beta,
    double R,double S,double T,double P,
    double alphaA,double alphaB,
    double mu,
    long long burnIn,
    long long sampleSteps,
    std::mt19937 &rng)
{
    int N = G.N;
    if (N == 0) return 0.0;
    int N_active = static_cast<int>(activeNodes.size());
    if (N_active == 0) return 0.0;

    std::uniform_real_distribution<double> uni(0.0, 1.0);

    int nA = 0, nB = 0;
    for (int i : activeNodes) {
        int t = (uni(rng) < 0.5) ? A_TYPE : B_TYPE;
        state[i] = t;
        if (t == A_TYPE) ++nA; else ++nB;
    }

    double sumFracA = 0.0;
    long long samples = 0;

    for (long long t = 0; t < burnIn + sampleSteps; ++t) {
        db_update_step_with_empties(
            G, activeNodes, state,
            beta, R, S, T, P, alphaA, alphaB,
            mu, rng, nA, nB
        );
        if (t >= burnIn) {
            sumFracA += static_cast<double>(nA) / static_cast<double>(N_active);
            ++samples;
        }
    }

    if (samples == 0) return 0.0;
    return sumFracA / static_cast<double>(samples);
}

// -------------------------
// CLI parsing
// -------------------------

struct Params {
    int Ne   = 400;        // number of nodes
    int k    = 4;          // degree for random regular graph
    double pe   = 0.0;     // empty-site fraction
    double beta = 0.005;   // selection intensity
    double mu   = 1e-4;    // mutation probability
    long long burnIn  = 10000000;
    long long sample  = 100000000;
    double u    = 0.4;     // donation-game cost
    double alphaA = 1.0;   // payoff of A vs empty
    double alphaB = 0.0;   // payoff of B vs empty
    unsigned long long seed = 0;    // 0 => random_device
    int nReal = 1;         // number of empties realizations to average over
    int graphTries = 2000; // max retries to generate connected RRG
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "  --Ne INT         number of nodes\n"
              << "  --k INT          degree of random regular graph (default 4)\n"
              << "  --pe DOUBLE      fraction of empty sites\n"
              << "  --beta DOUBLE    selection intensity\n"
              << "  --mu DOUBLE      mutation probability\n"
              << "  --burnin LL      burn-in steps\n"
              << "  --sample LL      sampling steps\n"
              << "  --u DOUBLE       donation-game cost u\n"
              << "  --alphaA DOUBLE  payoff A vs empty\n"
              << "  --alphaB DOUBLE  payoff B vs empty\n"
              << "  --seed ULL       RNG seed (0 => random_device)\n"
              << "  --nReal INT      number of empties realizations (>=1)\n"
              << "  --graphTries INT max retries for connected RRG generation\n";
}

Params parse_args(int argc, char** argv) {
    Params p;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto need = [&](const std::string &name){
            if (i+1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                print_usage(argv[0]);
                std::exit(1);
            }
        };
        if (arg == "--Ne") {
            need(arg);
            p.Ne = std::stoi(argv[++i]);
        } else if (arg == "--k") {
            need(arg);
            p.k = std::stoi(argv[++i]);
        } else if (arg == "--pe") {
            need(arg);
            p.pe = std::stod(argv[++i]);
        } else if (arg == "--beta") {
            need(arg);
            p.beta = std::stod(argv[++i]);
        } else if (arg == "--mu") {
            need(arg);
            p.mu = std::stod(argv[++i]);
        } else if (arg == "--burnin") {
            need(arg);
            p.burnIn = std::stoll(argv[++i]);
        } else if (arg == "--sample") {
            need(arg);
            p.sample = std::stoll(argv[++i]);
        } else if (arg == "--u") {
            need(arg);
            p.u = std::stod(argv[++i]);
        } else if (arg == "--alphaA") {
            need(arg);
            p.alphaA = std::stod(argv[++i]);
        } else if (arg == "--alphaB") {
            need(arg);
            p.alphaB = std::stod(argv[++i]);
        } else if (arg == "--seed") {
            need(arg);
            p.seed = std::stoull(argv[++i]);
        } else if (arg == "--nReal") {
            need(arg);
            p.nReal = std::stoi(argv[++i]);
        } else if (arg == "--graphTries") {
            need(arg);
            p.graphTries = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            std::exit(1);
        }
    }
    return p;
}

// -------------------------
// Main
// -------------------------

int main(int argc, char** argv) {
    Params p = parse_args(argc, argv);

    if (p.Ne < 2) {
        std::cerr << "Error: Ne must be >= 2\n";
        return 1;
    }
    if (p.k < 1 || p.k >= p.Ne) {
        std::cerr << "Error: k must satisfy 1 <= k < Ne\n";
        return 1;
    }
    if ((static_cast<long long>(p.Ne) * p.k) % 2LL != 0LL) {
        std::cerr << "Error: Ne*k must be even\n";
        return 1;
    }
    if (p.pe < 0.0 || p.pe >= 1.0) {
        std::cerr << "Error: pe must satisfy 0 <= pe < 1\n";
        return 1;
    }

    unsigned long long seed = p.seed;
    if (seed == 0) {
        std::random_device rd;
        seed = (static_cast<unsigned long long>(rd()) << 32) ^ rd();
    }
    std::mt19937 rng(seed);

    // Base connected random regular graph
    Graph base_graph(0);
    try {
        base_graph = make_random_regular_graph_connected(p.Ne, p.k, rng, p.graphTries);
    } catch (const std::exception &e) {
        std::cerr << "Graph generation failed: " << e.what() << "\n";
        return 1;
    }

    if (p.nReal < 1) p.nReal = 1;

    std::vector<int> state;
    std::vector<int> activeNodes;

    double sumX  = 0.0;
    double sumX2 = 0.0;

    for (int rep = 0; rep < p.nReal; ++rep) {
        // If you want a fresh graph per realization, uncomment:
        // base_graph = make_random_regular_graph_connected(p.Ne, p.k, rng, p.graphTries);

        sample_empties_connected(base_graph, p.pe, rng, state);

        activeNodes.clear();
        activeNodes.reserve(base_graph.N);
        for (int i = 0; i < base_graph.N; ++i) {
            if (state[i] != EMPTY) activeNodes.push_back(i);
        }

        double R = 1.0;
        double S = -p.u;
        double T = 1.0 + p.u;
        double P = 0.0;

        double xeq = simulate_mutation_equilibrium_with_empties(
            base_graph,
            state,
            activeNodes,
            p.beta,
            R, S, T, P,
            p.alphaA,
            p.alphaB,
            p.mu,
            p.burnIn,
            p.sample,
            rng
        );

        sumX  += xeq;
        sumX2 += xeq * xeq;
    }

    double n     = static_cast<double>(p.nReal);
    double meanX = sumX / n;
    double varX  = sumX2 / n - meanX * meanX;
    if (varX < 0.0) varX = 0.0;
    double sdX   = std::sqrt(varX);

    std::cout << "# Ne=" << p.Ne
              << " k=" << p.k
              << " pe=" << p.pe
              << " beta=" << p.beta
              << " mu=" << p.mu
              << " u=" << p.u
              << " alphaA=" << p.alphaA
              << " alphaB=" << p.alphaB
              << " nReal=" << p.nReal
              << " seed=" << seed
              << "\n";

    std::cout << p.pe << " " << meanX << " " << sdX << "\n";
    return 0;
}
