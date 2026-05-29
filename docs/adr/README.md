# Architecture Decision Records — qm-minimizer

Uma **ADR** (Architecture Decision Record) é um documento curto que registra uma decisão de arquitetura ou design, junto com o contexto que a motivou, as alternativas que foram consideradas e as consequências esperadas. A ideia, popularizada por Michael Nygard, é que decisões importantes não fiquem só na cabeça de quem escreveu o código: ficam versionadas no repositório, datadas, e podem ser revisitadas quando algo mudar. Neste projeto usamos ADRs para deixar explícitas as escolhas que afetam o comportamento e a performance do minimizador Quine-McCluskey — para que quem ler o código depois entenda **por que** ele está do jeito que está, não só o que ele faz.

## Índice

| ADR | Título | Status | Resumo |
|-----|--------|--------|--------|
| [0001](0001-language-choice.md) | Escolha de C++17 como linguagem | Aceita | C++ oferece bit ops nativas, threading sem GIL e STL madura — o sweet spot entre performance de C e produtividade de linguagens gerenciadas para o QM. |
| [0002](0002-architecture-layers.md) | Camadas Reader → IR → Minimizer → Writer | Aceita | Separar I/O (PLA) do algoritmo via interfaces abstratas isola complexidade, facilita testes unitários e abre caminho para outros formatos no futuro. |
| [0003](0003-term-encoding.md) | Codificação de termos como `value` + `mask` | Aceita | Dois `BitVec` por termo reduzem a operação central do QM (diferença em 1 bit) a XOR + AND + popcount, muito mais rápido que arrays de trits ou strings. |
| [0004](0004-threading-model.md) | Paralelismo coarse-grained na Fase A | Aceita | Cada par de grupos adjacentes vira tarefa independente via `std::thread`/`std::async`, sem dependência externa de OpenMP. A Fase B (cobertura) permanece sequencial. |
| [0005](0005-cover-strategy.md) | Estratégia de cobertura: essenciais + guloso | Aceita | Primos essenciais resolvem a maior parte; o restante é coberto por heurística gulosa. Branch-and-Bound fica como trabalho futuro para casos cíclicos. |
| [0006](0006-pla-dialect-scope.md) | Subset do dialeto PLA do Espresso | Aceita | Aceitamos `.i`, `.o=1`, `.p`, `.type {f,fr,fd}`, `.ilb`, `.ob`, `.e`. PLAs multi-output (`.o > 1`) são rejeitados com mensagem clara. |
| [0007](0007-iwls-dont-care-handling.md) | Tratamento de don't-cares em `.type fr` | Aceita | A Fase A opera só sobre on_set + dc_set explícito; don't-cares implícitos (complemento de on ∪ off) não são materializados. Resultado correto, possivelmente subótimo. |

## Convenções

- Notação booleana: `!A` = NOT, `+` = OR, justaposição `AB` ou `A·B` = AND.
- Todas as ADRs seguem o template: Contexto → Decisão → Alternativas → Consequências → Referências.
- Status possíveis: **Proposta**, **Aceita**, **Substituída por NNNN**, **Obsoleta**.
- Uma ADR aceita não é editada para mudar de ideia: cria-se uma nova ADR que a substitui.
