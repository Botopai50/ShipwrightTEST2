#include "SohMenu.h"
#include "SohGui.hpp"
#include "soh/OTRGlobals.h"
#include "UIWidgets.hpp"
#include "soh/Enhancements/Graphics/ToonLighting.h"

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// "Qualidade das Sombras" -- the four techniques that shape a shadow's EDGE, as opposed to deciding where
// it falls. See fast/shadow_map.h for what each one does and why it exists; this file is only the surface.
//
// Its own tab rather than more sliders under Actor Shadows, because these are a different question from the
// ones there. Those describe where the cascades sit and what they cost; these describe what the boundary
// looks like once it is in the right place, and they stack -- every one of them can be on at once, so they
// need room to be read side by side rather than a mode selector.
//
// Every technique is off by default. The defaults below are written out rather than pulled from
// fast/shadow_map.h, matching the convention in SohMenuWindWakerStyle.cpp, and each names the constant it
// must be kept in step with.

// Keyed by SHADOW_MAP_LAYOUT_*.
static const std::map<int32_t, const char*> shadowLayoutLabels = {
    { 0, "Cascatas" }, // SHADOW_MAP_LAYOUT_CASCADE
    { 1, "Clipmap" },  // SHADOW_MAP_LAYOUT_CLIPMAP
};

// Keyed by SHADOW_MAP_LADDER_*.
static const std::map<int32_t, const char*> shadowLadderModeLabels = {
    { 0, "Manual (sliders)" }, // SHADOW_MAP_LADDER_MANUAL
    { 1, "Automática" },       // SHADOW_MAP_LADDER_PRACTICAL
};

void SohMenu::AddMenuShadowQuality() {
    AddMenuEntry("Qualidade das Sombras", CVAR_SETTING("Menu.ShadowQualitySidebarSection"));

    // The whole tab only means anything in Shadow Map mode: every one of these acts on the depth-map
    // receiver, and none of them touch the vanilla blobs or the stencil silhouettes. Shown greyed with an
    // explanation rather than hidden, so the tab is not an empty page that looks broken.
    auto hideUnlessShadowMap = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                        SHADOW_MODE_SHADOW_MAP;
    };
    auto showUnlessShadowMap = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) ==
                        SHADOW_MODE_SHADOW_MAP;
    };

    // ===========================================================================================
    // Overview -- what this tab is, and the one button that puts it all back.
    // ===========================================================================================
    WidgetPath path = { "Qualidade das Sombras", "Visão Geral", SECTION_COLUMN_1 };
    AddSidebarEntry("Qualidade das Sombras", "Visão Geral", 3);

    AddWidget(path, "O modo Shadow Map não está ativo.", WIDGET_TEXT).PreFunc(showUnlessShadowMap);
    AddWidget(path, "Ative-o em Wind Waker Style > Actor Shadows > Shadow System.", WIDGET_TEXT)
        .PreFunc(showUnlessShadowMap);

    AddWidget(path, "Sobre", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Estas opções mudam apenas a APARÊNCIA da borda da sombra.", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Onde a sombra cai continua sendo decidido na aba Wind Waker Style.", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "As quatro técnicas são independentes e podem ser combinadas.", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);

    AddWidget(path, "Restaurar Tudo ao Padrão", WIDGET_BUTTON)
        .PreFunc(hideUnlessShadowMap)
        .Callback([](WidgetInfo& info) {
            // Clearing each CVar drops it back to the widget's DefaultValue, which is the same value the
            // renderer falls back to -- so this restores the shipped look without hardcoding it twice.
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.AnalyticEdge"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.AnalyticEdgeWidth"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.Jitter"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.JitterTaps"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.JitterRadius"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.StaticCache"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.Layout"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ClipmapLevels"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ClipmapBase"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ClipmapResolution"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EdgeHarden"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EdgeHardness"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EdgeThreshold"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderMode"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderLambda"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderNear"));
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Tooltip("Desliga as cinco técnicas e devolve todos os valores ao padrão."));

    // ===========================================================================================
    // Technique 2 -- analytic edge reconstruction.
    // ===========================================================================================
    path = { "Qualidade das Sombras", "Borda Analítica", SECTION_COLUMN_1 };
    AddSidebarEntry("Qualidade das Sombras", "Borda Analítica", 3);

    auto hideUnlessAnalytic = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.AnalyticEdge"), 0);
    };

    AddWidget(path, "Ativar Borda Analítica", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.AnalyticEdge"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Calcula onde a borda da sombra CRUZA o texel, em vez de misturar as quatro comparações.\n\n"
            "Usa exatamente as mesmas quatro leituras que já são feitas: não custa nenhuma leitura extra "
            "de textura, só aritmética.\n\n"
            "É a opção mais barata da aba e a que ataca o serrilhado diretamente. Exata para uma borda reta "
            "(paredes, degraus, telhados, plataformas); em folhagem ela volta sozinha ao comportamento "
            "antigo."));
    AddWidget(path, "Largura da Rampa: %.2f texels", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.AnalyticEdgeWidth"))
        .RaceDisable(false)
        .PreFunc(hideUnlessAnalytic)
        .Options(FloatSliderOptions()
                     .Tooltip("Quantos texels a transição ocupa.\n\n"
                              "1.00 é a resposta geométrica: a rampa ocupa exatamente o texel que a borda "
                              "atravessa. Acima disso a borda é suavizada de propósito -- é o amaciamento "
                              "mais barato do sistema, porque custa conta e nenhuma leitura.")
                     .Min(0.25f)
                     .Max(4.0f) // SHADOW_MAP_MAX_ANALYTIC_EDGE_WIDTH
                     .Step(0.05f)
                     .DefaultValue(2.0f) // SHADOW_MAP_DEFAULT_ANALYTIC_EDGE_WIDTH
                     .Format("%.2f"));

    // ===========================================================================================
    // Technique 3 -- stochastic jitter.
    // ===========================================================================================
    path = { "Qualidade das Sombras", "Jitter", SECTION_COLUMN_1 };
    AddSidebarEntry("Qualidade das Sombras", "Jitter", 3);

    auto hideUnlessJitter = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.Jitter"), 0);
    };

    AddWidget(path, "Ativar Jitter", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.Jitter"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Espalha as amostras num disco girado por um ângulo diferente em cada pixel.\n\n"
            "O degrau não fica menor, mas pixels vizinhos param de pular no mesmo lugar, então a borda é "
            "lida como granulado em vez de escada.\n\n"
            "ATENÇÃO: este jogo tem FXAA e não tem filtro temporal. Sem nada para calcular a média do "
            "granulado, ele aparece como chiado -- especialmente em movimento. Aumente as Amostras se "
            "incomodar."));
    AddWidget(path, "Amostras: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.JitterTaps"))
        .RaceDisable(false)
        .PreFunc(hideUnlessJitter)
        .Options(IntSliderOptions()
                     .Tooltip("Quantas leituras o disco faz.\n\n"
                              "É o único ajuste desta aba que custa banda de memória de verdade, e é também "
                              "o que decide se o resultado é lido como suavidade ou como chiado. Mais "
                              "amostras = mais suave e mais caro.")
                     .Min(1)
                     .Max(16) // SHADOW_MAP_MAX_JITTER_TAPS
                     .DefaultValue(8)); // SHADOW_MAP_DEFAULT_JITTER_TAPS
    AddWidget(path, "Raio: %.2f texels", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.JitterRadius"))
        .RaceDisable(false)
        .PreFunc(hideUnlessJitter)
        .Options(FloatSliderOptions()
                     .Tooltip("Tamanho do disco, em texels da cascata amostrada.\n\n"
                              "É o que define a largura da penumbra neste modo. Raio 0 desliga o efeito: "
                              "todas as amostras cairiam no mesmo lugar.")
                     .Min(0.0f)
                     .Max(8.0f) // SHADOW_MAP_MAX_JITTER_RADIUS
                     .Step(0.1f)
                     .DefaultValue(2.0f) // SHADOW_MAP_DEFAULT_JITTER_RADIUS
                     .Format("%.2f"));

    // ===========================================================================================
    // Edge hardening -- the control in the other direction from everything above.
    // ===========================================================================================
    path = { "Qualidade das Sombras", "Dureza da Borda", SECTION_COLUMN_1 };
    AddSidebarEntry("Qualidade das Sombras", "Dureza da Borda", 3);

    auto hideUnlessHarden = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EdgeHarden"), 0);
    };

    AddWidget(path, "As outras seções alargam a borda. Esta comprime.", WIDGET_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "A borda se identifica sozinha: cobertura vale 0 ou 1 em todo o interior, e só", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "o contorno cai no meio. Então isto age só no contorno, sem procurar por ele", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "e sem nenhuma leitura extra de textura.", WIDGET_TEXT).PreFunc(hideUnlessShadowMap);

    AddWidget(path, "Ativar Dureza da Borda", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EdgeHarden"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Comprime a faixa de transição da sombra para um contorno mais definido.\n\n"
            "Age depois de tudo o mais, sobre o valor final de cobertura -- então funciona igual com "
            "qualquer combinação das outras técnicas, e a visão de diagnóstico 5 continua mostrando o "
            "valor CRU, antes desta compressão."));
    AddWidget(path, "Dureza", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EdgeHardness"))
        .RaceDisable(false)
        .PreFunc(hideUnlessHarden)
        .Options(FloatSliderOptions()
                     .Tooltip("0% deixa a cobertura exatamente como chegou. 100% é um corte seco, sem "
                              "transição nenhuma.\n\n"
                              "Corte seco nem sempre é o que se quer: a penumbra carrega o tamanho do texel "
                              "da faixa, então tirá-la tira a única pista de que a distância está sendo "
                              "amostrada mais grosso -- e o contorno passa a mostrar a grade de texels que "
                              "ela escondia. O ponto costuma ficar um pouco antes de 100%.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .Step(0.01f)
                     .DefaultValue(0.5f) // SHADOW_MAP_DEFAULT_EDGE_HARDNESS
                     .IsPercentage());
    AddWidget(path, "Limiar", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EdgeThreshold"))
        .RaceDisable(false)
        .PreFunc(hideUnlessHarden)
        .Options(FloatSliderOptions()
                     .Tooltip("Onde fica o contorno dentro da faixa de cobertura. 50% é a resposta "
                              "geométrica: metade do kernel bloqueada é a borda.\n\n"
                              "Mover ENGROSSA ou AFINA a sombra. Abaixo de 50% um pixel pouco bloqueado já "
                              "conta como sombra e ela se espalha; acima, ela se recolhe. É com isto, junto "
                              "com a Dureza, que se engrossa um contorno em vez de só afiá-lo.")
                     .Min(0.05f)
                     .Max(0.95f)
                     .Step(0.01f)
                     .DefaultValue(0.5f) // SHADOW_MAP_DEFAULT_EDGE_THRESHOLD
                     .IsPercentage());

    // ===========================================================================================
    // Layout -- the ladder, or the clipmap. The one setting here that changes the SHAPE of the system.
    // ===========================================================================================
    path = { "Qualidade das Sombras", "Formato do Mapa", SECTION_COLUMN_1 };
    AddSidebarEntry("Qualidade das Sombras", "Formato do Mapa", 3);

    auto hideUnlessClipmap = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.Layout"), 0) !=
                            1; // SHADOW_MAP_LAYOUT_CLIPMAP
    };

    AddWidget(path, "Formato", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.Layout"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(ComboboxOptions()
                     .ComboMap(shadowLayoutLabels)
                     .DefaultIndex(0) // SHADOW_MAP_DEFAULT_LAYOUT
                     .Tooltip(
                         "Como o mapa é distribuído pelo mundo. É a única opção desta aba que muda a FORMA "
                         "do sistema, não o acabamento dele.\n\n"
                         "CASCATAS: faixas ajustadas a fatias do campo de visão. O que existe hoje. A razão "
                         "de texel entre faixas vizinhas é de 3 a 8 vezes, e é esse salto que a transição "
                         "precisa esconder.\n\n"
                         "CLIPMAP: quadrados aninhados centrados na CÂMERA, cada um com o dobro da extensão "
                         "do anterior. A razão de texel entre níveis vizinhos é exatamente 2, e as "
                         "fronteiras são círculos que andam COM você em vez de varrerem o mundo quando a "
                         "câmera gira.\n\n"
                         "É a metade direcional do Virtual Shadow Map da Unreal -- a parte que dá para "
                         "portar. A paginação de memória dela não dá: precisa de compute, indirect draw e "
                         "um passe de profundidade prévio.\n\n"
                         "TROCA: o nível 0 do clipmap é mais grosso que a cascata 0 (0,37 contra 0,09 "
                         "unidade por texel a 1024). A cascata 0 é fina além do que se enxerga àquela "
                         "distância; é justamente esse desperdício que o clipmap remove. Se a sombra aos "
                         "pés do Link ficar grosseira, suba a Resolução."));
    AddWidget(path, "Níveis: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ClipmapLevels"))
        .RaceDisable(false)
        .PreFunc(hideUnlessClipmap)
        .Options(IntSliderOptions()
                     .Tooltip("Quantos quadrados aninhados. Cada um dobra a extensão do anterior, então N "
                              "níveis alcançam a extensão base vezes 2^(N-1).\n\n"
                              "É AQUI que se ganha alcance. Níveis custam LINEAR e compram alcance "
                              "EXPONENCIAL: um nível a mais dobra o alcance pelo preço de uma fatia -- e uma "
                              "fatia de clipmap é pequena, porque densidade uniforme significa que nenhum "
                              "nível precisa ser grande para ser nítido.\n\n"
                              "Aumentar a Extensão do Nível 0 também estende o alcance, mas às custas da "
                              "nitidez perto. Mais níveis não tem esse custo.")
                     .Min(1)
                     .Max(10) // SHADOW_MAP_MAX_CLIPMAP_LEVELS
                     .DefaultValue(8)); // SHADOW_MAP_DEFAULT_CLIPMAP_LEVELS
    AddWidget(path, "Cache de Casters Estáticos", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.StaticCache"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Guarda uma segunda cópia de cada fatia do mundo contendo SÓ o que não se mexe, e a cada quadro "
            "copia essa cópia e redesenha apenas o que se moveu por cima.\n\n"
            "O caso que isto resolve: hoje uma árvore balançando custa a malha da sala inteira de novo, em "
            "toda fatia que a árvore alcança, todo quadro em que ela se mexe.\n\n"
            "A cópia substitui tanto a limpeza quanto a rasterização da sala. A 4096 uma fatia custa ~420 µs "
            "para redesenhar e a cópia custa ~62 µs; a 1024 a cópia custa ~4 µs, que é nada.\n\n"
            "CUSTA MEMÓRIA -- um segundo array do tamanho da camada do mundo:\n"
            "  cascatas a 4096: +96 MB     cascatas a 2048: +24 MB\n"
            "  clipmap a 1024:  +12 MB     clipmap a 2048:  +48 MB\n\n"
            "Por isso combina com o Clipmap: o formato que quer muitos níveis pequenos é justamente aquele "
            "onde a segunda cópia sai barata.\n\n"
            "NÃO ajuda cena parada -- essa já não redesenha nada. Isto compra de volta o custo do "
            "movimento, não o de ficar parado."));

    AddWidget(path, "Extensão do Nível 0: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ClipmapBase"))
        .RaceDisable(false)
        .PreFunc(hideUnlessClipmap)
        .Options(FloatSliderOptions()
                     .Tooltip("Meia-extensão do nível mais interno, em unidades de mundo. Toda a escada "
                              "segue: o nível i é este valor vezes 2^i.\n\n"
                              "120 com oito níveis alcança cerca de 15000 -- duas vezes e meia a escada de "
                              "cascatas -- com o nível 0 medindo 240 unidades de lado.\n\n"
                              "Menor deixa a sombra mais nítida aos pés e encurta o alcance total; maior faz "
                              "o contrário.")
                     .Min(20.0f)   // SHADOW_MAP_MIN_CLIPMAP_BASE
                     .Max(2000.0f) // SHADOW_MAP_MAX_CLIPMAP_BASE
                     .Step(10.0f)
                     .DefaultValue(120.0f) // SHADOW_MAP_DEFAULT_CLIPMAP_BASE
                     .Format("%.0f"));

    // ===========================================================================================
    // Technique 4 -- cascade split ladder.
    // ===========================================================================================
    path = { "Qualidade das Sombras", "Escada de Cascatas", SECTION_COLUMN_1 };
    AddSidebarEntry("Qualidade das Sombras", "Escada de Cascatas", 3);

    auto hideUnlessAutoLadder = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderMode"), 0) !=
                            1; // SHADOW_MAP_LADDER_PRACTICAL
    };

    AddWidget(path, "Distribuição das Faixas", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderMode"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(ComboboxOptions()
                     .ComboMap(shadowLadderModeLabels)
                     .DefaultIndex(0) // SHADOW_MAP_DEFAULT_LADDER_MODE
                     .Tooltip("Onde caem as fronteiras entre as cascatas, o que decide o tamanho do texel "
                              "em cada faixa -- ou seja, o tamanho do degrau ANTES de qualquer filtro.\n\n"
                              "Manual usa os sliders de distância da aba Wind Waker Style, como sempre.\n\n"
                              "Automática recalcula as fronteiras internas para uniformizar o texel entre as "
                              "faixas. O ALCANCE não muda: a última distância continua sendo a sua.\n\n"
                              "Não custa nada -- são três números, sem shader e sem memória nova."));
    AddWidget(path, "Uniforme <-> Logarítmica", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderLambda"))
        .RaceDisable(false)
        .PreFunc(hideUnlessAutoLadder)
        .Options(FloatSliderOptions()
                     .Tooltip("0% = faixas com a mesma PROFUNDIDADE. 100% = faixas com a mesma RAZÃO, que é "
                              "o que deixa o texel do mesmo tamanho em todas.\n\n"
                              "Logarítmica pura joga a primeira fronteira muito perto da câmera, e uma "
                              "cascata que cobre quase nada desperdiça uma fatia inteira. A mistura é o "
                              "meio-termo usual.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .Step(0.01f)
                     .DefaultValue(0.85f) // SHADOW_MAP_DEFAULT_LADDER_LAMBDA
                     .IsPercentage());
    AddWidget(path, "Distância Inicial: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderNear"))
        .RaceDisable(false)
        .PreFunc(hideUnlessAutoLadder)
        .Options(FloatSliderOptions()
                     .Tooltip("De onde a escada é gerada.\n\n"
                              "Não é o plano próximo da câmera, que é pequeno o bastante para puxar a "
                              "primeira fronteira para quase zero. É a distância a partir da qual vale a "
                              "pena resolver sombras com precisão.")
                     .Min(1.0f)
                     .Max(1000.0f)
                     .Step(5.0f)
                     .DefaultValue(40.0f) // SHADOW_MAP_DEFAULT_LADDER_NEAR
                     .Format("%.0f"));

    AddWidget(path, "Resolução por Nível", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ClipmapResolution"))
        .RaceDisable(false)
        .PreFunc(hideUnlessClipmap)
        .Options(ComboboxOptions()
                     .ComboMap(shadowMapResolutionLabels)
                     .DefaultIndex(4096) // SHADOW_MAP_DEFAULT_CLIPMAP_RESOLUTION
                     .Tooltip(
                         "Resolução de cada nível, SEPARADA da Resolução das cascatas.\n\n"
                         "Separada porque as duas querem coisas opostas: um clipmap quer muitos níveis "
                         "pequenos, uma escada de cascatas quer poucos grandes. Compartilhar o número faz um "
                         "passar fome ou o outro estourar a memória.\n\n"
                         "Unidades de mundo por texel, contra a escada de cascatas na SUA 4096:\n"
                         "  distância    cascatas   clip 2048   clip 4096\n"
                         "       150        0,090       0,234       0,117\n"
                         "       900        0,740       0,938       0,469\n"
                         "      2000        0,740       3,750       1,875\n"
                         "      4000        3,600       7,500       3,750\n\n"
                         "A 2048 o clipmap perde em quase toda distância. Ele precisa da resolução maior por "
                         "um motivo estrutural: um nível é um QUADRADO CENTRADO NA CÂMERA, enquanto uma "
                         "cascata é uma laje AJUSTADA AO CAMPO DE VISÃO. A câmera olha para um lado só, "
                         "então o quadrado gasta a maior parte da área em chão que ninguém está vendo. É o "
                         "preço das fronteiras que andam com você.\n\n"
                         "8 níveis a 4096 = 256 MB. A camada de personagens usa só os dois níveis mais "
                         "internos, então 4096 lá é desperdício -- baixar Resolução (Personagens) para 1024 "
                         "tira 60 MB do total.\n\n"
                         "Subir aqui deixa tudo mais nítido de uma vez; subir os Níveis estende o alcance."));

    // What the ladder ACTUALLY produced, read back from the renderer rather than recomputed here.
    //
    // This is the answer to "what did automatic decide", and it cannot be got any other way: with the
    // automatic ladder on, the split sliders no longer say where the bands are, and the texel size never
    // did -- it falls out of the projection the fit builds from the camera. Shown in both modes, so manual
    // and automatic can be compared against the same numbers.
    //
    // WIDGET_TEXT draws widget.name and PreFunc runs first, so rewriting the name each frame is what makes
    // it live.
    AddWidget(path, "Resultado", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Cascatas em uso:", WIDGET_TEXT)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP;
            if (!info.isHidden) {
                const char* report = ToonLighting_ShadowMapCascadeReport();
                info.name = std::string("Cascatas em uso:\n") + (report != nullptr ? report : "");
            }
        });

    // The cross-fade between cascades. It has always existed and has never been reachable from the menu --
    // only from the console -- which is why a hard cascade seam had no control to soften it.
    AddWidget(path, "Suavidade da Transição", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.BlendFraction"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Quanto do alcance de cada faixa é usado para dissolver na faixa seguinte.\n\n"
                              "O shader amostra as DUAS cascatas dentro dessa banda e mistura com "
                              "smoothstep. É isso que impede que a mudança de resolução apareça como uma "
                              "linha dura varrendo o chão conforme a câmera anda.\n\n"
                              "Em 0% a transição é um corte seco. Valores altos suavizam mais, mas custam: "
                              "dentro da banda cada pixel faz o dobro das leituras.\n\n"
                              "A banda resultante de cada faixa aparece em 'Cascatas em uso', acima.")
                     .Min(0.0f)
                     .Max(0.5f)
                     .Step(0.01f)
                     .DefaultValue(0.2f) // SHADOW_MAP_DEFAULT_BLEND_FRACTION
                     .IsPercentage());


}
} // namespace SohGui
