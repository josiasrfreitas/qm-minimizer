# ADR 0002 — Camadas Reader → IR → Minimizer → Writer

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

O minimizador precisa: (1) ler PLAs do Espresso, (2) executar o QM, (3) escrever o resultado de volta em PLA (ou SOP textual para verificação humana). Um caminho tentador é abrir o arquivo direto no `main()`, parsear cabeçalho e linhas, e já começar a popular as listas de minterms. Funciona, mas mistura três responsabilidades distintas: parsing de texto, álgebra do QM, e formatação de saída. Cada uma evolui por motivos diferentes.

Os arquivos em `data/tests/` são pequenos (3 a 6 variáveis, verificação manual por K-map). Os de `data/benchmark/` e `data/validation/` são grandes (até n=512, 6400 termos, `.type fr`). Se o parser estiver enroscado no algoritmo, qualquer ajuste no formato de entrada (por exemplo, no futuro aceitar BLIF ou JSON) força reescrever também a lógica de minimização. E, mais imediato: testes unitários do QM ficam dependentes de I/O em disco, que é o oposto do que se quer.

A separação clássica em camadas — adaptador de entrada → representação intermediária → core algorítmico → adaptador de saída — resolve isso. A representação intermediária (IR) é uma estrutura `Function` que carrega `num_inputs`, `on_set`, `dc_set` e nomes opcionais de variáveis. O Minimizer consome `Function`, produz `Solution` (lista de implicantes primos cobrindo a função). O Writer consome `Solution`.

## Decisão

Definir quatro camadas com interfaces abstratas:

```cpp
struct Function {
    int num_inputs;
    std::vector<std::string> input_labels;   // opcional
    std::vector<Term> on_set;
    std::vector<Term> dc_set;
};

struct Solution {
    std::vector<Term> prime_implicants;      // cobertura final
    Stats stats;                             // tempo, #primos, etc.
};

class IReader  { public: virtual Function read(std::istream&) = 0; };
class IMinimizer { public: virtual Solution minimize(const Function&) = 0; };
class IWriter  { public: virtual void write(std::ostream&, const Solution&) = 0; };
```

Concretamente teremos `PlaReader`, `QmMinimizer`, `PlaWriter` e `SopTextWriter`. O `main()` é um orquestrador fino: parseia CLI, escolhe um Reader e um Writer, chama `minimize()`.

## Alternativas consideradas

- **Tudo no `main()`**: mais rápido de escrever, mas inviabiliza testes unitários do core. Cada ajuste no PLA quebra o algoritmo. Descartado.
- **Camadas sem interfaces abstratas (só funções livres)**: funciona, mas dificulta substituir o Reader em testes (não dá para injetar um `MockReader` que produz `Function` em memória). Descartado por testabilidade.
- **Pipeline streaming (linha a linha do PLA já alimentando o QM)**: economiza memória, mas o QM precisa da lista completa de minterms para começar a Fase A. Otimização prematura sem benefício real.

## Consequências

**Positivas:**
- Testes unitários do `QmMinimizer` recebem `Function` montadas à mão, sem tocar disco.
- Adicionar BLIF ou JSON no futuro é só implementar `IReader`/`IWriter`; o core não muda.
- `SopTextWriter` (útil para verificar os PLAs pequenos de `data/tests/` contra K-map manual) coexiste com `PlaWriter` sem duplicação.
- Bugs ficam localizados: se a saída está errada, o problema é num dos três pontos bem definidos.

**Limitações:**
- Indireção via `virtual` adiciona overhead microscópico (~1 chamada virtual por arquivo lido/escrito) — irrelevante; o gargalo é a Fase A do QM.
- Mais arquivos no projeto. Compensado por organização clara em `src/io/`, `src/core/`, `src/cli/`.

## Referências

- Hunt, A. & Thomas, D. *The Pragmatic Programmer*, capítulo sobre desacoplamento e ortogonalidade.
- Documentação do Espresso (Berkeley) sobre o formato PLA — descreve o que o `PlaReader` precisa aceitar.
- ADR 0006 (escopo do dialeto PLA), ADR 0003 (codificação de `Term`).
