#include <bits/stdc++.h>
using namespace std;

const int N = 4;                 // nodes: 0=A,1=B,2=C,3=D
const int INF = 1000000;

string name_of(int i){ return string(1, char('A'+i)); }

struct Config {
    bool split_horizon = false;
    bool poison_reverse = false;
    int maxRounds = 10;
};

using Vec = vector<int>;
using Matrix = vector<Vec>;

// Pretty print distances and next hops
void print_tables(const Matrix &D, const vector<vector<int>>& nextHop){
    cout << "       ";
    for(int j=0;j<N;++j) cout << setw(6) << name_of(j);
    cout << "    ";
    for(int j=0;j<N;++j) cout << setw(6) << name_of(j);
    cout << "\n";

    cout << "       ";
    for(int j=0;j<N;++j) cout << setw(6) << "dist";
    cout << "    ";
    for(int j=0;j<N;++j) cout << setw(6) << "nxt";
    cout << "\n";

    for(int i=0;i<N;++i){
        cout << "from " << name_of(i) << ":";
        for(int j=0;j<N;++j){
            if(D[i][j] >= INF) cout << setw(6) << "inf";
            else cout << setw(6) << D[i][j];
        }
        cout << "    ";
        for(int j=0;j<N;++j){
            if(nextHop[i][j] < 0) cout << setw(6) << "-";
            else cout << setw(6) << name_of(nextHop[i][j]);
        }
        cout << "\n";
    }
    cout << "\n";
}

// Run one scenario: converge on full graph, then apply failure(s), then continue and observe
void run_scenario(const string &title, const Matrix &baseCost, const Config &cfg){
    cout << "================ " << title << " =================\n\n";

    Matrix curCost = baseCost;

    // ----- Initialization -----
    Matrix D(N, Vec(N, INF));
    vector<vector<int>> nextHop(N, vector<int>(N, -1));

    for(int x=0;x<N;++x){
        D[x][x] = 0;
        nextHop[x][x] = x;
        for(int y=0;y<N;++y){
            if(x != y && curCost[x][y] < INF){
                D[x][y] = curCost[x][y];
                nextHop[x][y] = y;
            }
        }
    }

    cout << "Initial routing tables (direct costs before iterative rounds):\n";
    print_tables(D, nextHop);

    // ----- Pre-failure convergence -----
    cout << "Running rounds to converge (pre-failure)...\n";
    int round = 0;
    bool changed = true;
    while(changed && round < cfg.maxRounds){
        round++;
        changed = false;

        if (!cfg.split_horizon && !cfg.poison_reverse) {
            // --- PURE DV LOGIC (for no-mitigation scenarios) ---
            Matrix D_prev = D;
            for(int u=0; u<N; ++u){
                for(int dest=0; dest<N; ++dest){
                    if(u == dest) continue;
                    int min_dist = INF;
                    int best_next_hop = -1;
                    vector<int> neighbors;
                    for(int v=0; v<N; ++v) if(v!=u && curCost[u][v] < INF) neighbors.push_back(v);

                    for(int neighbor : neighbors){
                        if(D_prev[neighbor][dest] >= INF) continue;
                        long long path_cost_ll = (long long)curCost[u][neighbor] + (long long)D_prev[neighbor][dest];
                        int path_cost = (path_cost_ll >= INF) ? INF : (int)path_cost_ll;
                        if(path_cost < min_dist){
                            min_dist = path_cost;
                            best_next_hop = neighbor;
                        }
                    }

                    if(D[u][dest] != min_dist || nextHop[u][dest] != best_next_hop){
                        D[u][dest] = min_dist;
                        nextHop[u][dest] = best_next_hop;
                        changed = true;
                    }
                }
            }
        } else {
            // --- ORIGINAL LOGIC (for split horizon/poison reverse) ---
            Matrix Dnext = D;
            vector<vector<int>> nextHopNext = nextHop;
            for(int x=0;x<N;++x){
                vector<int> neighbors;
                for(int v=0; v<N; ++v) if(v!=x && curCost[x][v] < INF) neighbors.push_back(v);
                for(int v: neighbors){
                    Vec adv(N, INF);
                    for(int y=0;y<N;++y){
                        adv[y] = D[x][y];
                        if(cfg.split_horizon && nextHop[x][y] == v) adv[y] = INF;
                        if(cfg.poison_reverse && nextHop[x][y] == v) adv[y] = INF;
                    }
                    for(int y=0;y<N;++y){
                        if(adv[y] >= INF || curCost[v][x] >= INF) continue;
                        long long candidate_ll = (long long)curCost[v][x] + (long long)adv[y];
                        int candidate = (candidate_ll >= INF ? INF : (int)candidate_ll);
                        if(candidate < Dnext[v][y]){
                            Dnext[v][y] = candidate;
                            nextHopNext[v][y] = x;
                            changed = true;
                        }
                    }
                }
            }
            D.swap(Dnext);
            nextHop.swap(nextHopNext);
        }
        cout << "Pre-failure Round " << round << ":\n";
        print_tables(D, nextHop);
    }
    if(!changed) cout << "Pre-failure converged after " << round << " rounds.\n\n";
    else cout << "Pre-failure stopped after max rounds (" << cfg.maxRounds << ").\n\n";

    // ----- Simulate link failure(s) -----
    cout << "Simulating link failure(s): B-D removed\n\n";
    curCost[1][3] = curCost[3][1] = INF; // Remove B-D link

    // ----- Post-failure rounds -----
    cout << "Running rounds after failure to observe propagation and reconvergence...\n";
    int post_round = 0;
    changed = true;
    while(changed && post_round < cfg.maxRounds){
        post_round++;
        changed = false;

        if (!cfg.split_horizon && !cfg.poison_reverse) {
            // --- PURE DV LOGIC (for no-mitigation scenarios) ---
            Matrix D_prev = D;
            for(int u=0; u<N; ++u){
                for(int dest=0; dest<N; ++dest){
                    if(u == dest) continue;
                    int min_dist = INF;
                    int best_next_hop = -1;
                    vector<int> neighbors;
                    for(int v=0; v<N; ++v) if(v!=u && curCost[u][v] < INF) neighbors.push_back(v);

                    for(int neighbor : neighbors){
                        if(D_prev[neighbor][dest] >= INF) continue;
                        long long path_cost_ll = (long long)curCost[u][neighbor] + (long long)D_prev[neighbor][dest];
                        int path_cost = (path_cost_ll >= INF) ? INF : (int)path_cost_ll;
                        if(path_cost < min_dist){
                            min_dist = path_cost;
                            best_next_hop = neighbor;
                        }
                    }

                    if(D[u][dest] != min_dist || nextHop[u][dest] != best_next_hop){
                        D[u][dest] = min_dist;
                        nextHop[u][dest] = best_next_hop;
                        changed = true;
                    }
                }
            }
        } else {
            // --- ORIGINAL LOGIC (for split horizon/poison reverse) ---
            Matrix Dnext = D;
            vector<vector<int>> nextHopNext = nextHop;
            for(int x=0;x<N;++x){
                vector<int> neighbors;
                for(int v=0; v<N; ++v) if(v!=x && curCost[x][v] < INF) neighbors.push_back(v);
                for(int v: neighbors){
                    Vec adv(N, INF);
                    for(int y=0;y<N;++y){
                        adv[y] = D[x][y];
                        if(cfg.split_horizon && nextHop[x][y] == v) adv[y] = INF;
                        if(cfg.poison_reverse && nextHop[x][y] == v) adv[y] = INF;
                    }
                    for(int y=0;y<N;++y){
                        if(adv[y] >= INF || curCost[v][x] >= INF) continue;
                        long long candidate_ll = (long long)curCost[v][x] + (long long)adv[y];
                        int candidate = (candidate_ll >= INF ? INF : (int)candidate_ll);
                        if(candidate < Dnext[v][y]){
                            Dnext[v][y] = candidate;
                            nextHopNext[v][y] = x;
                            changed = true;
                        }
                    }
                }
            }
             for(int u=0; u<N; ++u){
                for(int dest=0; dest<N; ++dest){
                    int nh = nextHopNext[u][dest];
                    if(nh >= 0){
                        if(curCost[u][nh] >= INF){
                            if(Dnext[u][dest] != INF){
                                Dnext[u][dest] = INF;
                                nextHopNext[u][dest] = -1;
                                changed = true;
                            }
                        }
                    }
                }
            }
            D.swap(Dnext);
            nextHop.swap(nextHopNext);
        }
        cout << "Post-failure Round " << post_round << ":\n";
        print_tables(D, nextHop);
    }
    if(!changed) cout << "Post-failure converged after " << post_round << " rounds.\n\n";
    else cout << "Post-failure stopped after max rounds (" << cfg.maxRounds << ").\n\n";
}

int main(){
    Matrix baseCost(N, Vec(N, INF));
    for(int i=0;i<N;++i) baseCost[i][i] = 0;
    baseCost[0][1] = baseCost[1][0] = 1; // A-B
    baseCost[0][2] = baseCost[2][0] = 3; // A-C
    baseCost[1][2] = baseCost[2][1] = 1; // B-C
    baseCost[1][3] = baseCost[3][1] = 1; // B-D
    baseCost[2][3] = baseCost[3][2] = 2; // C-D

    // SCENARIO PLAIN DV (no mitigation)
    Config plain; plain.split_horizon = false; plain.poison_reverse = false;
    run_scenario("Plain Distance-Vector (no mitigation) — fail B-D only", baseCost, plain);

    // SCENARIO Split Horizon
    Config sh; sh.split_horizon = true; sh.poison_reverse = false;
    run_scenario("Split Horizon — fail B-D only", baseCost, sh);

    // SCENARIO Poison Reverse
    Config pr; pr.split_horizon = false; pr.poison_reverse = true;
    run_scenario("Poison Reverse — fail B-D only", baseCost, pr);

    return 0;
}