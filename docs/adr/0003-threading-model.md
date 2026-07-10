# ADR 0003 — Fase A paralela por par de grupos de Hamming

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

Fase A é O(M²) onde M é o número de termos vivos. Fase B (cobertura) é tipicamente muito menor. Faz sentido paralelizar a primeira, com granularidade que não force locks.

## Decisão

Cada par adjacente `(grupo_k, grupo_{k+1})` (agrupados por peso de Hamming) é uma unidade independente. Despachadas com `std::async`:

```cpp
for (const auto& wu : work_units)
    futures.push_back(std::async(std::launch::async, fuse_pair, ...));
for (auto& f : futures) merge(f.get());
```

Cada tarefa acumula resultados locais; o agregador funde sequencialmente. Sem `std::mutex` no hot path.

Fase B fica single-thread — escolhas dependem umas das outras e o ganho não compensa a complexidade.

## Consequências

- Speedup limitado ao número de pares ativos (30–50 em casos grandes). Cobre máquinas com até ~16 threads.
- Não usa OpenMP — `libomp` não vem com Apple Clang por padrão, e a dependência não compensa.
- Se Fase B virar gargalo, paralelizar o loop guloso é uma extensão local — não muda o resto.
