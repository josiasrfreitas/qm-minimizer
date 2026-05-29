# ADR 0006 — Subset do dialeto PLA do Espresso

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

O formato PLA, originalmente definido para o Espresso (Berkeley), é amplo: aceita múltiplas saídas, vários tipos (`f`, `fr`, `fd`, `fdr`, etc.), notações alternativas para don't-cares, rótulos opcionais, comentários, e até campos arquiteturais herdados de era pré-CAD. Implementar tudo é trabalho considerável e a maior parte das features não aparece nos arquivos que o minimizador consome.

Os arquivos vêm de dois lugares:

- `data/tests/`: PLAs pequenos (3 a 6 vars, 1 saída) usados para verificação manual contra K-maps. Usam `.i`, `.o 1`, `.p`, `.type f` ou `.type fr`, possivelmente `.ilb`/`.ob`.
- `data/benchmark/` e `data/validation/`: 10 casos do IWLS 2020, todos com 1 saída, `.type fr`, 6400 termos, n de 32 a 512.

Em **nenhum** desses casos aparece `.o > 1` (múltiplas saídas). PLAs multi-output exigiriam decisão arquitetural diferente: ou rodar um QM independente por coluna de saída (correto, mas perde a oportunidade de compartilhar primos), ou implementar QM multi-output com primos compartilhados (corretíssimo, bem mais complexo). Nenhum dos dois é necessário para o escopo atual.

## Decisão

O `PlaReader` aceita o seguinte subset:

| Diretiva | Comportamento |
|----------|---------------|
| `.i N` | Lê `N` como `num_inputs`. Obrigatório. |
| `.o 1` | Aceito. Qualquer valor `!= 1` é **rejeitado** com mensagem clara. |
| `.p P` | Número anunciado de termos. Usado para reservar capacidade no `std::vector`; divergência do real é warning, não erro. |
| `.type f` | Apenas on_set; tudo fora é off_set implícito. |
| `.type fr` | on_set e off_set listados; o que não aparecer é don't-care implícito (ver ADR 0007). |
| `.type fd` | on_set e dc_set listados; off_set implícito. |
| `.ilb a b c ...` | Rótulos das entradas, copiados para `Function.input_labels`. |
| `.ob y` | Rótulo da saída (preservado para o `PlaWriter`). |
| `.e` ou `.end` | Marca fim do arquivo; o que vier depois é ignorado. |
| `#` no início da linha | Comentário, ignorado. |
| Linhas em branco | Ignoradas. |

Cada linha de termo é parseada como `<input_pattern> <output_bit>`, onde `input_pattern` é uma string de `'0' '1' '-'` de comprimento `num_inputs`, e `output_bit` é `'0'`, `'1'` ou `'-'`. A classificação no on/off/dc set depende de `.type` e do bit de saída.

```cpp
if (output == '1') fn.on_set.push_back(parse_term(pattern));
else if (output == '0') fn.off_set.push_back(parse_term(pattern));   // só checagem
else if (output == '-') fn.dc_set.push_back(parse_term(pattern));
```

`.o > 1` produz erro:

```
erro: o minimizador suporta apenas funções de saída única (.o = 1).
       arquivo declara .o = 4. veja docs/adr/0006-pla-dialect-scope.md.
```

## Alternativas consideradas

- **Suporte completo a multi-output**: cobre PLAs além dos consumidos hoje, mas complexidade considerável (decisão entre QM independente por saída vs. multi-output compartilhado, mudanças em todas as camadas). Não justificado por ausência de casos. Descartado.
- **Aceitar `.o > 1` rodando QM independente por coluna**: parece barato, mas exige reescrever o Writer (formato PLA multi-output) e perde a oportunidade de discutir primos compartilhados — meia-resposta. Descartado.
- **Subset ainda menor (só `.i`, `.p`, termos)**: rejeitaria os PLAs de `data/tests/` que trazem `.ilb`/`.ob`. Descartado.
- **Aceitar qualquer diretiva silenciosamente**: arrisca produzir saídas erradas sem que o usuário perceba. Descartado em favor de erro explícito.

## Consequências

**Positivas:**
- Cobre 100% dos PLAs consumidos pelo projeto (testes, benchmark, validação) sem código morto.
- Mensagens de erro explícitas guiam o usuário quando o PLA está fora do escopo.
- Parser fica pequeno (~150 linhas), fácil de testar com fixtures de `data/tests/`.

**Limitações:**
- PLAs multi-output exigem extensão futura (provavelmente como nova ADR descrevendo a estratégia escolhida).
- Não suportamos `.type fdr` nem notações binárias compactas usadas por algumas variantes do Espresso. Não aparecem nos benchmarks.
- O off_set lido em `.type fr` é usado apenas como invariante de teste — o QM em si não opera sobre ele (ver ADR 0007).

## Referências

- Brayton, R. K. et al. *Logic Minimization Algorithms for VLSI Synthesis* (Espresso book), Kluwer, 1984 — descrição completa do formato PLA.
- Manual do `espresso` (Berkeley, UC Berkeley CAD group), seção sobre `.type`.
- IWLS 2020 Programming Contest — descrição dos arquivos de benchmark.
- ADR 0007 (semântica de don't-cares em `.type fr`), ADR 0002 (camada Reader).
