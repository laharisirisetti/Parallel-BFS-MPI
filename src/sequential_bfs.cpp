#include <bits/stdc++.h>
using namespace std;

vector<int> runBFS(const vector<vector<int>>& graph, int source)
{
    int V = graph.size();

    vector<int> dist(V, -1);
    queue<int> q;

    dist[source] = 0;
    q.push(source);

    while (!q.empty())
    {
        int node = q.front();
        q.pop();

        for (int child : graph[node])
        {
            if (dist[child] == -1)
            {
                dist[child] = dist[node] + 1;
                q.push(child);
            }
        }
    }

    return dist;
}

void readInputAndBuildGraph(
    int& V,
    int& source,
    vector<vector<int>>& graph)
{
    int E;
    cin >> V >> E >> source;

    graph.resize(V);

    for (int i = 0; i < E; i++)
    {
        int u, v;
        cin >> u >> v;

        graph[u].push_back(v);
    }
}

void printShortestPaths(const vector<int>& dist)
{
    for (int d : dist)
    {
        cout << d << " ";
    }

    cout << endl;
}

int main()
{
    int V;
    int source;
    vector<vector<int>> graph;

    readInputAndBuildGraph(V, source, graph);

    auto _bench_start = chrono::steady_clock::now();
    vector<int> dist = runBFS(graph, source);
    auto _bench_end = chrono::steady_clock::now();
    fprintf(stderr, "TIME %.6f\n", chrono::duration<double>(_bench_end - _bench_start).count());

    printShortestPaths(dist);

    return 0;
}