#include <bits/stdc++.h>
#include <mpi.h>

using namespace std;

// ------------------------------------------------------------
// Constants
// ------------------------------------------------------------

constexpr int ROOT_PROCESS = 0;

constexpr int OFFSET_SIZE_TAG = 0;
constexpr int OFFSET_DATA_TAG = 1;
constexpr int CSR_SIZE_TAG = 2;
constexpr int CSR_DATA_TAG = 3;
constexpr int START_VERTEX_TAG = 4;

// ------------------------------------------------------------
// MPI State
// ------------------------------------------------------------

int mpiRank;
int worldSize;

// ------------------------------------------------------------
// Data Structures
// ------------------------------------------------------------

struct LocalGraph
{
    int startVertex;
    vector<int> offsets;
    vector<int> csr;
};

struct GraphData
{
    int vertexCount;
    int sourceVertex;
    vector<vector<int>> graph;
    vector<int> partitionOffsets;
};

// ------------------------------------------------------------
// Utility Functions
// ------------------------------------------------------------

void printShortestPaths(const vector<int>& shortestPaths)
{

    for (int distance : shortestPaths)
    {
        cout << distance << " ";
    }

    cout << endl;
}

vector<int> computeDisplacements(const vector<int>& counts)
{
    vector<int> displacements(counts.size(), 0);

    for (size_t i = 1; i < counts.size(); ++i)
    {
        displacements[i] =
            displacements[i - 1] + counts[i - 1];
    }

    return displacements;
}

int getOwner(
    const vector<int>& partitionOffsets,
    int vertex
)
{
    auto it = upper_bound(
        partitionOffsets.begin(),
        partitionOffsets.end(),
        vertex
    );

    return static_cast<int>(it - partitionOffsets.begin() - 1);
}

// ------------------------------------------------------------
// MPI Communication
// ------------------------------------------------------------

vector<int> exchange(const vector<vector<int>>& children)
{
    const int processCount = static_cast<int>(children.size());

    // Prepare send counts.
    vector<int> sendCounts(processCount);

    for (int i = 0; i < processCount; ++i)
    {
        sendCounts[i] = static_cast<int>(children[i].size());
    }

    // Prepare send displacements.
    const vector<int> sendDisplacements =
        computeDisplacements(sendCounts);

    // Flatten send buffers.
    int totalSendCount = 0;

    for (const auto& buffer : children)
    {
        totalSendCount += static_cast<int>(buffer.size());
    }

    vector<int> sendBuffer;
    sendBuffer.reserve(totalSendCount);

    for (const auto& buffer : children)
    {
        sendBuffer.insert(
            sendBuffer.end(),
            buffer.begin(),
            buffer.end()
        );
    }

    // Exchange counts.
    vector<int> receiveCounts(processCount);

    MPI_Alltoall(
        sendCounts.data(),
        1,
        MPI_INT,
        receiveCounts.data(),
        1,
        MPI_INT,
        MPI_COMM_WORLD
    );

    // Prepare receive displacements.
    const vector<int> receiveDisplacements =
        computeDisplacements(receiveCounts);

    // Prepare receive buffer.
    const int totalReceiveCount =
        accumulate(
            receiveCounts.begin(),
            receiveCounts.end(),
            0
        );

    vector<int> receiveBuffer(totalReceiveCount);

    // Exchange actual data.
    MPI_Alltoallv(
        sendBuffer.data(),
        sendCounts.data(),
        sendDisplacements.data(),
        MPI_INT,
        receiveBuffer.data(),
        receiveCounts.data(),
        receiveDisplacements.data(),
        MPI_INT,
        MPI_COMM_WORLD
    );

    return receiveBuffer;
}

void sendLocalGraph(
    const LocalGraph& localGraph,
    int destinationProcess
)
{
    const int offsetSize =
        static_cast<int>(localGraph.offsets.size());

    MPI_Send(
        &offsetSize,
        1,
        MPI_INT,
        destinationProcess,
        OFFSET_SIZE_TAG,
        MPI_COMM_WORLD
    );

    MPI_Send(
        localGraph.offsets.data(),
        offsetSize,
        MPI_INT,
        destinationProcess,
        OFFSET_DATA_TAG,
        MPI_COMM_WORLD
    );

    const int csrSize =
        static_cast<int>(localGraph.csr.size());

    MPI_Send(
        &csrSize,
        1,
        MPI_INT,
        destinationProcess,
        CSR_SIZE_TAG,
        MPI_COMM_WORLD
    );

    MPI_Send(
        localGraph.csr.data(),
        csrSize,
        MPI_INT,
        destinationProcess,
        CSR_DATA_TAG,
        MPI_COMM_WORLD
    );

    MPI_Send(
        &localGraph.startVertex,
        1,
        MPI_INT,
        destinationProcess,
        START_VERTEX_TAG,
        MPI_COMM_WORLD
    );
}

void receiveLocalGraph(LocalGraph& localGraph)
{
    int offsetSize;

    MPI_Recv(
        &offsetSize,
        1,
        MPI_INT,
        ROOT_PROCESS,
        OFFSET_SIZE_TAG,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );

    localGraph.offsets.resize(offsetSize);

    MPI_Recv(
        localGraph.offsets.data(),
        offsetSize,
        MPI_INT,
        ROOT_PROCESS,
        OFFSET_DATA_TAG,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );

    int csrSize;

    MPI_Recv(
        &csrSize,
        1,
        MPI_INT,
        ROOT_PROCESS,
        CSR_SIZE_TAG,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );

    localGraph.csr.resize(csrSize);

    MPI_Recv(
        localGraph.csr.data(),
        csrSize,
        MPI_INT,
        ROOT_PROCESS,
        CSR_DATA_TAG,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );

    MPI_Recv(
        &localGraph.startVertex,
        1,
        MPI_INT,
        ROOT_PROCESS,
        START_VERTEX_TAG,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );
}

// ------------------------------------------------------------
// Graph Construction
// ------------------------------------------------------------

LocalGraph buildLocalGraph(
    const GraphData& graphData,
    int startVertex,
    int endVertex
)
{
    const int localVertexCount =
        endVertex - startVertex + 1;

    LocalGraph localGraph;

    localGraph.startVertex = startVertex;
    localGraph.offsets.resize(localVertexCount + 1, 0);

    // Build CSR offsets.
    for (int vertex = startVertex;
         vertex <= endVertex;
         ++vertex)
    {
        const int localIndex =
            vertex - startVertex;

        localGraph.offsets[localIndex + 1] =
            localGraph.offsets[localIndex] +
            static_cast<int>(graphData.graph[vertex].size());
    }

    // Build CSR data.
    for (int vertex = startVertex;
         vertex <= endVertex;
         ++vertex)
    {
        for (int child : graphData.graph[vertex])
        {
            localGraph.csr.push_back(child);
        }
    }

    return localGraph;
}

// Split vertices among processors using contiguous block partitioning.
void computeLocalCSRAndSend(
    GraphData& graphData,
    LocalGraph& rootLocalGraph
)
{
    graphData.partitionOffsets.resize(worldSize + 1);

    const int baseVertexCount =
        graphData.vertexCount / worldSize;

    const int remainingVertices =
        graphData.vertexCount % worldSize;

    int startVertex = 0;

    for (int process = 0;
         process < worldSize;
         ++process)
    {
        graphData.partitionOffsets[process] = startVertex;

        const int localVertexCount =
            baseVertexCount +
            (process < remainingVertices ? 1 : 0);

        const int endVertex =
            startVertex + localVertexCount - 1;

        // Build local CSR graph.
        LocalGraph localGraph =
            buildLocalGraph(
                graphData,
                startVertex,
                endVertex
            );

        // Root keeps its own local graph.
        if (process == ROOT_PROCESS)
        {
            rootLocalGraph = move(localGraph);
        }
        else
        {
            sendLocalGraph(localGraph, process);
        }

        startVertex = endVertex + 1;
    }

    graphData.partitionOffsets[worldSize] = graphData.vertexCount;
}

void readInputAndBuildGraph(GraphData& graphData)
{
    int edgeCount;

    cin >> graphData.vertexCount
        >> edgeCount
        >> graphData.sourceVertex;

    vector<pair<int, int>> edges(edgeCount);

    for (auto& [from, to] : edges)
    {
        cin >> from >> to;
    }

    graphData.graph.resize(graphData.vertexCount);

    for (const auto& [from, to] : edges)
    {
        graphData.graph[from].push_back(to);
    }
}

// ------------------------------------------------------------
// Data Distribution
// ------------------------------------------------------------

void receiveFullGraphData(
    GraphData& graphData,
    LocalGraph& localGraph
)
{
    // Root already has its local graph.
    // Other processes receive theirs.
    if (mpiRank != ROOT_PROCESS)
    {
        receiveLocalGraph(localGraph);
    }
    
    // Broadcast common graph information.
    MPI_Bcast(
        &graphData.vertexCount,
        1,
        MPI_INT,
        ROOT_PROCESS,
        MPI_COMM_WORLD
    );

    MPI_Bcast(
        &graphData.sourceVertex,
        1,
        MPI_INT,
        ROOT_PROCESS,
        MPI_COMM_WORLD
    );

    if (mpiRank != ROOT_PROCESS)
    {
        graphData.partitionOffsets.resize(worldSize + 1);
    }

    MPI_Bcast(
        graphData.partitionOffsets.data(),
        worldSize + 1,
        MPI_INT,
        ROOT_PROCESS,
        MPI_COMM_WORLD
    );

}

// ------------------------------------------------------------
// Parallel BFS
// ------------------------------------------------------------

vector<int> runParallelBFS(
    const GraphData& graphData,
    const LocalGraph& localGraph
)
{
    vector<bool> visited(
        graphData.vertexCount,
        false
    );

    vector<int> distance(
        graphData.vertexCount,
        -1
    );

    vector<int> currentFrontier;

    int level = 0;

    // Only the process owning the source starts the BFS.
    if (
        getOwner(
            graphData.partitionOffsets,
            graphData.sourceVertex
        ) == mpiRank
    )
    {
        currentFrontier.push_back(
            graphData.sourceVertex
        );

        visited[graphData.sourceVertex] = true;
    }

    int localFrontierSize =
        static_cast<int>(currentFrontier.size());

    int globalFrontierSize;

    MPI_Allreduce(
        &localFrontierSize,
        &globalFrontierSize,
        1,
        MPI_INT,
        MPI_SUM,
        MPI_COMM_WORLD
    );

    while (globalFrontierSize > 0)
    {
        vector<int> nextFrontier;

        vector<vector<int>> sendBuffers(worldSize);

        // Process current frontier.
        for (int vertex : currentFrontier)
        {
            distance[vertex] = level;

            const int localVertexIndex =
                vertex - localGraph.startVertex;

            const int offsetStart =
                localGraph.offsets[localVertexIndex];

            const int offsetEnd =
                localGraph.offsets[localVertexIndex + 1];

            for (int index = offsetStart;
                 index < offsetEnd;
                 ++index)
            {
                const int child =
                    localGraph.csr[index];

                const int childOwner =
                    getOwner(
                        graphData.partitionOffsets,
                        child
                    );

                // Child belongs to this process.
                if (childOwner == mpiRank)
                {
                    if (!visited[child])
                    {
                        visited[child] = true;
                        nextFrontier.push_back(child);
                    }
                }
                // Child belongs to another process.
                else
                {
                    sendBuffers[childOwner].push_back(child);
                }
            }
        }

        // Exchange newly discovered remote vertices.
        const vector<int> receivedVertices =
            exchange(sendBuffers);

        for (int vertex : receivedVertices)
        {
            if (!visited[vertex])
            {
                visited[vertex] = true;
                nextFrontier.push_back(vertex);
            }
        }

        currentFrontier = move(nextFrontier);

        localFrontierSize =
            static_cast<int>(currentFrontier.size());

        MPI_Allreduce(
            &localFrontierSize,
            &globalFrontierSize,
            1,
            MPI_INT,
            MPI_SUM,
            MPI_COMM_WORLD
        );

        ++level;
    }

    return distance;
}

// ------------------------------------------------------------
// MPI Initialization
// ------------------------------------------------------------

void initializeMPI(
    int& argc,
    char* argv[]
)
{
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(
        MPI_COMM_WORLD,
        &mpiRank
    );

    MPI_Comm_size(
        MPI_COMM_WORLD,
        &worldSize
    );
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main(int argc, char* argv[])
{
    initializeMPI(argc, argv);

    GraphData graphData;
    LocalGraph localGraph;

    // Root reads the graph and distributes local CSR graphs.
    if (mpiRank == ROOT_PROCESS)
    {
        readInputAndBuildGraph(graphData);

        computeLocalCSRAndSend(
            graphData,
            localGraph
        );
    }

    // Receive common graph information and local graph.
    receiveFullGraphData(
        graphData,
        localGraph
    );

    // Run distributed BFS.
    vector<int> localDistance =
        runParallelBFS(
            graphData,
            localGraph
        );

    // Combine distances from all processes.
    vector<int> finalDistance(
        graphData.vertexCount
    );

    MPI_Reduce(
        localDistance.data(),
        finalDistance.data(),
        graphData.vertexCount,
        MPI_INT,
        MPI_MAX,
        ROOT_PROCESS,
        MPI_COMM_WORLD
    );

    if (mpiRank == ROOT_PROCESS)
    {
        printShortestPaths(finalDistance);
    }

    MPI_Finalize();

    return 0;
}