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

int mpiRank;
int worldSize;

constexpr int OFFSET_SIZE_TAG = 0;
constexpr int OFFSET_DATA_TAG = 1;
constexpr int CSR_SIZE_TAG    = 2;
constexpr int CSR_DATA_TAG    = 3;
constexpr int START_TAG       = 4;

void printShortestPaths(const vector<int> &shortestPaths){
      cout << "shortest paths" << endl;
     for (int d : shortestPaths)
     {
          cout << d << " ";
     }
     cout << endl;
}

vector<int> computeDisplacements(const vector<int>& counts)
{
    vector<int> disp(counts.size(),0);

    for(size_t i=1;i<counts.size();i++)
        disp[i]=disp[i-1]+counts[i-1];

    return disp;
}

vector<int> exchange(const vector<vector<int>> &children)
{
     // Prepare sendCounts
     int size = children.size();
     vector<int> sendCounts(size);
     for (int i = 0; i < size; i++)
     {
          sendCounts[i] = children[i].size();
     }

     auto sendDisp = computeDisplacements(sendCounts);

     // Prepare SendBuffer
     vector<int> sendBuffer;
     int total = 0;
     for (auto &v : children)
          total += v.size();
     sendBuffer.reserve(total);
     for (int i = 0; i < size; i++)
     {
          // cout<<"p"<<mpiRank<<" to "<<i<<" ";
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
     auto recvDisp = computeDisplacements(recvCounts);


     // Prepare Receive Buffer
     int recvBuffSize = accumulate(recvCounts.begin(), recvCounts.end(), 0);
     vector<int> recvBuffer(recvBuffSize);

     // Exchange whole data
     MPI_Alltoallv(sendBuffer.data(), sendCounts.data(), sendDisp.data(), MPI_INT, recvBuffer.data(), recvCounts.data(), recvDisp.data(), MPI_INT, MPI_COMM_WORLD);

     // cout<<"p"<<mpiRank<<" receive ";
     // for(int i=0;i<recvBuffSize;i++){
     //      cout<<recvBuffer[i]<<" ";
     // }
     // cout<<endl;

     return recvBuffer;
}

vector<int> runParallelBFS(const GraphData &graphData,const LocalGraph &localGraph){
     vector<bool> visited(graphData.V, false);
     vector<int> dist(graphData.V, -1);
     vector<int> current_frontier;
     int level = 0;
     if (graphData.vertexOwner[graphData.S] == mpiRank)
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
                    if (graphData.vertexOwner[child] == mpiRank)
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
     MPI_Recv(&localOffsetSize, 1, MPI_INT, 0, OFFSET_SIZE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     localGraph.offsets.resize(localOffsetSize);
     MPI_Recv(localGraph.offsets.data(), localOffsetSize, MPI_INT, 0, OFFSET_DATA_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     int localCsrSize;
     MPI_Recv(&localCsrSize, 1, MPI_INT, 0, CSR_SIZE_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     localGraph.csr.resize(localCsrSize);
     MPI_Recv(localGraph.csr.data(), localCsrSize, MPI_INT, 0, CSR_DATA_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
     MPI_Recv(&localGraph.startVertex, 1, MPI_INT, 0, START_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

void sendLocalGraph(const LocalGraph &localGraph, int process){
     int offsetSize = localGraph.offsets.size();
     MPI_Send(&offsetSize, 1, MPI_INT, process, OFFSET_SIZE_TAG, MPI_COMM_WORLD);
     MPI_Send(localGraph.offsets.data(), offsetSize, MPI_INT, process, OFFSET_DATA_TAG, MPI_COMM_WORLD);
     int csrSize = localGraph.csr.size();
     MPI_Send(&csrSize, 1, MPI_INT, process, CSR_SIZE_TAG, MPI_COMM_WORLD);
     MPI_Send(localGraph.csr.data(), csrSize, MPI_INT, process, CSR_DATA_TAG, MPI_COMM_WORLD);
     MPI_Send(&localGraph.startVertex, 1, MPI_INT, process, START_TAG, MPI_COMM_WORLD);
}

void receiveFullGraphData(GraphData &graphData, LocalGraph &localGraph){
     // Boradcast common data
     MPI_Bcast(&graphData.V, 1, MPI_INT, 0, MPI_COMM_WORLD);
     MPI_Bcast(&graphData.S, 1, MPI_INT, 0, MPI_COMM_WORLD);
     if (mpiRank != 0)
          graphData.vertexOwner.resize(graphData.V);
     MPI_Bcast(graphData.vertexOwner.data(), graphData.V, MPI_INT, 0, MPI_COMM_WORLD);

     if (mpiRank != 0)
     {
          receiveLocalGraph(localGraph);
     }
}

LocalGraph buildLocalGraph(const GraphData &graphData, int start, int endd){
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
     return localGraph;
}

// Split vertices among processors (Contiguous block partitioning with the remainder distributed among the first rem processes)
void computeLocalCSRAndSend(GraphData &graphData, LocalGraph &rootLocalGraph)
{
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
          LocalGraph localGraph = buildLocalGraph(graphData, start, endd);
          
          // send offsets and csr
          if (i == 0)
          {
              rootLocalGraph = move(localGraph);
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

void initializeMPI(int& argc, char* argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpiRank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
}

int main(int argc, char *argv[])
{
     initializeMPI(argc, argv);
     GraphData graphData;
     LocalGraph localGraph;
     if (mpiRank == 0)
     {
          readInputAndBuildGraph(graphData);
          computeLocalCSRAndSend(graphData, localGraph);
     }

     receiveFullGraphData(graphData, localGraph);
     vector<int> localDist = runParallelBFS(graphData,localGraph);
     vector<int> final_dist(graphData.V);
     MPI_Reduce(localDist.data(), final_dist.data(), graphData.V, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
     if (mpiRank == 0)
     {
         printShortestPaths(final_dist);
     }

     MPI_Finalize();
}