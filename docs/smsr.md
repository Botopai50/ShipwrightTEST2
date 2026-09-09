# SMSR no Shadow Map existente

Base: `Otimização_Foda_CLAUDE.O`, Shipwright `ee679f1d9a51135cdf7544a381f792a45f0f4229`
e libultraship `f6b83b962f4d3833ffa5cd260cf194ef94566409`.
Implementação na branch `Otimização_Foda_GPT.A_SMSR` dos dois repositórios.

## Como comparar

Em **Sombras > Geral > Silhueta**, selecione **Shadow Map original** ou **Shadow Map + SMSR**.
O sistema Shadow Map precisa estar ativo. A troca preserva resolução, mapas e configurações dos filtros.

Com SMSR, a visibilidade é binária. PCF, jitter, borda analítica, endurecimento e cross-fade de cascatas
são ignorados. A intensidade da sombra, iluminação e fog continuam sendo aplicados pelo jogo.
O modo original mantém o comportamento anterior e é o padrão.

Em **Modo Avançado > Borda**:

| Configuração | Padrão | Intervalo |
|---|---:|---:|
| Busca SMSR por sentido | 16 texels | 1–64 |
| Epsilon de profundidade normalizada | 0,00002 | 0–0,001 |

CVars: `Graphics.ShadowQuality.SMSR`, `Graphics.ShadowQuality.SMSRMaxSteps` e
`Graphics.ShadowQuality.SMSREpsilon`, com o prefixo de enhancements habitual.
Restaurar padrões também redefine esses três valores. Perfis de qualidade preservam a escolha da técnica.

## Implementação

O shader usa `Texture2DArray.Load` no mapa existente. Calcula o teste binário, as quatro descontinuidades
e `dc`. Somente fragmentos iluminados junto à silhueta entram na revectorização. As travessias avançam
um texel por passo, no eixo perpendicular à descontinuidade dominante. O código calcula distâncias
orientadas, `L`, ONDS/`don`, classificação das extremidades e o recorte `vSMSR` dos 12 casos.
Interseções usam o mínimo das duas visibilidades. A contribuição reconstruída multiplica a sombra original.

O plano geométrico do receptor fornece a variação de profundidade entre texels, sem derivadas dentro das
travessias divergentes. O epsilon resolve apenas comparações muito próximas. Busca truncada, limite do mapa
ou plano degenerado usam um fallback conservador de sombra dura. Não se inventa uma extremidade ao atingir
o limite de busca.

Não há passe, textura ou buffer de descontinuidades adicional. São acrescentados 16 bytes ao constant
buffer de sombras existente. A implementação vale para Cascatas e Clipmap e usa a resolução da camada
amostrada, incluindo personagens em resolução diferente.

Referência: Macedo e Apolinário, *Revectorization-Based Shadow Mapping*, GI 2016;
[suplemento, seção 2](https://marciocerqueira.github.io/docs/publications/2016-GI-Supp.pdf).
O código identifica explicitamente os casos 1–12.

## Correções necessárias na base Clipmap

- A alocação DirectX agora respeita até 10 níveis do Clipmap, mantendo o limite de 3 para Cascatas.
- O shader usa o número real de níveis como deslocamento da camada de personagens. O buffer fixo de
  três matrizes não é ampliado nem acessado além de seu limite.
- A escada de cascatas permanece limitada a seus vetores de três entradas mesmo com Clipmap ativo.
- A visualização de profundidade usa o número real de níveis e os índices corretos dos personagens.

Essas correções recuperam níveis que anteriormente eram solicitados e não alocados. Por isso, valores altos
de níveis podem consumir mais memória e renderização do que na implementação incompleta anterior.
As alterações experimentais anteriores de compactação/estabilização de Clipmap não foram transplantadas
sobre esta base.

## Validação local

```sh
cmake -S libultraship/tests/smsr -B build-smsr
cmake --build build-smsr --config Release --target smsr_tests smsr_prism_driver smsr_clipmap_tests
ctest --test-dir build-smsr -C Release -R "^smsr_" --output-on-failure
```

As três suítes passaram com MSVC e Direct3D 11 WARP:

- Os 12 casos, igualdade na fronteira, normalização orientada e fallback de busca truncada.
- Revectorização de uma borda analítica em oito orientações, visibilidade estritamente 0/1 e preservação
  de regiões já sombreadas; no recorte interno do caso sintético, 768 pixels divergiam da diagonal na
  sombra original e nenhum após SMSR. Isso é um teste controlado, não uma captura do jogo.
- Receptor inclinado com profundidade de 16 bits, troca entre os caminhos original/SMSR, camadas e
  resoluções distintas, limites do mapa e da configuração.
- Alocação real de texturas e views para 1–10 níveis, limites do vetor de cascatas e índices de personagens.
- Seis variantes completas do shader expandidas pelo Prism do projeto, compiladas nativamente como
  VS/PS 4.0 e 4.1, incluindo Toon Lighting, fog, alpha e ambos os kernels originais. Reflexão verifica
  o alinhamento do parâmetro SMSR com o constant buffer C++ de produção.

SMSR acrescenta buscas junto às bordas: não representa ganho garantido de FPS na mesma resolução.
Pode permitir experimentar mapas menores com melhor silhueta, mas o custo e a aparência precisam ser
comparados no jogo. Nenhum build SoH/Windows ou GitHub Actions foi acompanhado.
