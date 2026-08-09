#!/usr/bin/env python3
import random
from pathlib import Path
from typing import List, Set, Tuple


ROOT = Path(__file__).resolve().parent
OUTPUT_DIR = ROOT / "test_cases" / "benchmark"
SEED = 42
SOURCE_VERTEX = 0

Edge = Tuple[int, int]


def make_undirected_edge(u: int, v: int) -> Edge:
    return (u, v) if u <= v else (v, u)


def undirected_to_directed(edges: Set[Edge]) -> List[Edge]:
    directed_edges: List[Edge] = []
    for u, v in sorted(edges):
        directed_edges.append((u, v))
        directed_edges.append((v, u))
    return directed_edges


def write_graph(path: Path, n: int, directed_edges: List[Edge], source: int = SOURCE_VERTEX) -> None:
    unique_edges = sorted(set(directed_edges))
    with path.open("w", encoding="utf-8") as handle:
        handle.write(f"{n} {len(unique_edges)} {source}\n")
        for u, v in unique_edges:
            handle.write(f"{u} {v}\n")

    print(f"Generated {path.name}: V={n}, E={len(unique_edges)}")


def generate_connected_random_graph(n: int, target_undirected_edges: int, rng: random.Random) -> Set[Edge]:
    if target_undirected_edges < n - 1:
        raise ValueError("target_undirected_edges must be at least n - 1")

    edges: Set[Edge] = set()

    # Start with a random spanning tree to ensure connectivity.
    for vertex in range(1, n):
        parent = rng.randrange(vertex)
        edges.add(make_undirected_edge(parent, vertex))

    while len(edges) < target_undirected_edges:
        u = rng.randrange(n)
        v = rng.randrange(n)

        if u == v:
            continue

        edges.add(make_undirected_edge(u, v))

    return edges


def generate_skewed_graph(n: int, target_undirected_edges: int, rng: random.Random) -> Set[Edge]:
    if target_undirected_edges < n - 1:
        raise ValueError("target_undirected_edges must be at least n - 1")

    edges: Set[Edge] = set()
    degree = [0] * n

    # Build a preferential-attachment style tree to create hub-like nodes.
    for vertex in range(1, n):
        weights = [degree[prev] + 1 for prev in range(vertex)]
        parent = rng.choices(range(vertex), weights=weights, k=1)[0]
        edge = make_undirected_edge(parent, vertex)
        edges.add(edge)
        degree[parent] += 1
        degree[vertex] += 1

    while len(edges) < target_undirected_edges:
        u = rng.choices(range(n), weights=[degree[i] + 1 for i in range(n)], k=1)[0]
        v = rng.choices(range(n), weights=[degree[i] + 1 for i in range(n)], k=1)[0]

        if u == v:
            continue

        edge = make_undirected_edge(u, v)
        if edge not in edges:
            edges.add(edge)
            degree[u] += 1
            degree[v] += 1

    return edges


def generate_chain(n: int) -> Set[Edge]:
    return {make_undirected_edge(i, i + 1) for i in range(n - 1)}


def generate_star(n: int) -> Set[Edge]:
    return {make_undirected_edge(0, v) for v in range(1, n)}


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    rng = random.Random(SEED)

    # Sparse, connected random graph: good for testing basic traversal cost.
    n = 12000
    target_undirected_edges = 30000
    sparse_edges = generate_connected_random_graph(n, target_undirected_edges, rng)
    write_graph(OUTPUT_DIR / "random_sparse.txt", n, undirected_to_directed(sparse_edges))

    # Medium-density graph: balances communication and work per frontier.
    n = 8000
    target_undirected_edges = 80000
    medium_edges = generate_connected_random_graph(n, target_undirected_edges, rng)
    write_graph(OUTPUT_DIR / "random_medium.txt", n, undirected_to_directed(medium_edges))

    # Skewed-degree graph: useful for stress-testing imbalance and communication.
    n = 10000
    target_undirected_edges = 60000
    skewed_edges = generate_skewed_graph(n, target_undirected_edges, rng)
    write_graph(OUTPUT_DIR / "skewed_powerlaw.txt", n, undirected_to_directed(skewed_edges))

    # Structured baseline: chain is useful for high-depth, low-branching behavior.
    n = 12000
    chain_edges = generate_chain(n)
    write_graph(OUTPUT_DIR / "chain.txt", n, undirected_to_directed(chain_edges))

    # Structured baseline: star stresses hub-heavy work distribution.
    n = 12000
    star_edges = generate_star(n)
    write_graph(OUTPUT_DIR / "star.txt", n, undirected_to_directed(star_edges))

    print("\nAll benchmark graphs generated.")


if __name__ == "__main__":
    main()
