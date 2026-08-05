#include <bits/stdc++.h>
using namespace std;

int main(){
     int V,E;
     cin>>V>>E;
     vector<vector<int>> edges(E, vector<int> (3));
     for(int i=0;i<E;i++){
          cin>>edges[i][0]>>edges[i][1]>>edges[i][2];
     }
     int k;
     cin>>k;
     vector<int> explorers(k);
     for(int i=0;i<k;i++){
          cin>>explorers[i];
     }
     int R;
     cin>>R;
     int L;
     cin>>L;
     vector<bool> isBlockedChamber(V);
     for(int i=0;i<L;i++){
          int node;
          cin>>node;
          isBlockedChamber[node] = true;
     }

     vector<vector<int>> graph(V);
     for(int i=0;i<E;i++){
          int u = edges[i][0], v = edges[i][1], d = edges[i][2];
          graph[u].push_back(v);
          if(d) graph[v].push_back(u);
     }

     vector<int> dist(V,-1);
     vector<bool> visited(V, false);
     queue<int> q;
     dist[R] = 0;
     visited[R] = true;
     q.push(R);

     while(!q.empty()){
          int node = q.front();
          q.pop();
          for(int i=0;i<graph[node].size();i++){
               int child = graph[node][i];
               if(!visited[child] && !isBlockedChamber[child]){
                    visited[child] = true;
                    dist[child] = dist[node] + 1;
                    q.push(child);
               }
          }
     }
     for(int i=0;i<k;i++){
          cout<<dist[explorers[i]]<<" ";
     }
     cout<<endl;
}