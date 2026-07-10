# ADR 0001 — Camadas Reader → IR → Minimizer → Writer

- **Status:** Aceita
- **Data:** 2026-05-29

## Contexto

O minimizador lê PLA, roda QM, escreve PLA. Misturar parsing com algoritmo num único módulo é tentador no começo e vira lock-in depois: trocar de formato (BLIF, JSON, tabela direta) ou de algoritmo (Espresso) implica reescrever o caminho todo.

## Decisão

Três interfaces abstratas com uma struct comum entre elas:

```
PlaReader → Function → QuineMcCluskey → Solution → PlaWriter
```

`Function` carrega `n_inputs`, `type`, `on_set`, `off_set`, `dc_set` e nomes opcionais de variáveis. Nada PLA-específico.

## Consequências

- Trocar formato = implementar novo `Reader`/`Writer`. Algoritmo não muda.
- Parser e algoritmo são testáveis isoladamente.
- A IR é otimizada para SOP/QM. Algoritmos com outra estrutura interna (BDD, AIG) provavelmente vão querer uma IR diferente — limite aceito.
