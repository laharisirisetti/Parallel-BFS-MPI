# Parallel BFS: Design Notes

## The problem

BFS from a single source is naturally serial: you finish one level before you know
the next. The work per vertex is tiny (look at its neighbours, mark the unvisited
ones), so the only way to go faster is to spread the vertices across processes and
have each expand its own slice of every level in parallel. The whole design really
comes down to one question: when a process expands a vertex and finds a neighbour it
does not own, how does that neighbour reach the process that does, without anyone
deadlocking?

## Splitting the work: vertices, not edges

I split the **vertices** across processes. Each process owns a contiguous range of
vertex ids and stores only those vertices' adjacency, in CSR form. Ownership is a
pure function of the vertex id and the partition boundaries, so **every process can
work out who owns any vertex** without asking anyone. That property is what makes
routing a discovered neighbour cheap: the process that finds it already knows where
to send it.

An equal split by vertex count is the obvious first cut, but it ignores that BFS work
follows **edges**, not vertices. A few high-degree hub vertices can dump far more work
on one process than another. So I size the ranges by cumulative out-degree: walk a
prefix sum of degrees and cut where each process gets roughly the same number of
edges. The ranges stay contiguous, so ownership is still a simple range check, but the
edge load is balanced.

Root reads the graph, computes the partition, and hands out the pieces: the partition
boundaries, the vertex count and the source vertex are broadcast to everyone (so any
process can answer "who owns v"), while each process's own CSR slice is delivered with
point-to-point sends.

## Global termination: why local completion deadlocks

The subtle part is deciding when BFS is finished. My first instinct was to let each
process stop once it had visited all of its own vertices, or once its own frontier went
empty. Both are wrong, and in the same way. BFS is **level-synchronous**: every process
has to move through the levels together. If one process finishes its local work early
and leaves the loop, it stops taking part in the per-level exchange. But another
process, a level or two behind, may still discover a neighbour that the early process
owns, and now there is no one to receive it. The whole run hangs.

So termination has to be **global**, not local. At the end of each level every process
contributes its next-frontier size to an `MPI_Allreduce` (sum), so everyone ends up with
the same global frontier size, and the loop continues while that is greater than zero.
Everyone enters and leaves each level together, and no process is ever left waiting on a
peer that already quit.

## Distance from a level counter, not the parent

In an ordinary BFS you set a child's distance to the parent's distance plus one. That
does not work cleanly here, because a child can be discovered by a process that does not
own the parent, so it would have to ask the parent's owner for the parent's distance,
adding a round of communication for something we already know. Instead each process
keeps a local `level` counter that advances in step with the global levels. Any vertex
first reached during level *k* simply gets distance *k*. No parent lookup, no extra
messages.

## The frontier exchange

Within a level, each process walks its current frontier. For every neighbour: if the
process owns it, it checks visited and, if the neighbour is new, pushes it into the next
frontier locally. If it does not own it, the neighbour goes into a per-owner bucket to be
shipped out.

The hard part is the receive side. A process does not know in advance whether anyone will
send to it, or how much, so if it just posts blind receives it can hang. The naive fix is
to have everyone send their full bucket to everyone; then each process expects exactly
`P - 1` messages, but it pays `P - 1` sends and receives every level no matter how little
data actually moves.

Instead the exchange happens in two collective steps. First an `MPI_Alltoall` swaps the
per-peer counts, so every process learns exactly how many vertices each peer is sending it
(zero for most peers on a typical level). With those counts in hand it can size its receive
buffer and displacements, and a single `MPI_Alltoallv` then moves all the variable-length
payloads at once. There is no manual matching and no guessing who will send, and because
the exchange is collective it also lines every process up at the level boundary, so no
separate barrier is needed.

After the exchange each process merges the vertices it received, skipping any already
visited, into its next frontier. Then the frontier-size `MPI_Allreduce` decides whether the
loop continues.

## The full step

1. Root reads the graph.
2. Partition vertices by cumulative out-degree; broadcast the boundaries.
3. Distribute each process's CSR slice.
4. The process owning the source seeds the frontier at distance 0.
5. While the global frontier is non-empty:
   - expand the local frontier, assigning the current level as distance;
   - bucket non-local neighbours by owner;
   - exchange counts, then payloads (`MPI_Alltoall` + `MPI_Alltoallv`);
   - merge received vertices into the next frontier;
   - `MPI_Allreduce` the frontier size to decide whether to continue.
6. Gather local distance arrays to root (`MPI_Gatherv`).
7. Root assembles the full distance vector and prints it.

## What we get, and where it breaks down

Running on a single process reproduces the sequential result, which doubles as a
correctness check. Past that, the honest picture is that this design only pays off in a
fairly narrow regime, and the benchmarks bear that out:

- **BFS work per level is tiny.** A level often touches only a handful of vertices, so
  there is very little compute to set against a fixed per-level communication cost. On
  small graphs the traversal finishes in microseconds and the collectives dominate.
- **The cost scales with depth.** Every level is three global collectives (`Alltoall`,
  `Alltoallv`, `Allreduce`). A high-diameter graph like a long chain has as many levels
  as vertices, so the communication piles up and adding processes makes it strictly worse.
- **It helps when there is enough width.** On a large, low-diameter graph, where each
  level expands a wide frontier, the per-level compute finally outweighs the exchange and
  real speedup appears.

None of this is a correctness problem; it is the shape of level-synchronous BFS on
commodity hardware. The place it would genuinely shine is much larger graphs on real
distributed memory, ideally with communication overlapped with computation.

## Approaches not taken

- **Splitting edges instead of vertices** would balance load more directly but scatters
  each vertex's adjacency across processes, so expanding a single vertex would need
  communication. Owning whole vertices keeps expansion local.
- **A 2-D partition** (the Graph500 approach) cuts the communication volume of the
  frontier exchange and is the right answer at very large scale, but it is a lot more
  bookkeeping than this problem needed in order to get correct first.
- **Direction-optimizing (push/pull) BFS** would help the wide-frontier levels by
  switching to a pull step when the frontier is large. A clear future improvement, and
  orthogonal to the partition choice.
