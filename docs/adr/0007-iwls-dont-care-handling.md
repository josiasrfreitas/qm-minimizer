# ADR 0007 — Tratamento de don't-cares em `.type fr`

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

No formato PLA do Espresso com `.type fr` (formato dos benchmarks IWLS 2020), o arquivo lista **explicitamente** o on_set (linhas com saída `1`) e o off_set (linhas com saída `0`). Toda a tupla de entrada que **não** aparece em nenhum dos dois é, por convenção, don't-care **implícito**: a função não está definida ali e o minimizador pode escolher livremente o valor para reduzir a expressão.

Isso é poderoso em teoria — don't-cares aumentam a chance de fundir termos e produzir primos maiores, levando a SOPs menores. O Espresso explora isso. Mas há um problema brutal de escala. Para n=32 já temos 2^32 ≈ 4×10^9 inputs possíveis. Para n=512, são 2^512 ≈ 10^154 inputs — número astronomicamente maior que átomos no universo observável. Os benchmarks IWLS trazem 6400 termos listados (somando on e off); **toda** a vastidão restante é don't-care implícito. Não há como materializar essa lista.

Há duas abordagens possíveis:

1. **Usar implicitamente os don't-cares**: durante a Fase A, considerar que qualquer termo pode "se expandir" para regiões não cobertas pelo off_set. Isso é o que o Espresso faz (com estruturas BDD/cubo-listas sofisticadas). Implementar corretamente exige machinery bem além do escopo atual.
2. **Ignorar don't-cares implícitos**: rodar QM só sobre o on_set (mais o dc_set explícito, se existir). Resultado: cobre o on_set sem violar o off_set, mas pode ser **subótimo** porque não aproveita os DCs para expandir primos.

## Decisão

A Fase A opera **exclusivamente sobre on_set ∪ dc_set_explícito**. Don't-cares implícitos (complemento de on ∪ off) **não** são considerados. O off_set lido do arquivo é usado apenas como invariante de teste em modo debug: nenhum termo do off_set pode ser coberto pela solução.

```cpp
Function fn = reader.read(stream);
// Combina on_set com dc_set explícito como "inputs disponíveis para QM"
std::vector<Term> qm_input = fn.on_set;
qm_input.insert(qm_input.end(), fn.dc_set.begin(), fn.dc_set.end());

auto primes = phase_a(qm_input);

#ifndef NDEBUG
for (const auto& off : fn.off_set)
    assert(!any_covers(primes, off));   // invariante de corretude
#endif

auto solution = cover(primes, fn.on_set);   // cobertura só do on_set
```

**Invariante de corretude.** Por que esse atalho não erra? Toda fusão na Fase A produz um termo cuja imagem (conjunto de minterms cobertos) é a união das imagens dos dois termos fundidos. Como começamos só com termos de on_set (e dc_set, que por definição é "qualquer valor está OK"), todo termo gerado cobre apenas inputs que estavam originalmente no on_set ou no dc_set. Em particular, **nunca** cobre um input do off_set. Logo: a solução é **correta** (cobre tudo do on, evita tudo do off). Apenas pode usar mais primos que o mínimo absoluto, porque não exploramos a liberdade dos DCs implícitos para crescer primos maiores.

## Alternativas consideradas

- **Materializar todos os don't-cares implícitos**: impossível para n ≥ ~25. Descartado por escala.
- **Representar a função em BDD (Binary Decision Diagram)**: caminho que o Espresso e o ABC seguem. Resolve o problema de escala, mas exige reescrever todo o core (não é mais QM tabular). Fora do escopo desta versão.
- **Amostrar don't-cares (heurística)**: escolher k DCs aleatórios para "preencher" o on_set. Aumenta a chance de fusões úteis, mas perde a garantia de não cobrir o off_set sem checagens caras. Custo-benefício ruim.
- **Operar QM sobre on_set ∪ all_dc_implicit, com all_dc_implicit calculado simbolicamente como `!off_set`**: equivalente ao caminho BDD. Mesmo problema de escopo.

## Consequências

**Positivas:**
- Fase A roda sobre no máximo ~6400 termos, viável em tempo razoável mesmo para n=512.
- Corretude garantida pela invariante acima — verificável por `assert` em debug.
- Implementação simples; consistente com QM "de livro".
- Off_set é usado para **validação**, transformando o `.type fr` em verificação extra.

**Limitações:**
- **Solução pode ser subótima** comparada ao que o Espresso produziria. Em alguns casos a falta de DCs implícitos não muda muito; em outros pode aumentar o número de primos finais.
- Não temos métrica direta da distância para o ótimo (precisaria comparar com Espresso/ABC). Sugere-se reportar a contagem de primos da nossa solução vs. a do Espresso quando disponível.
- Migrar para uma abordagem com don't-cares implícitos no futuro provavelmente significará abandonar QM tabular em favor de BDDs ou cube lists — mudança grande de arquitetura.

## Referências

- IWLS 2020 Programming Contest — descrição dos arquivos e da semântica `.type fr`.
- Brayton, R. K. et al. *Logic Minimization Algorithms for VLSI Synthesis*, capítulo sobre uso de don't-cares no Espresso.
- Bryant, R. E. *Graph-Based Algorithms for Boolean Function Manipulation*. IEEE Trans. Computers, 1986 (BDDs — caminho alternativo).
- ADR 0006 (escopo do dialeto PLA), ADR 0003 (representação de termo).
