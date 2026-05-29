# ADR 0004 — Paralelismo coarse-grained na Fase A

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

O QM tem duas fases. **Fase A** (geração de implicantes primos): os termos são divididos em grupos por número de bits `1`; em cada iteração, comparam-se pares de grupos adjacentes `(G_k, G_{k+1})` procurando termos fundíveis; os termos fundidos formam a próxima geração; quem não foi fundido vira primo. Iterar até não haver mais fusões. **Fase B** (cobertura): selecionar um subconjunto mínimo de primos que cobre todos os minterms da função.

A Fase A é onde quase todo o tempo é gasto nos benchmarks IWLS. E ela é **embaraçosamente paralela na granularidade de pares de grupos**: comparar `(G_0, G_1)` é totalmente independente de comparar `(G_1, G_2)`, exceto pelo fato de que ambos podem marcar termos de `G_1` como "usado". Mas essa marcação é monotônica (uma vez usado, fica usado) e podemos resolvê-la com `std::atomic<bool>` por termo — ou, mais simples, deixar cada thread coletar suas marcações localmente e mesclar ao final da iteração.

A Fase B (cobertura) é sequencial por natureza: a heurística gulosa (ADR 0005) escolhe o primo que cobre mais minterms ainda descobertos, atualiza o estado, e repete. Paralelizar isso renderia pouco — o gargalo é a Fase A.

Sobre tecnologia de threading: OpenMP seria conveniente (`#pragma omp parallel for`), mas no macOS o Clang da Apple não vem com `libomp`; instalar via Homebrew funciona mas adiciona uma dependência externa que prejudica reprodutibilidade entre ambientes de build. `std::thread` e `std::async` já estão na STL.

## Decisão

Paralelizar **só a Fase A**, com granularidade de "um par de grupos adjacentes por tarefa". Usamos `std::async` com `std::launch::async` e coletamos via `std::future`:

```cpp
std::vector<std::future<PartialResult>> tasks;
for (size_t k = 0; k + 1 < groups.size(); ++k) {
    tasks.push_back(std::async(std::launch::async,
        [&, k]() { return merge_pair(groups[k], groups[k+1]); }));
}
std::vector<Term> next_gen;
for (auto& f : tasks) {
    auto part = f.get();
    next_gen.insert(next_gen.end(), part.fused.begin(), part.fused.end());
    mark_used(groups[part.k],   part.used_left);
    mark_used(groups[part.k+1], part.used_right);
}
```

Após o `join` implícito de todos os `future`s, os termos não marcados como usados viram primos e a próxima iteração começa.

Granularidade: para n=512 há até 513 grupos, mas tipicamente muito menos populados na prática (a maioria dos grupos é vazia ou tem poucos termos). Para evitar lançar centenas de threads ociosas, dividimos os pares em lotes de tamanho `max(1, num_pairs / (2 * hw_concurrency))` — cada thread cuida de várias `merge_pair` em sequência.

## Alternativas consideradas

- **OpenMP (`#pragma omp parallel for`)**: sintaxe mais limpa, mas exige `libomp` no macOS. Reprodutibilidade entre ambientes de build é prioridade. Descartado.
- **Paralelismo fino dentro de `merge_pair` (cada par de termos numa task)**: overhead de criação de tarefa supera o ganho — cada comparação são ~16 ops bit a bit. Descartado por granularidade errada.
- **Paralelismo SIMD manual (AVX2/AVX-512)**: ganho marginal sobre o que o compilador já faz com `__builtin_popcountll`, custo alto de portabilidade (nem toda CPU alvo tem AVX-512). Não compensa nesta primeira versão.
- **Pool de threads escrito à mão**: mais código, sem ganho relevante sobre `std::async` para um número modesto de tarefas por iteração.

## Consequências

**Positivas:**
- Sem dependência externa: compila com `g++ -std=c++17 -pthread` em Linux e macOS.
- Speedup esperado próximo de linear no número de núcleos para benchmarks grandes, onde Fase A domina.
- Fácil desligar: flag `--threads=1` cai no caminho sequencial.

**Limitações:**
- `std::async` com `std::launch::async` cria uma thread por task em algumas implementações — daí o agrupamento em lotes.
- Mesclagem das marcações "usado" após cada iteração custa O(termos) — irrelevante diante do trabalho de comparação.
- Não paralelizamos a Fase B. Se um benchmark futuro tiver Fase B dominante (matriz de cobertura enorme), será preciso revisitar (possivelmente nova ADR).

## Referências

- ISO/IEC 14882:2017, `<future>` e `<thread>`.
- Williams, A. *C++ Concurrency in Action*, 2ª ed. — padrões de uso de `std::async`.
- Documentação do OpenMP 5.0 (descartado, mas referência para comparação).
- ADR 0001 (linguagem e toolchain), ADR 0005 (Fase B sequencial).
