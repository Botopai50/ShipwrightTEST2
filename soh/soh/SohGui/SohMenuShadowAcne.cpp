#include "SohMenu.h"
#include "SohGui.hpp"
#include "soh/OTRGlobals.h"
#include "UIWidgets.hpp"
#include "soh/Enhancements/Graphics/ToonLighting.h"

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// "Correção de Acne" -- the methods for stopping a surface shadowing itself.
//
// Its own tab rather than a section of Qualidade das Sombras, because it answers a different question.
// That tab is about what a shadow's EDGE looks like; this one is about a surface being wrongly shadowed at
// all. They are independent: acne is just as possible with a beautiful edge.
//
// The methods stack, and each is a different place to intervene -- see fast/shadow_map.h for the full
// reasoning. Defaults are written out here, matching the convention in the other menu files, and each names
// the constant it must be kept in step with.

void SohMenu::AddMenuShadowAcne() {
    AddMenuEntry("Correção de Acne", CVAR_SETTING("Menu.ShadowAcneSidebarSection"));

    auto hideUnlessShadowMap = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                        SHADOW_MODE_SHADOW_MAP;
    };
    auto showUnlessShadowMap = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) ==
                        SHADOW_MODE_SHADOW_MAP;
    };
    // Every control below the master switch is hidden while it is off, so the page shows what is in play.
    auto hideUnlessAcne = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowAcne.Enabled"), 1);
    };

    // ===========================================================================================
    // What acne is, and the master switch.
    // ===========================================================================================
    WidgetPath path = { "Correção de Acne", "Métodos", SECTION_COLUMN_1 };
    AddSidebarEntry("Correção de Acne", "Métodos", 3);

    AddWidget(path, "O modo Shadow Map não está ativo.", WIDGET_TEXT).PreFunc(showUnlessShadowMap);
    AddWidget(path, "Ative-o em Wind Waker Style > Actor Shadows > Shadow System.", WIDGET_TEXT)
        .PreFunc(showUnlessShadowMap);

    AddWidget(path, "O que é acne", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Uma superfície fazendo sombra em si mesma: listras escuras no chão que, num", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "ângulo rasante, viram raios convergindo no horizonte.", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "A defesa padrão é o slope bias do rasterizador, aplicado enquanto o mapa de", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "profundidade é escrito, e normalmente ela basta. Estes métodos são para onde não", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);
    AddWidget(path, "basta: um texel muito grande, ou uma superfície quase de lado para a luz.", WIDGET_TEXT)
        .PreFunc(hideUnlessShadowMap);

    AddWidget(path, "Ativar Correção", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowAcne.Enabled"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Chave geral dos métodos abaixo.\n\n"
            "LIGADA por padrão, com os valores que se mostraram necessários na prática: um deslocamento "
            "pequeno pela normal, escalado pela inclinação, e mais nada.\n\n"
            "O slope bias do rasterizador continua sendo a defesa principal. Estes métodos cobrem onde ele "
            "não basta."));

    AddWidget(path, "Restaurar Tudo ao Padrão", WIDGET_BUTTON)
        .PreFunc(hideUnlessAcne)
        .Callback([](WidgetInfo& info) {
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowAcne.Enabled"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowAcne.NormalOffset"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowAcne.NormalTexels"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowAcne.SlopeScaled"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowAcne.SlopeMax"));
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Tooltip("Devolve os métodos abaixo à combinação recomendada."));

    // ===========================================================================================
    // Method 1 -- normal offset. The one that is correct in principle.
    // ===========================================================================================
    AddWidget(path, "Método 1: Deslocamento pela Normal", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessAcne);
    AddWidget(path, "Ativar Deslocamento pela Normal", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowAcne.NormalOffset"))
        .RaceDisable(false)
        .PreFunc(hideUnlessAcne)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Afasta o ponto de amostragem da superfície ao longo da NORMAL dela, antes de projetar.\n\n"
            "É o único método correto em princípio, e não um remendo: o erro que se está corrigindo é um "
            "deslocamento no espaço do mundo, e este também é.\n\n"
            "Como o deslocamento é ao longo da superfície e não da luz, ele NÃO descola a sombra do pé de "
            "quem a projeta -- que é o preço dos outros dois métodos.\n\n"
            "Comece por aqui. Na prática este e a Escala por Inclinação resolvem sozinhos."));
    AddWidget(path, "Distância: %.2f texels", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowAcne.NormalTexels"))
        .RaceDisable(false)
        .PreFunc(hideUnlessAcne)
        .Options(FloatSliderOptions()
                     .Tooltip("Em múltiplos do texel da cascata amostrada, não em unidades de mundo.\n\n"
                              "O erro que isto corrige é ele próprio do tamanho de um texel, então uma "
                              "distância fixa em unidades de mundo seria grande demais na cascata próxima "
                              "e pequena demais na distante.\n\n"
                              "Se ainda houver listras, aumente. Se as sombras começarem a encolher perto "
                              "dos contatos, diminua.")
                     .Min(0.0f)
                     .Max(8.0f) // SHADOW_MAP_MAX_ACNE_NORMAL_TEXELS
                     .Step(0.1f)
                     .DefaultValue(0.6f) // SHADOW_MAP_DEFAULT_ACNE_NORMAL_TEXELS
                     .Format("%.2f"));

    // ===========================================================================================
    // Method 2 -- slope scaling. Not an offset of its own; it shapes the other three.
    // ===========================================================================================
    AddWidget(path, "Método 2: Escala por Inclinação", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessAcne);
    AddWidget(path, "Escalar pela Inclinação", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowAcne.SlopeScaled"))
        .RaceDisable(false)
        .PreFunc(hideUnlessAcne)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Não é um deslocamento próprio: multiplica o deslocamento pela normal por quão de lado a "
            "superfície está "
            "em relação à luz.\n\n"
            "Acne é um problema de ângulo rasante. Uma superfície de frente para a luz não tem nenhum; uma "
            "de lado tem a profundidade disparando ao longo de um texel. Escalar pelo ângulo gasta a "
            "correção onde ela é necessária e quase nada onde não é."));
    AddWidget(path, "Limite da Escala: %.1fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowAcne.SlopeMax"))
        .RaceDisable(false)
        .PreFunc(hideUnlessAcne)
        .Options(FloatSliderOptions()
                     .Tooltip("Teto da multiplicação, e ele NÃO é opcional.\n\n"
                              "O fator cresce para infinito conforme a superfície fica de lado para a luz, "
                              "e um deslocamento sem limite ali joga a amostra para outra parte da cena "
                              "inteira.")
                     .Min(1.0f)
                     .Max(10.0f) // SHADOW_MAP_MAX_ACNE_SLOPE_MAX
                     .Step(0.1f)
                     .DefaultValue(3.5f) // SHADOW_MAP_DEFAULT_ACNE_SLOPE_MAX
                     .Format("%.1f"));

}
} // namespace SohGui
