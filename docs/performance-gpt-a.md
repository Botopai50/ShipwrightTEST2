# Otimizações de desempenho — GPT.A

Branch baseada em `Otimização_Foda`, commit `85cc2c24b84e3206a8d518de52de58d84a3f0a08`,
com libultraship inicialmente em `988b4fc267b51f71e91aa255b4830ffdc200b5f6`.

Das três propostas da análise original, duas já estavam implementadas nesta base mais recente:

1. **Buffers alpha separados:** o Direct3D 11 usa buffers por camada e slot em
   `ShadowMapUploadAlphaCasters` e `ShadowMapDrawAlphaRange`. O interpretador passa slots distintos
   para mundo estático e objetos de cenário. Os registros dinâmicos são invalidados a cada passe;
   o registro estático pode sobreviver entre frames. Essa implementação foi preservada.
2. **Interpolação sem cópia do mapa de saída:** `FrameInterpolation_Interpolate` recebe o mapa por
   referência e escreve diretamente nele. `Graph_ProcessGfxCommands` reutiliza os mapas entre frames.
   Essa implementação foi preservada; adicionar `std::move` ao antigo retorno não seria aplicável.
3. **Retorno direto de recursos em cache:** implementado na libultraship desta branch. A API síncrona
   normaliza o prefixo OTR e retorna o recurso válido sem criar `promise/shared_future`. Em caso de
   cache miss, compartilha `QueueResourceLoad` com a API assíncrona, mantendo prioridade, controle de
   pedidos em andamento e nova consulta ao cache no worker. O prefixo OTR agora também preserva
   `initData` na API assíncrona.

O submódulo aponta para o commit que contém a terceira alteração; a configuração de branch acompanha
`Otimização_Foda_GPT.A` no repositório libultrashipTEST2.

## Validação

Os testes em `libultraship/tests/resource_manager` compilam o código de produção do gerenciador de
recursos com MSVC/C++20 em Release, usando o thread pool real e substitutos em memória para IO e
decodificação. Não dependem de ROM ou GPU.

- 1.000 cache hits síncronos com identificador previamente construído: **0 alocações** na thread chamadora.
- Controle via API assíncrona seguido de `.get()`: **2.000 alocações** para os mesmos 1.000 hits.
- Testes aprovados: seleção alternativa/exata, prefixos OTR, dirty/reload, unload/reload, arquivos ausentes,
  metadados explícitos, isolamento de owner/parent, chamadas síncronas/assíncronas concorrentes e retry
  após exceção na decodificação.

```sh
cmake -S libultraship/tests/resource_manager -B build-resource-tests
cmake --build build-resource-tests --config Release
ctest --test-dir build-resource-tests -C Release --output-on-failure
```

O resultado de alocações comprova a remoção daquele custo específico. Não é uma medição de FPS do
jogo. A compilação completa e o teste visual em Direct3D 11 são validações separadas dos testes acima.

## Otimizações 4–6

4. **Shaders:** o backend DirectX 11 mantém cache persistente do bytecode em `shadercache-dx11`, com
   chave derivada do HLSL expandido, flags, perfil e variante do kernel de sombras. O pré-aquecimento
   compila variantes previsíveis em threads auxiliares antes de ativar uma opção que as exigirá; falhas
   continuam sendo tratadas pelo caminho normal de compilação.
5. **Leitura de profundidade:** consultas visuais do brilho de sol/tochas podem reutilizar o resultado
   imediatamente anterior quando as coordenadas permanecem iguais. Um anel de três buffers e queries
   `D3D11_QUERY_EVENT` evita `Map` bloqueante; coordenadas novas ou resultados expirados usam a leitura
   síncrona para preservar a resposta correta. A opção `Graphics.AsyncDepthReadback` fica disponível nas
   configurações avançadas e vem ativada por padrão no DirectX 11.
6. **Perfis de sombras:** o menu Wind Waker Style oferece perfis rápidos para 1024/512, 2048/1024 e
   4096/4096 (cenário/personagens). Eles alteram somente as resoluções das camadas do Shadow Map e
   preservam cascatas, alcance e número de faixas. A resolução do clipmap continua independente no menu
   Qualidade das Sombras.

O teste independente `libultraship/tests/depth_readback` usa um dispositivo D3D11 WARP e cobre fallback
síncrono, reaproveitamento com atraso, conclusão do anel, mudança de coordenadas, expiração, reset,
modo desativado e lotes inválidos. Ele foi compilado em Release com MSVC e passou no `ctest` local.
