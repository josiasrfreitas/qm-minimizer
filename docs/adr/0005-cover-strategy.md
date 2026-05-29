# ADR 0005 — Estratégia de cobertura: essenciais + guloso

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

A Fase B do QM monta a **tabela de implicantes primos**: linhas são os primos gerados na Fase A, colunas são os minterms da função (apenas do on_set; don't-cares não precisam ser cobertos), célula `(p, m) = 1` se o primo `p` cobre o minterm `m`. O objetivo é escolher o **menor subconjunto** de linhas que cobre todas as colunas. Esse é exatamente o problema de **Set Cover**, NP-difícil. Para os benchmarks IWLS com 6400 minterms e potencialmente milhares de primos, uma busca exaustiva é inviável.

Felizmente, na prática a maior parte da função é resolvida por duas observações clássicas:

1. **Implicantes primos essenciais**: se algum minterm é coberto por apenas **um** primo, esse primo tem que estar na solução. Removemos esses primos da tabela e marcamos os minterms que eles cobrem como já cobertos.
2. **Dominância de linha**: se o conjunto de minterms cobertos pelo primo `p1` é um superconjunto do conjunto coberto por `p2` *e* `p1` tem custo menor ou igual, então `p2` pode ser descartado sem perda.

Depois desses dois passos, o que sobra é o **ciclo**: minterms cobertos por vários primos, sem dominância clara. Aqui escolher ótimo exige Branch-and-Bound (ou redução a SAT/ILP). Para esta primeira versão, aceitamos uma solução possivelmente subótima nesses casos cíclicos, em troca de simplicidade e tempo.

## Decisão

Implementar a Fase B em três passos sequenciais:

```cpp
Solution cover(const std::vector<Term>& primes, const std::vector<Term>& on_set) {
    auto table = build_table(primes, on_set);

    // 1. Essenciais
    auto essentials = pick_essentials(table);
    remove_covered(table, essentials);

    // 2. Dominância de linha (opcional; habilitada por flag)
    if (cfg.use_row_dominance) prune_dominated_rows(table);

    // 3. Guloso para o ciclo restante
    auto rest = greedy_cover(table);

    return merge(essentials, rest);
}
```

A heurística gulosa escolhe iterativamente o primo que cobre mais minterms ainda descobertos, empata por número menor de literais (primos "mais simples" primeiro), e remove os minterms cobertos. Para `n` minterms restantes, o guloso garante razão de aproximação `H(n) ≈ ln(n)` em relação ao ótimo — bom o suficiente para tabelas residuais pequenas, que é o cenário comum após essenciais + dominância.

**Branch-and-Bound não entra nesta versão.** Está catalogado em `docs/future-work.md` como melhoria para casos cíclicos.

## Alternativas consideradas

- **Só guloso (sem essenciais)**: erra em casos onde um primo essencial não é escolhido primeiro pela heurística. Descartado: essenciais são corretos por construção, custam quase nada e melhoram a qualidade.
- **Branch-and-Bound completo**: dá o ótimo, mas exige bookkeeping cuidadoso (limites, poda, ordenação) e arrisca explodir em tempo. Fora de escopo para a primeira entrega.
- **Redução a ILP/SAT com solver externo**: dá o ótimo, mas adiciona dependência (CBC, MiniSat). Queremos a solução self-contained. Descartado.
- **Petrick's method (multiplicação algébrica de cláusulas)**: dá o ótimo, mas o número de termos cresce exponencialmente. Inviável para n grande.

## Consequências

**Positivas:**
- Implementação direta em algumas centenas de linhas.
- Para os PLAs de `data/tests/` (verificação K-map), a etapa de essenciais costuma resolver 100% da cobertura — o guloso nem é acionado.
- Performance previsível: O(|primos| × |minterms|) para construir a tabela; o guloso é O(|primos|² × |minterms|) no pior caso, mas a tabela já reduzida geralmente é pequena.

**Limitações:**
- **Solução pode ser subótima em casos cíclicos.** Documentado nos relatórios: quando a saída tiver mais primos que o esperado, é o guloso operando no ciclo.
- Não há garantia de minimalidade global. Para os benchmarks IWLS isso pode aparecer; reportamos quando ocorrer.
- Branch-and-Bound fica como trabalho futuro — quando entrar, será uma ADR 0008 substituindo a parte gulosa.

## Referências

- McCluskey, E. J. *Minimization of Boolean Functions*. Bell System Technical Journal, 1956.
- Quine, W. V. *The Problem of Simplifying Truth Functions*. American Mathematical Monthly, 1952.
- Brayton, R. K. et al. *Logic Minimization Algorithms for VLSI Synthesis* (Espresso), 1984.
- Cormen et al. *Introduction to Algorithms*, capítulo de Set Cover (análise do guloso).
- ADR 0004 (Fase A paralela), ADR 0003 (representação de primo como `Term`).
