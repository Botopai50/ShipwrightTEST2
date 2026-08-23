#include "SohMenu.h"
#include "SohGui.hpp"
#include "soh/OTRGlobals.h"
#include "UIWidgets.hpp"
#include "soh/Enhancements/Graphics/ToonLighting.h"

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// "Qualidade das Sombras" -- the five techniques that shape a shadow's EDGE, as opposed to deciding where
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

// Keyed by SHADOW_MAP_FILTER_* -- what the depth pass stores, which decides whether the map can be blurred.
static const std::map<int32_t, const char*> shadowFilterModeLabels = {
    { 0, "Profundidade (PCF)" }, // SHADOW_MAP_FILTER_DEPTH
    { 1, "ESM (exponencial)" },  // SHADOW_MAP_FILTER_ESM
    { 2, "VSM (variância)" },    // SHADOW_MAP_FILTER_VSM
    { 3, "MSM (4 momentos)" },   // SHADOW_MAP_FILTER_MSM
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
    AddWidget(path, "As cinco técnicas são independentes e podem ser combinadas.", WIDGET_TEXT)
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
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.JitterTemporal"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.FilterMode"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EsmExponent"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.BlurRadius"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.BleedReduction"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderMode"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderLambda"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.LadderNear"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ScreenSpace"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ScreenBlur"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ScreenDepthTolerance"));
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
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
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
                     .DefaultValue(1.0f) // SHADOW_MAP_DEFAULT_ANALYTIC_EDGE_WIDTH
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
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
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
    AddWidget(path, "Girar a Cada Quadro", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.JitterTemporal"))
        .RaceDisable(false)
        .PreFunc(hideUnlessJitter)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Avança o padrão a cada quadro, então o granulado se move em vez de ficar parado.\n\n"
            "Desligado por padrão de propósito: um padrão parado é uma textura que o olho para de ver, "
            "enquanto um que se mexe é um brilho que o olho não consegue ignorar. Sem filtro temporal, "
            "animar o ruído geralmente piora. Com muitas amostras pode ajudar -- teste."));

    // ===========================================================================================
    // Technique 1 -- filterable shadow maps.
    // ===========================================================================================
    path = { "Qualidade das Sombras", "Mapa Filtrável", SECTION_COLUMN_1 };
    AddSidebarEntry("Qualidade das Sombras", "Mapa Filtrável", 3);

    auto hideUnlessFilterable = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.FilterMode"), 0) == 0;
    };
    auto hideUnlessEsm = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.FilterMode"), 0) != 1;
    };
    auto hideUnlessMoments = [](WidgetInfo& info) {
        const int32_t mode = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.FilterMode"), 0);
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        (mode != 2 && mode != 3);
    };

    AddWidget(path, "O que o Mapa Guarda", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.FilterMode"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(ComboboxOptions()
                     .ComboMap(shadowFilterModeLabels)
                     .DefaultIndex(0) // SHADOW_MAP_DEFAULT_FILTER_MODE
                     .Tooltip(
                         "Profundidade crua NÃO pode ser borrada: a média de duas profundidades é uma "
                         "superfície que não existe em nenhuma das duas, e um mapa de profundidade borrado "
                         "compara errado em todo lugar.\n\n"
                         "Os outros três guardam algo cuja média faz sentido, o que permite borrar o MAPA. "
                         "Isso tira o custo da suavização do shader de material e coloca num passe só -- o "
                         "que importa muito aqui, porque o shader de material é compilado dentro do quadro "
                         "na primeira vez que cada material aparece.\n\n"
                         "ESM: mais barato, vaza luz quando o bloqueador está muito à frente.\n"
                         "VSM: filtrável em hardware, sofre com bloqueadores sobrepostos.\n"
                         "MSM: o único que aguenta geometria sobreposta (que este jogo tem muita), e o mais "
                         "caro."));
    AddWidget(path, "Raio do Borrão: %.2f texels", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.BlurRadius"))
        .RaceDisable(false)
        .PreFunc(hideUnlessFilterable)
        .Options(FloatSliderOptions()
                     .Tooltip("O borrão gaussiano aplicado sobre o próprio mapa.\n\n"
                              "É ESTE ajuste que define a largura da penumbra nos modos filtráveis. Em 0 o "
                              "formato filtrável não faz nada de útil: o formato é o que torna o borrão "
                              "legal, o borrão é o que amacia a borda.")
                     .Min(0.0f)
                     .Max(8.0f) // SHADOW_MAP_MAX_BLUR_RADIUS
                     .Step(0.1f)
                     .DefaultValue(2.0f) // SHADOW_MAP_DEFAULT_BLUR_RADIUS
                     .Format("%.2f"));
    AddWidget(path, "Expoente ESM: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.EsmExponent"))
        .RaceDisable(false)
        .PreFunc(hideUnlessEsm)
        .Options(FloatSliderOptions()
                     .Tooltip("Quão abruptamente o degrau reconstruído cai.\n\n"
                              "Baixo demais e a sombra vira um degradê lavado; alto demais e a exponencial "
                              "estoura o formato de armazenamento, a borda volta a ficar dura e ganha acne "
                              "por cima.")
                     .Min(5.0f)   // SHADOW_MAP_MIN_ESM_EXPONENT
                     .Max(200.0f) // SHADOW_MAP_MAX_ESM_EXPONENT
                     .Step(1.0f)
                     .DefaultValue(80.0f) // SHADOW_MAP_DEFAULT_ESM_EXPONENT
                     .Format("%.0f"));
    AddWidget(path, "Redução de Vazamento", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.BleedReduction"))
        .RaceDisable(false)
        .PreFunc(hideUnlessMoments)
        .Options(FloatSliderOptions()
                     .Tooltip("Corta a cauda baixa da distribuição antes de usar o resultado.\n\n"
                              "VSM e MSM dão um LIMITE SUPERIOR da fração iluminada, não a fração em si, e "
                              "onde dois bloqueadores em profundidades diferentes dividem o mesmo texel esse "
                              "limite fica frouxo -- o que se vê como uma sombra ficando translúcida no meio.\n\n"
                              "Acima de uns 50% a penumbra começa a endurecer de volta, que é justamente o "
                              "defeito que o modo existe para remover.")
                     .Min(0.0f)
                     .Max(0.99f)
                     .Step(0.01f)
                     .DefaultValue(0.20f) // SHADOW_MAP_DEFAULT_BLEED_REDUCTION
                     .IsPercentage());

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
                     .DefaultValue(0.75f) // SHADOW_MAP_DEFAULT_LADDER_LAMBDA
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

    // ===========================================================================================
    // Technique 5 -- screen-space shadow mask.
    // ===========================================================================================
    path = { "Qualidade das Sombras", "Espaço de Tela", SECTION_COLUMN_1 };
    AddSidebarEntry("Qualidade das Sombras", "Espaço de Tela", 3);

    auto hideUnlessScreenSpace = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ScreenSpace"), 0);
    };

    AddWidget(path, "Requer que a geometria da sala esteja capturada como caster do mundo.", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Superfícies fora dessa captura -- transparentes, água, partículas -- caem", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "automaticamente de volta para a amostragem normal das cascatas.", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);

    AddWidget(path, "Ativar Máscara em Espaço de Tela", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ScreenSpace"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Resolve a sombra do quadro inteiro numa máscara e borra ESSA máscara.\n\n"
            "É a única das cinco cuja penumbra é medida em PIXELS DE TELA, que é a unidade em que o "
            "serrilhado é realmente percebido: um borrão de 2 pixels tem 2 pixels em qualquer distância e "
            "em qualquer cascata, por mais grosso que seja o texel dela. Também reduz o receiver a uma "
            "única leitura.\n\n"
            "Funciona desenhando a geometria da sala uma vez com a matriz da câmera -- reaproveitando os "
            "mesmos buffers já enviados para as cascatas, então não há captura nova nem passe extra pelo "
            "interpretador."));
    AddWidget(path, "Raio do Borrão: %.1f px", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ScreenBlur"))
        .RaceDisable(false)
        .PreFunc(hideUnlessScreenSpace)
        .Options(FloatSliderOptions()
                     .Tooltip("Largura do borrão da máscara, em pixels de tela a 1080p (acompanha a "
                              "resolução, para o visual não mudar de monitor para monitor).")
                     .Min(0.0f)
                     .Max(16.0f) // SHADOW_MAP_MAX_SCREEN_BLUR
                     .Step(0.5f)
                     .DefaultValue(2.0f) // SHADOW_MAP_DEFAULT_SCREEN_BLUR
                     .Format("%.1f"));
    AddWidget(path, "Tolerância de Profundidade: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowQuality.ScreenDepthTolerance"))
        .RaceDisable(false)
        .PreFunc(hideUnlessScreenSpace)
        .Options(FloatSliderOptions()
                     .Tooltip("Quão diferentes duas profundidades podem ser antes de o borrão se recusar a "
                              "misturá-las.\n\n"
                              "Sem isso a máscara vaza por cima das silhuetas: uma parede sombreada borra o "
                              "termo dela no chão iluminado atrás, o que aparece como um halo. Apertado "
                              "demais e o borrão para de funcionar em qualquer superfície inclinada, porque "
                              "uma inclinação muda a profundidade ao longo do kernel por construção.")
                     .Min(0.1f)
                     .Max(100.0f)
                     .Step(0.5f)
                     .DefaultValue(6.0f) // SHADOW_MAP_DEFAULT_SCREEN_DEPTH_TOLERANCE
                     .Format("%.1f"));
}
} // namespace SohGui
