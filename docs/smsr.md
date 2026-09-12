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

## Atualização do cache do cenário

O cache de upload DirectX 11 agora acompanha a versão da geometria do cenário. Comparar apenas o endereço
do buffer e a quantidade de vértices podia manter dados antigos na GPU: após capturas sem envio, uma nova
captura pode reutilizar uma alocação antiga com conteúdo diferente. O mapa era então redesenhado usando
vértices desatualizados, mesmo com o intervalo de atualização em 1 quadro.

A invalidação por versão cobre geometria opaca e recortes por transparência, nas Cascatas e no Clipmap,
com Shadow Map original ou SMSR. Geometria inalterada continua reaproveitando o upload. Não muda os
divisores configurados nem a ordem de desenho. O teste reproduz a falha nos buffers; confirmar se ela
explica os saltos observados pelo usuário ainda exige comparação na mesma cena do jogo.

## Movimento do sol com a câmera parada

A direção usada pelo Shadow Map anteriormente vinha dos comandos de iluminação de 8 bits do jogo.
Esses valores podem permanecer iguais por vários quadros da lógica antes de mudar uma unidade: uma
sombra longa de uma parede ou torre fica parada e depois salta. O interpretador também retinha pequenas
mudanças angulares. Reduzir o intervalo das cascatas para 1 não recuperava a precisão perdida na origem.

Para a iluminação solar automática externa, o Shadow Map agora calcula a mesma órbita do ambiente em
ponto flutuante e amostra o horário com a mesma fração de interpolação usada pelas matrizes do frame.
O interpretador não retém mais pequenas mudanças de direção. A seleção entre sol e lua e a altura mínima
da luz são preservadas. Interiores, iluminação sobrescrita e ajustes do depurador mantêm o caminho existente.

A interpolação trata a passagem pela meia-noite nos dois sentidos e reinicia em trocas de cena, grandes
saltos de horário, troca entre sol e lua e reativação. Horário parado não produz movimento artificial.
O reaproveitamento de geometria permanece; mapas cuja projeção muda com a luz precisam ser redesenhados,
portanto esta correção de fluidez pode aumentar o custo na GPU. A aparência e o FPS ainda precisam ser
comparados no jogo. Os intervalos de cascatas configurados pelo usuário continuam valendo.

O teste isolado `tests/shadow_light` simula uma câmera e uma parede paradas: em 40 quadros de lógica,
a direção quantizada mudou apenas 5 vezes, enquanto o novo caminho movimentou a ponta da sombra nos
120 frames renderizados. Também verifica pausa, volta do relógio, mudanças de cena e troca de modo.

```sh
cmake -S tests/shadow_light -B build-shadow-light
cmake --build build-shadow-light --config Release
ctest --test-dir build-shadow-light -C Release --output-on-failure
```

## Estabilidade da grade e da profundidade

A orientação dos eixos do mapa agora acompanha a mudança de direção da luz pela menor rotação,
preservando a orientação anterior. Recalcular os eixos a partir do eixo vertical do mundo provocava
rotação adicional perto do sol alto e uma mudança brusca ao trocar o eixo de referência. Isso altera
a posição dos texels sobre os contornos mesmo quando a luz muda pouco. A mesma base é usada para
desenhar e consultar o mapa em Cascatas e Clipmap, com o método original ou SMSR.

No teste da órbita perto do meio-dia, o deslocamento acumulado do eixo da grade caiu de 0,35629 para
0,058273, aproximadamente seis vezes menor. A direção da luz continua em movimento; não há retenção
angular, filtro temporal nem desfocagem. Uma luz parada mantém a base idêntica para preservar o cache.
Isso reduz uma fonte de instabilidade, mas não garante eliminar todo aliasing temporal de uma borda
binária reconstruída de um mapa de resolução finita.

O rasterizador também aplica uma margem de dois incrementos D16 para evitar que uma superfície se
auto-sombreie quando o arredondamento muda. O limite de bias usa a precisão D16 e não pode mais virar
zero, valor que desativava o limite no DirectX. Nos mapas muito extensos, a margem mínima pode exceder
o limite nominal em unidades de mundo devido à precisão disponível. O teste planar reproduziu
auto-sombreamento em 32/64 frames sem margem e 0/64 com ela, preservando a oclusão de outra superfície.

Esses testes são isolados; não medem a tremulação na imagem enviada pelo usuário. A comparação visual
da cena do portão continua necessária. As alterações não aumentam a resolução nem criam passes extras.

## Comparações sobre paredes inclinadas

O modo original filtrado agora usa a profundidade do plano geométrico da parede em cada texel consultado.
Antes, os quatro texels eram comparados com uma só profundidade do pixel receptor, produzindo falsa
oclusão em superfícies inclinadas em relação à luz. Os deslocamentos do filtro com jitter também deslocam
a referência de profundidade. A reconstrução analítica recebe a diferença entre as profundidades de cada
amostra. A comparação inclui uma margem de um incremento D16 para arredondamento.

A alteração não acrescenta leituras de textura ou passes. Acrescenta o cálculo do plano e a correção das
referências. Um teste HLSL em 64 posições subtexel reproduz falsa oclusão na comparação antiga e verifica
as referências corrigidas, preservando a oclusão de outra superfície. As seis variantes completas
compilaram em VS/PS 4.0 e 4.1. O SMSR já fazia comparação no plano e não foi alterado por esta correção:
os seus testes existentes passam, mas não reproduzem as pontas mostradas pelo usuário. Esse sintoma
ainda precisa de investigação; não se trata de uma correção comprovada das fotos no modo SMSR.

## Contorno no modo original: descontinuidade entre superfícies

A borda analítica comparava profundidades e interpolava a magnitude das diferenças. Quando texels
vizinhos pertencem ao oclusor e à parede receptora, eles não descrevem uma superfície contínua.
Essa interpolação deslocava o contorno conforme a distância entre as superfícies ou a margem de bias.
Agora a rampa é calculada sobre a visibilidade binária das quatro comparações. Não há novas leituras,
aumento de resolução ou alteração automática das configurações. A opção continua sendo uma aproximação
de cobertura, e as descrições do menu deixaram de prometer recuperação geométrica exata.

Um teste WARP mantém uma borda vertical e altera apenas as profundidades de suas amostras. A versão
anterior falha; a corrigida preserva a mesma cobertura nos 2048 pixels comparados, além dos interiores
iluminados e sombreados. Isso demonstra o defeito do filtro, mas não reproduz a cena do portão.
A alteração atua no modo Original, tanto em cascatas quanto em clipmap. O SMSR ignora essa função;
as pontas relatadas nesse modo ainda não têm causa confirmada.

Também foi testada a escolha de cascatas com triângulos em perspectiva: o DirectX WARP entrega em
SV_Position.w a profundidade interpolada esperada nos 1024 pixels verificados. Inverter esse valor
seria um erro. A hipótese foi descartada e a seleção de produção foi preservada.

## Texels vazios em receptores rasantes

A captura de diagnóstico 2 enviada pelo usuário mostra as pontas em verde, na camada do cenário.
A investigação encontrou uma falha comum ao Original e ao SMSR: ao estender o plano receptor até
os texels consultados, a profundidade pode ultrapassar 1 em ângulos rasantes. Comparar esse valor
com um texel limpo (profundidade 1) criava oclusão onde não havia geometria no mapa.

Agora o valor exato de limpeza permanece iluminado. No Original, a regra é aplicada às quatro
comparações antes do PCF ou da rampa analítica; no SMSR, antes de classificar a descontinuidade.
Profundidades reais abaixo de 1 continuam sendo comparadas normalmente. Não há aumento de resolução,
passes ou leituras de textura.

O teste WARP com textura R16 vazia e receptor rasante falhou antes da correção e passou depois.
Também verifica a camada de personagens, gradientes positivos/negativos nas comparações filtradas,
oclusores reais e profundidades próximas do limite distante. As variantes completas HLSL são validadas
separadamente. O caso sintético não reproduz a geometria do portão: a correção dessa falha está
verificada, mas a eliminação de todas as pontas da captura ainda depende de comparação no jogo.

## Captura manual do defeito no DirectX

Em Sombras, habilite as opções avançadas e use **Salvar captura das sombras (DirectX)** na seção
Depuração, com o portão defeituoso visível. A próxima atualização do mapa salva uma subpasta numerada
em `shadow-captures` dentro da pasta de dados do aplicativo. O caminho aparece no menu e no log.
Envie a subpasta compactada junto da imagem correspondente. Não é preciso alterar resolução, bias,
filtro ou horário para capturar. O botão pode pausar brevemente o jogo durante a leitura da GPU.

A captura é manual, de um único quadro, e contém apenas a camada do cenário: `world.sds` guarda
profundidades D16 sem perda; `capture.json` guarda os parâmetros do receptor e as matrizes efetivamente
associadas às fatias, com seus indicadores de validade. Não inclui a ROM, a geometria completa, nem os
pixels de posição mundial da imagem final; é uma captura dos mapas e parâmetros, não um replay completo.
Ela permite inspecionar a grade, a profundidade armazenada e possíveis divergências de projeção.

O formato SDS1 usa inteiros little-endian: quatro u32 (magic 0x31534453, largura, altura, número de fatias).
Cada linha de cada fatia contém u32 quantidade de runs, seguida de pares (u16 profundidade, u32 repetição).
As linhas são guardadas sem o padding da GPU. Há limite de 512 MiB por arquivo de profundidade e 8192 por
dimensão. Uma falha aparece no menu; um arquivo incompleto sem metadados válidos não é uma captura pronta.
A solicitação é consumida uma vez. Nenhuma imagem de sombra, filtro ou resolução foi alterada nesta etapa.

O teste `smsr_depth_capture` compila o bloco de exportação do backend e verifica leitura real de uma
textura D16 WARP com dez fatias e linhas com padding, igualdade de cada valor após descompressão,
metadados, consumo da solicitação e mensagem de falha com mapa inativo. A configuração dos testes
precisa encontrar `nlohmann/json.hpp` (`SHADOW_CAPTURE_JSON_INCLUDE` pode indicar o diretório de includes).

## Validação local

```sh
cmake -S libultraship/tests/smsr -B build-smsr
cmake --build build-smsr --config Release --target smsr_tests smsr_prism_driver smsr_clipmap_tests smsr_upload_tests smsr_light_frame_tests smsr_depth_tests smsr_capture_tests
ctest --test-dir build-smsr -C Release -R "^smsr_" --output-on-failure
```

As suítes isoladas usam MSVC e Direct3D 11 WARP:

- Os 12 casos, igualdade na fronteira, normalização orientada e fallback de busca truncada.
- Revectorização de uma borda analítica em oito orientações, visibilidade estritamente 0/1 e preservação
  de regiões já sombreadas; no recorte interno do caso sintético, 768 pixels divergiam da diagonal na
  sombra original e nenhum após SMSR. Isso é um teste controlado, não uma captura do jogo.
- Receptor inclinado com profundidade de 16 bits, troca entre os caminhos original/SMSR, camadas e
  resoluções distintas, limites do mapa e da configuração.
- Alocação real de texturas e views para 1–10 níveis, limites do vetor de cascatas e índices de personagens.
- Leitura dos buffers da GPU após reutilização de alocações: reprodução dos dados antigos e correção
  por versão do cenário, tanto para vértices opacos como para recortes por transparência.
- Seis variantes completas do shader expandidas pelo Prism do projeto, compiladas nativamente como
  VS/PS 4.0 e 4.1, incluindo Toon Lighting, fog, alpha e ambos os kernels originais. Reflexão verifica
  o alinhamento do parâmetro SMSR com o constant buffer C++ de produção.

SMSR acrescenta buscas junto às bordas: não representa ganho garantido de FPS na mesma resolução.
Pode permitir experimentar mapas menores com melhor silhueta, mas o custo e a aparência precisam ser
comparados no jogo. Nenhum build SoH/Windows ou GitHub Actions foi acompanhado.

A captura também registra `game_context`: cena, relógio bruto do jogo (`day_time_u16`, escala
0–65535 para 24 horas), horários anterior/atual da interpolação solar, fração do quadro renderizado,
seleção automática sol/lua e elevação mínima configurada. Esses valores são lidos no subquadro da
captura, não no clique do menu. A direção efetiva da luz permanece registrada nos eixos/matrizes
exportados, inclusive quando a iluminação automática está desativada.
