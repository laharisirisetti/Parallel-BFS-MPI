#include <bits/stdc++.h>
#include <mpi.h>
using namespace std;

struct LocalGraph
{
     int startVertex;
     vector<int> offsets;
     vector<int> csr;
};

struct GraphData
{
     int V;
     int S;
     vector<vector<int>> graph;
     vector<int> vertexOwner;
};

vector<int> exchange(vector<vector<int>> &children)
{
     int rank;
     MPI_Comm_rank(MPI_COMM_WORLD, &rank);

     // Prepare sendCounts
     int size = children.size();
     vector<int> sendCounts(size);
     for (int i = 0; i < size; i++)
     {
          sendCounts[i] = children[i].size();
     }

     // Prepare SendDispacements
     vector<int> sendDisplacements(size, 0);
     for (int i = 1; i < size; i++)
     {
          sendDisplacements[i] = sendDisplacements[i - 1] + sendCounts[i - 1];
     }

     // Prepare SendBuffer
     vector<int> sendBuffer;
     for (int i = 0; i < size; i++)
     {
          // cout<<"p"<<rank<<" to "<<i<<" ";
          for (int j = 0; j < children[i].size(); j++)
          {
               sendBuffer.push_back(children[i][j]);
               // cout<<children[i][j]<<" ";
          }
          // cout<<endl;
     }

     // Exchange counts
     vector<int> recvCounts(size);
     MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT, MPI_COMM_WORLD);

     // Prepare RecvDisplacements
     vector<int> recvDisplacements(size, 0);
     for (int i = 1; i < size; i++)
     {
          recvDisplacements[i] = recvDisplacements[i - 1] + recvCounts[i - 1];
     }

     // Prepare Receive Buffer
     int recvBuffSize = accumulate(recvCounts.begin(), recvCounts.end(), 0);
     vector<int> recvBuffer(recvBuffSize);

     // Exchange whole data
     MPI_Alltoallv(sendBuffer.data(), sendCounts.data(), sendDisplacements.data(), MPI_INT, recvBuffer.data(), recvCounts.data(), recvDisplacements.data(), MPI_INT, MPI_COMM_WORLD);

     // cout<<"p"<<rank<<" receive ";
     // for(int i=0;i<recvBuffSize;i++){
     //      cout<<recvBuffer[i]<<" ";
     // }
     // cout<<endl;

     return recvBuffer;
}

vector<int> runParallelBFS(GraphData &graphData, LocalGraph &localGraph){
     int worldSize, rank;
     MPI_Comm_rank(MPI_COMM_WORLD, &rank);
     MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
     vector<bool> visited(graphData.V, false);
     vector<int> dist(graphData.V, -1);
     vector<int> current_frontier;
     int level = 0;
     if (graphData.vertexOwner[graphData.S] == rank)
     {
          current_frontier.push_back(graphData.S);
          visited[graphData.S] = true;
     }
     int local_curr_nodes_size = current_frontier.size();
     int global_curr_nodes_size;
     MPI_Allreduce(&local_curr_nodes_size, &global_curr_nodes_size, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

     while (global_curr_nodes_size)
     {
          vector<int> next_frontier;
          vector<vector<int>> sendBuffers(worldSize);
          for (int node : current_frontier)
          {
               dist[node] = level;
               int offsetStart = localGraph.offsets[node - localGraph.startVertex];
               int offsetEnd = localGraph.offsets[node - localGraph.startVertex + 1];
               for (int ind = offsetStart; ind < offsetEnd; ind++)
               {
                    int child = localGraph.csr[ind];
                    if (graphData.vertexOwner[child] == rank)
                    {
                         if (!visited[child])
                         {
                              visited[child] = true;
                              next_frontier.push_back(child);
                         }
                    }
                    else
                    {
                         sendBuffers[graphData.vertexOwner[child]].push_back(child);
                    }
               }
          }

          // exchange sendBuffers
          vector<int> recvBuffer = exchange(sendBuffers);
          for (int child : recvBuffer)
          {
               if (!visited[child])
               {
                    visited[child] = true;
                    next_frontier.push_back(child);
               }
          }
          // cout<<"Completed level "<<level<<endl;
          current_frontier = next_frontier;
          local_curr_nodes_size = current_frontier.size();
          MPI_Allreduce(&local_curr_nodes_size, &global_curr_nodes_size, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
          // cout<<"global_size "<<global_curr_nodes_size<<endl;
          level++;
     }
     return dist;
}

void receiveLocalGraph(LocalGraph &localGraph){
     int localOffsetSize;
     MPI_Recv(&localOffsetSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     localGraph.offsets.resize(localOffsetSize);
     MPI_Recv(localGraph.offsets.data(), localOffsetSize, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     int localCsrSize;
     MPI_Recv(&localCsrSize, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     localGraph.csr.resize(localCsrSize);
     MPI_Recv(localGraph.csr.data(), localCsrSize, MPI_INT, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     MPI_Recv(&localGraph.startVertex, 1, MPI_INT, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void sendLocalGraph(LocalGraph &localGraph, int process){
     int offsetSize = localGraph.offsets.size();
     MPI_Send(&offsetSize, 1, MPI_INT, process, 0, MPI_COMM_WORLD);
     MPI_Send(localGraph.offsets.data(), offsetSize, MPI_INT, process, 1, MPI_COMM_WORLD);
     int csrSize = localGraph.csr.size();
     MPI_Send(&csrSize, 1, MPI_INT, process, 2, MPI_COMM_WORLD);
     MPI_Send(localGraph.csr.data(), csrSize, MPI_INT, process, 3, MPI_COMM_WORLD);
     MPI_Send(&localGraph.startVertex, 1, MPI_INT, process, 4, MPI_COMM_WORLD);
}

void receiveFullGraphData(GraphData &graphData, LocalGraph &localGraph){
     int rank;
     MPI_Comm_rank(MPI_COMM_WORLD, &rank);
     // Boradcast common data
     MPI_Bcast(&graphData.V, 1, MPI_INT, 0, MPI_COMM_WORLD);
     MPI_Bcast(&graphData.S, 1, MPI_INT, 0, MPI_COMM_WORLD);
     if (rank != 0)
          graphData.vertexOwner.resize(graphData.V);
     MPI_Bcast(graphData.vertexOwner.data(), graphData.V, MPI_INT, 0, MPI_COMM_WORLD);

     if (rank != 0)
     {
          receiveLocalGraph(localGraph);
     }
}

// Split vertices among processors (Contiguous block partitioning with the remainder distributed among the first rem processes)
void computeLocalCSRAndSend(GraphData &graphData, LocalGraph &rootLocalGraph)
{
     int worldSize;
     MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
     graphData.vertexOwner.resize(graphData.V);
     int base = graphData.V / worldSize;
     int remaining = graphData.V % worldSize;
     int lastEnd = -1;
     for (int i = 0; i < worldSize; i++)
     {
          int start = lastEnd + 1;
          int endd = start + base - 1;
          if (i < remaining)
               endd++;
          lastEnd = endd;
          // cout<<"p"<<i<<" "<<start<<" "<<endd<<endl;
          for (int node = start; node <= endd; node++)
          {
               graphData.vertexOwner[node] = i;
          }

          // compute patrial Compressed Sparse graph for this process
          int paritalV = endd - start + 1;
          LocalGraph localGraph;
          localGraph.offsets.resize(paritalV + 1, 0);
          for (int node = start + 1; node <= endd + 1; node++)
          {
               localGraph.offsets[node - start] = localGraph.offsets[node - start - 1] + graphData.graph[node - 1].size();
          }

          for (int node = start; node <= endd; node++)
          {
               for (int child : graphData.graph[node])
               {
                    localGraph.csr.push_back(child);
               }
          }
          localGraph.startVertex = start;
          // send offsets and csr
          if (i == 0)
          {
               rootLocalGraph.offsets = localGraph.offsets;
               rootLocalGraph.csr = localGraph.csr;
               rootLocalGraph.startVertex = localGraph.startVertex;
          }
          else
          {
               sendLocalGraph(localGraph, i);
          }
     }
}

void readInputAndBuildGraph(GraphData &graphData)
{
     int E;
     cin >> graphData.V >> E >> graphData.S;
     vector<vector<int>> edges(E, vector<int>(2));
     for (int i = 0; i < E; i++)
     {
          cin >> edges[i][0] >> edges[i][1];
     }

     // Build graph
     graphData.graph.resize(graphData.V);
     for (int i = 0; i < E; i++)
     {
          graphData.graph[edges[i][0]].push_back(edges[i][1]);
     }
}

int main(int argc, char *argv[])
{
     MPI_Init(&argc, &argv);
     int worldSize, rank;
     MPI_Comm_rank(MPI_COMM_WORLD, &rank);
     MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

     // read input by root and distribute vertice and graph
     GraphData graphData;
     LocalGraph localGraph;
     if (rank == 0)
     {
          readInputAndBuildGraph(graphData);
          computeLocalCSRAndSend(graphData, localGraph);
     }

     receiveFullGraphData(graphData, localGraph);
     vector<int> localDist = runParallelBFS(graphData,localGraph);
     vector<int> final_dist(graphData.V);
     MPI_Reduce(localDist.data(), final_dist.data(), graphData.V, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
     if (rank == 0)
     {
          cout << "shortest paths" << endl;
          for (int d : final_dist)
          {
               cout << d << " ";
          }
          cout << endl;
     }

     MPI_Finalize();
}