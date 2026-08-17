#include "SohMenu.h"
#include <cstdlib> // std::abs for the resolution snap below
#include "SohGui.hpp"
#include "soh/OTRGlobals.h"
#include "UIWidgets.hpp"
#include "soh/Enhancements/Graphics/ToonLighting.h"

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// Offered shadow-map sizes. Keys are the actual resolution, which is what the CVar stores, so the combobox
// reads and writes the value the renderer uses rather than an index into this list.
static const std::map<int32_t, const char*> shadowMapResolutionLabels = {
    { 512, "512" },
    { 1024, "1024" },
    { 2048, "2048" },
    { 4096, "4096" },
};

// Keyed by ShadowMode (see ToonLighting.h) -- the three shadow systems are mutually exclusive.
static const std::map<int32_t, const char*> shadowModeLabels = {
    { SHADOW_MODE_VANILLA, "Vanilla" },
    { SHADOW_MODE_ACTOR, "Actor Shadows" },
    { SHADOW_MODE_SHADOW_MAP, "Shadow Map" },
};

// "Wind Waker Style" — the home for the Wind Waker-flavoured rendering features. The internal CVar keys
// keep their original "ToonLighting" / "WorldLighting" names (predating the GUI labels) so existing
// settings/saves are unaffected by label changes.
void SohMenu::AddMenuWindWakerStyle() {
    AddMenuEntry("Wind Waker Style", CVAR_SETTING("Menu.WindWakerStyleSidebarSection"));

    // ===========================================================================================
    // Cel Shading — relights actors/objects with a single dominant light and a soft toon ramp.
    // ===========================================================================================
    auto hideUnlessCelEnabled = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ToonLighting.Enabled"), 1);
    };
    WidgetPath path = { "Wind Waker Style", "Cel Shading", SECTION_COLUMN_1 };
    // 3 columns with the controls kept in column 1 (like the Audio page) so the sliders sit in a narrow
    // left strip and the game stays visible behind the menu while you tune the values.
    AddSidebarEntry("Wind Waker Style", "Cel Shading", 3);
    AddWidget(path, "Enable Cel Shading", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ToonLighting.Enabled"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Re-lights actors and objects with a single dominant light and a soft Wind Waker-style ramp. "
            "Only affects objects, not the static scene. Pairs well with cel-shaded texture packs."));
    AddWidget(path, "Options", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessCelEnabled);
    AddWidget(path, "Reset All to Defaults", WIDGET_BUTTON)
        .PreFunc(hideUnlessCelEnabled)
        .Callback([](WidgetInfo& info) {
            // Clearing each CVar drops it back to the slider's DefaultValue (the same value the renderer
            // falls back to), so this restores the default look without hardcoding the numbers twice.
            CVarClear(CVAR_ENHANCEMENT("Graphics.ToonLighting.RampCenter"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ToonLighting.RampSoftness"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ToonLighting.HighlightIntensity"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ToonLighting.ShadowIntensity"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ToonLighting.PointLightRange"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ToonLighting.UseNaviLight"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ToonLighting.TransitionTime"));
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Tooltip("Resets all the Cel Shading sliders below to their default values."));
    AddWidget(path, "Ramp Center", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ToonLighting.RampCenter"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCelEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("Where the dark-to-light transition sits. Higher = more of the surface "
                              "stays in shadow.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.5f)
                     .IsPercentage());
    AddWidget(path, "Ramp Softness", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ToonLighting.RampSoftness"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCelEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("Width of the transition band. Low = a hard cel edge; high = a softer "
                              "gradient.")
                     .Format("%.2f") // 2 decimals; the 0.01 step makes the drag land on hundredths (no snap)
                     .Min(0.01f)
                     .Max(0.2f)
                     .DefaultValue(0.02f));
    AddWidget(path, "Highlight Intensity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ToonLighting.HighlightIntensity"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCelEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("Brightness of the lit side. Higher = brighter highlights.")
                     .Min(0.0f)
                     .Max(2.0f)
                     .DefaultValue(0.6f)
                     .IsPercentage());
    AddWidget(path, "Shadow Intensity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ToonLighting.ShadowIntensity"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCelEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("How dark the shadow side gets. 0% = no shadow (flat), 100% = full "
                              "shadow down to ambient.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.6f)
                     .IsPercentage());
    AddWidget(path, "Point Light Range", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ToonLighting.PointLightRange"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCelEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("Extends how far a point light can remain an object's key light, as a "
                              "multiplier on its actual radius (key selection only -- the game's real "
                              "lighting is unchanged). Raise it so an orbiting fairy keeps lighting nearby "
                              "objects even when it swings to its far side. 1x = the light's literal range.")
                     .Format("%.1fx")
                     .Min(1.0f)
                     .Max(4.0f)
                     .DefaultValue(1.5f));
    AddWidget(path, "Use Navi as a Light Source", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ToonLighting.UseNaviLight"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCelEnabled)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Let Navi count as a candidate key light for cel shading. Navi blinks on/off and orbits Link, so "
            "leaving this on makes the lighting on nearby objects shift around with her. Turn it off to ignore "
            "Navi and keep the key light steady (the sun/moon or a torch wins instead)."));
    AddWidget(path, "Transition Time", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ToonLighting.TransitionTime"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCelEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("How long the key light takes to ease from one source to another. Higher "
                              "= slower, more deliberate travel between the sun and a fairy/torch.")
                     .Format("%.1fs")
                     .Min(0.1f)
                     .Max(6.0f)
                     .DefaultValue(1.0f));
    AddWidget(path, "Debug", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessCelEnabled);
    AddWidget(path, "Light Source Viewer", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("ToonLighting.ShowDebug"))
        .PreFunc(hideUnlessCelEnabled)
        .Options(CheckboxOptions().Tooltip(
            "Draws a debug ray from each actor for every candidate light (coloured by the light, longer "
            "when stronger), a cyan range ring around each point light, and a bold magenta needle down "
            "the chosen key light, so you can see which light is winning and where the key points."));
    AddWidget(path, "Highlight Lit Objects", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("ToonLighting.HighlightBands"))
        .PreFunc(hideUnlessCelEnabled)
        .Options(CheckboxOptions().Tooltip(
            "Renders every cel-shaded object as flat white on the lit side and flat black in shadow (the "
            "texture is discarded), so it is obvious which draws are being relit -- handy for confirming "
            "whether large surfaces like water or lava are getting relit."));

    // ===========================================================================================
    // Lights — Wind Waker flame-flicker tweaks (Misc) plus the cast light pools (Light Casting). On by
    // default. Internal CVar keys keep their "WorldLighting" names.
    // ===========================================================================================
    auto hideUnlessLightCastEnabled = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.Enabled"), 0);
    };
    // Navi's pool sliders need both Light Casting and Navi Light Casting on.
    auto hideUnlessNaviCast = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.Enabled"), 0) ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.UseNaviLight"), 1);
    };
    // Wild-fairy pool sliders need Light Casting and the Wild Fairies toggle on.
    auto hideUnlessWildFairyCast = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.Enabled"), 0) ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.OtherFairyLights"), 0);
    };
    // The Deku stick's cast-size slider needs Light Casting and the Deku Stick toggle on.
    auto hideUnlessDekuStickCast = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.Enabled"), 0) ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.DekuStickLight"), 1);
    };
    // Rotation Speed / Size Flicker sit with the cast-pool controls, but hide entirely while "Use Wind Waker
    // default movement" is on (the renderer pins them to the authentic 1x).
    auto hideUnlessCustomMovement = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.Enabled"), 0) ||
                        CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.WWDefaultMovement"), 1);
    };
    path.sidebarName = "Lights";
    AddSidebarEntry("Wind Waker Style", "Lights", 2); // Light Casting in column 1, Misc in column 2

    // --- Misc: scene-wide tweaks, independent of the cast pools (right column) ---
    path.column = SECTION_COLUMN_2;
    AddWidget(path, "Misc", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Hide Vanilla Torch Glow", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.HideVanillaGlow"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Hides the original flat, billboarded, flickering glow circle the game draws over torches and "
            "other glow lights (it clashes with the cast pools). Applies while Light Casting is on."));
    AddWidget(path, "Improve Flame Flicker", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.ImproveFlameFlicker"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Replaces the game's fast, jagged per-frame torch/flame flicker with a slow, organic Wind Waker "
            "flicker. Applied at the source, so it affects the vanilla scene lighting and Cel Shading even "
            "when Light Casting is off."));
    AddWidget(path, "Flicker Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.FlickerSpeed"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            info.options->disabled =
                !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldLighting.ImproveFlameFlicker"), 1);
            info.options->disabledTooltip = "Enable \"Improve Flame Flicker\" to adjust this.";
        })
        .Options(FloatSliderOptions()
                     .Tooltip("How often flames pick a new brightness for the Wind Waker flicker. Higher = "
                              "faster; lower = a lazier flame.")
                     .Format("%.2fx")
                     .Min(0.1f)
                     .Max(3.0f)
                     .DefaultValue(1.0f));
    AddWidget(path, "Navi's Light Tint", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.NaviSaturation"))
        .RaceDisable(false)
        .Options(FloatSliderOptions()
                     .Tooltip("Tints Navi's light toward her current colour. Her light is normally white, but "
                              "she changes colour when targeting (yellow on enemies, and so on); raise this to "
                              "let a little of that colour through. Applied at the source, so it tints her cast "
                              "pool, the objects she lights under Cel Shading, and the vanilla lighting alike. "
                              "0% = white.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.2f)
                     .IsPercentage());

    // --- Light Casting: the cast light pools ---
    // A slider with a "Reset" button to its right that clears only that slider's CVar (matching the
    // Cosmetics Editor's Silly tab). Drawn as a WIDGET_CUSTOM because the declarative widgets can't nudge the
    // cursor between the slider and a same-line button — the label sits above the bar, so the button has to
    // be dropped two rows to line up with it. `format` may be nullptr to use the default. The PreFunc hides
    // the whole row when light casting (and, for Navi rows, Navi casting) is off.
    auto addSliderWithReset = [&](const char* label, const char* cvar, float minVal, float maxVal, float defVal,
                                  const char* format, bool isPercentage, auto hideFunc, const char* tooltip) {
        AddWidget(path, label, WIDGET_CUSTOM).PreFunc(hideFunc).CustomFunction([=](WidgetInfo&) {
            float sliderWidth = ImGui::GetContentRegionAvail().x - 90.0f; // leave room for the Reset button
            if (sliderWidth < 80.0f) {
                sliderWidth = 80.0f;
            }
            auto opts = FloatSliderOptions()
                            .Tooltip(tooltip)
                            .Min(minVal)
                            .Max(maxVal)
                            .DefaultValue(defVal)
                            .Size(ImVec2(sliderWidth, 0.0f))
                            .Color(THEME_COLOR);
            if (format != nullptr) {
                opts.Format(format);
            }
            if (isPercentage) {
                opts.IsPercentage();
            }
            CVarSliderFloat(label, cvar, opts);
            // Drop the button down to the slider bar (the label is on the row above it).
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::CalcTextSize("g").y * 2));
            std::string resetId = "Reset##WWL_" + std::string(label);
            if (Button(resetId.c_str(),
                       ButtonOptions().Size(ImVec2(80.0f, 36.0f)).Padding(ImVec2(5.0f, 0.0f)).Color(THEME_COLOR))) {
                CVarClear(cvar);
                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
        });
    };
    // Light Casting fills the left column, grouped per light source — each group is [enable] + its cast
    // size/intensity, divided by a separator; the global pool-movement controls sit at the end.
    path.column = SECTION_COLUMN_1;
    AddWidget(path, "Light Casting", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Enable Light Casting", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.Enabled"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Casts a pool of light from each point light (torch, fairy, ...) onto the surrounding world "
            "geometry, Wind Waker-style. Affects only the static world, not actors/objects (lit by Cel "
            "Shading)."));
    // Pool movement (global) sits right under the master toggle.
    AddWidget(path, "Use Wind Waker default movement", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.WWDefaultMovement"))
        .RaceDisable(false)
        .PreFunc(hideUnlessLightCastEnabled)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Pins the pool's tumble and size pulse to the authentic Wind Waker rates. Turn off to reveal and "
            "set Rotation Speed and Size Flicker yourself."));
    AddWidget(path, "Rotation Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.RotationSpeed"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCustomMovement)
        .Options(FloatSliderOptions()
                     .Tooltip("Speed of the Wind Waker two-axis tumble that animates the pool's faceted "
                              "edges, as a multiplier on the authentic rate. 1.0 = authentic; 0 = static.")
                     .Format("%.2fx")
                     .Min(0.0f)
                     .Max(3.0f)
                     .DefaultValue(1.0f));
    AddWidget(path, "Size Flicker", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.SizeFlicker"))
        .RaceDisable(false)
        .PreFunc(hideUnlessCustomMovement)
        .Options(FloatSliderOptions()
                     .Tooltip("Depth of the Wind Waker size pulse -- the pool's dominant flicker. The orb "
                              "gently grows/shrinks on a slow random walk (re-rolled every ~0.2 s, eased). "
                              "1.0 = authentic (~5%); 0 = steady. (Navi is excluded -- she isn't a flame.)")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(3.0f)
                     .DefaultValue(1.0f));
    addSliderWithReset("Cast Size", CVAR_ENHANCEMENT("Graphics.WorldLighting.SphereSize"), 0.1f, 4.0f, 0.5f,
                       "%.2fx", false, hideUnlessLightCastEnabled,
                       "Size of each light's cast pool, as a multiplier on the light's radius. Smaller keeps "
                       "the pool tight around the source; larger spreads it wider.");
    addSliderWithReset("Light Intensity", CVAR_ENHANCEMENT("Graphics.WorldLighting.Intensity"), 0.0f, 2.0f,
                       0.2f, nullptr, true, hideUnlessLightCastEnabled, "Brightness of the cast light pools.");

    // Navi
    AddWidget(path, "WWLSepNavi", WIDGET_SEPARATOR).RaceDisable(false).PreFunc(hideUnlessLightCastEnabled);
    AddWidget(path, "Enable Navi Light Casting", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.UseNaviLight"))
        .RaceDisable(false)
        .PreFunc(hideUnlessLightCastEnabled)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Also cast a pool from Link's fairy (Navi). Navi darts around quickly, so her pool moves a lot."));
    addSliderWithReset("Navi Cast Size", CVAR_ENHANCEMENT("Graphics.WorldLighting.NaviSphereSize"), 0.1f, 4.0f,
                       0.75f, "%.2fx", false, hideUnlessNaviCast,
                       "Navi's pool size, separate from the main Cast Size, so you can keep Navi tight "
                       "without shrinking the torches.");
    addSliderWithReset("Navi Light Intensity", CVAR_ENHANCEMENT("Graphics.WorldLighting.NaviIntensity"), 0.0f,
                       2.0f, 0.2f, nullptr, true, hideUnlessNaviCast,
                       "Navi's pool brightness, separate from the main Light Intensity.");

    // Other fairies
    AddWidget(path, "WWLSepFairy", WIDGET_SEPARATOR).RaceDisable(false).PreFunc(hideUnlessLightCastEnabled);
    AddWidget(path, "Enable Other Fairy Light Casting", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.OtherFairyLights"))
        .RaceDisable(false)
        .PreFunc(hideUnlessLightCastEnabled)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Makes non-Navi fairies emit light (they don't in vanilla): the fairies that drift around places "
            "like Kokiri Forest, and the healing fairies found out in the world (the magic one casts a wider "
            "pool). Since it turns them into real light sources, they then cast light pools AND can light nearby "
            "objects via Cel Shading, the same as Navi. A cluster of them can make the lighting busy."));
    addSliderWithReset("Other Fairy Cast Size", CVAR_ENHANCEMENT("Graphics.WorldLighting.WildFairySphereSize"),
                       0.1f, 4.0f, 0.75f, "%.2fx", false, hideUnlessWildFairyCast,
                       "Pool size for non-Navi fairies (Kokiri Forest fairies + the healing fairies), separate "
                       "from torches and Navi. The magic (big) fairy is already larger than the rest.");
    addSliderWithReset("Other Fairy Intensity", CVAR_ENHANCEMENT("Graphics.WorldLighting.WildFairyIntensity"),
                       0.0f, 2.0f, 0.2f, nullptr, true, hideUnlessWildFairyCast,
                       "Pool brightness for non-Navi fairies, separate from the main Light Intensity.");

    AddWidget(path, "Debug", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessLightCastEnabled);
    AddWidget(path, "Show Light Spheres", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("WorldLighting.ShowLightSpheres"))
        .PreFunc(hideUnlessLightCastEnabled)
        .Options(CheckboxOptions().Tooltip(
            "Overlays a translucent faceted shell of each light's icosphere -- the volume used for its cast "
            "pool -- tinted by the light, so you can see where the pools are, their size, and their spin. "
            "(The renderer has no line primitive, so this is a shell rather than a true wireframe.)"));

    // Held Deku stick — its own light source. Unlike the casting groups above it feeds Cel Shading + Actor
    // Shadows even with Light Casting off, so it lives in the right column with its toggle always visible;
    // only its cast-size (a casting-pool control) hides when Light Casting is off.
    path.column = SECTION_COLUMN_2;
    AddWidget(path, "Deku Stick", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Enable Deku Stick Light Casting", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldLighting.DekuStickLight"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Makes a lit, held Deku stick a real light source at its burning tip (it isn't in vanilla). Like a "
            "torch it lights nearby objects via Cel Shading and casts their shadows, and -- with Light Casting "
            "on -- casts its own pool on the world. This one toggle controls all three."));
    addSliderWithReset("Deku Stick Cast Size", CVAR_ENHANCEMENT("Graphics.WorldLighting.DekuStickSphereSize"),
                       0.1f, 4.0f, 0.5f, "%.2fx", false, hideUnlessDekuStickCast,
                       "The held Deku stick's pool size, separate from torches, so you can size the stick's "
                       "pool on its own.");

    // ===========================================================================================
    // Actor Shadows — Wind Waker-style shape shadows: each actor casts its own silhouette onto the ground
    // (following slopes), from the same key light Cel Shading uses, with a soft edge. Replaces the vanilla
    // blob/feet shadows. Internal CVar keys use "WorldShadows"; the UI says "Actor Shadows".
    // ===========================================================================================
    // The tuning sliders below shape the stencil-volume silhouettes, so they only apply to Actor Shadows --
    // not to Vanilla (nothing to tune) and not to Shadow Map (its own cascade settings).
    auto hideUnlessShadowsEnabled = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                        SHADOW_MODE_ACTOR;
    };
    auto hideUnlessAnyShadowSystem = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) ==
                        SHADOW_MODE_VANILLA;
    };
    path.sidebarName = "Actor Shadows";
    path.column = SECTION_COLUMN_1;
    AddSidebarEntry("Wind Waker Style", "Actor Shadows", 3);
    AddWidget(path, "Shadow System", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"))
        .RaceDisable(false)
        .Options(ComboboxOptions()
                     .DefaultIndex(SHADOW_MODE_VANILLA)
                     .ComboMap(shadowModeLabels)
                     .Tooltip("Which shadow system draws. Only one is active at a time.\n\n"
                              "Vanilla: the original game's shadows (Link's feet, the NPC/enemy circles, the "
                              "horse shadow, the sign and snake-statue texture shadows).\n\n"
                              "Actor Shadows: a shape-based drop shadow per actor -- its own silhouette cast "
                              "from the single key light Cel Shading picks, wrapped onto the real ground so it "
                              "follows slopes and bumps. Uses the Cel Shading key selection, but works whether "
                              "or not Cel Shading itself is on.\n\n"
                              "Shadow Map: cascaded depth-map shadows, so the world shadows itself and actors "
                              "cast onto it. Direct3D 11 only -- other backends fall back to Vanilla."));
    AddWidget(path, "Suppress Vanilla Shadows", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldShadows.SuppressVanillaShadows"))
        .RaceDisable(false)
        .PreFunc(hideUnlessAnyShadowSystem)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Hide the original game's actor shadows (Link's feet, the NPC/enemy circles, the horse shadow, "
            "the sign and snake-statue texture shadows) so only the selected system's shadows show. Turn off "
            "to draw both."));
    AddWidget(path, "Options", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowsEnabled);
    AddWidget(path, "Reset All to Defaults", WIDGET_BUTTON)
        .PreFunc(hideUnlessShadowsEnabled)
        .Callback([](WidgetInfo& info) {
            CVarClear(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.WorldShadows.SuppressVanillaShadows"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.WorldShadows.Opacity"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.WorldShadows.EdgeSoftness"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.WorldShadows.Length"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.WorldShadows.SlabDepth"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.WorldShadows.SlabRise"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.WorldShadows.MaxDistance"));
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Tooltip("Resets all the Actor Shadows sliders below to their default values."));
    AddWidget(path, "Opacity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldShadows.Opacity"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowsEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("How dark the shadow's core is. 0 = invisible; higher = darker.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.2f)
                     .IsPercentage());
    AddWidget(path, "Edge Softness", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldShadows.EdgeSoftness"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowsEnabled)
        .Options(IntSliderOptions()
                     .Tooltip("Smooths the shadow's outline: edge cells the silhouette only partially covers "
                              "render lighter, anti-aliasing the shape. 0 = hard edge; 1 = one lighter step; "
                              "2 = a finer ramp plus a slightly wider fringe.")
                     .Min(0)
                     .Max(2)
                     .DefaultValue(0)
                     .ShowButtons(true)
                     .Format("%d"));
    AddWidget(path, "Length", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldShadows.Length"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowsEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("How long the shadow may get. The key light is raised toward straight-overhead "
                              "before projecting, so a low light still casts a short shadow tucked under the "
                              "actor (like the vanilla shadow). Lower = always short and steep; higher = lets "
                              "a low light stretch the shadow out further.")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.2f));
    AddWidget(path, "Slab Depth", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldShadows.SlabDepth"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowsEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("How far below the feet the shadow conforms to the ground. The shadow is a thin "
                              "stencil 'slab' at the feet that wraps onto whatever ground is inside it. Higher = "
                              "follows ground that dips further (steeper inclines), but past a ledge the shadow "
                              "creeps further down the drop. Lower = clings tight to the feet and won't spill "
                              "over cliff edges, but may clip on steep slopes.")
                     .Format("%.0f")
                     .Min(5.0f)
                     .Max(200.0f)
                     .DefaultValue(8.0f));
    AddWidget(path, "Slab Rise", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldShadows.SlabRise"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowsEnabled)
        .Options(FloatSliderOptions()
                     .Tooltip("How far ABOVE the feet the shadow can climb onto rising ground. Raise this so the "
                              "shadow still appears where an incline rises higher than the actor's feet (without "
                              "it, the shadow vanishes on up-slopes). Too high starts to catch the actor's own "
                              "lower legs, so keep it just above the ground rise you need.")
                     .Format("%.0f")
                     .Min(0.0f)
                     .Max(120.0f)
                     .DefaultValue(8.0f));
    AddWidget(path, "Render Distance: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WorldShadows.MaxDistance"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowsEnabled)
        .Options(IntSliderOptions()
                     .Tooltip("Performance: actors farther than this from the camera get no shape shadow (each "
                              "shadow rebuilds and redraws the actor's whole silhouette, so distant ones cost "
                              "more than they're worth). Lower to gain frames in crowded scenes; raise for "
                              "shadows that stay visible into the distance.")
                     .Min(300)
                     .Max(5000)
                     .DefaultValue(550)
                     .ShowButtons(true)
                     .Format("%d"));
    AddWidget(path, "Debug", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowsEnabled);
    AddWidget(path, "Show Shadow Volume", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("WorldShadows.ShowVolume"))
        .PreFunc(hideUnlessShadowsEnabled)
        .Options(CheckboxOptions().Tooltip(
            "Draws the actual 3D shadow volume translucently so you can see its shape: black top/bottom caps, "
            "blue side walls. The ground inside this volume is what gets shadowed."));

    // Shadow Map tuning. Shown only in that mode, next to the Actor Shadows sliders it replaces, because the
    // two systems share nothing: these describe a depth map and its cascades, those describe a stencil
    // silhouette. Defaults here are written out rather than pulled from fast/shadow_map.h so the menu builds
    // without the renderer's headers -- they must be kept in step with it, and each one names its constant.
    auto hideUnlessShadowMap = [](WidgetInfo& info) {
        info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                        SHADOW_MODE_SHADOW_MAP;
    };
    AddWidget(path, "Opções", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Restaurar Tudo ao Padrão", WIDGET_BUTTON)
        .PreFunc(hideUnlessShadowMap)
        .Callback([](WidgetInfo& info) {
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.Strength"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.Resolution"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.ActorResolution"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.CascadeCount"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.Split0"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.Split1"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.Split2"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.FilterWidth"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.EdgeHardness"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.EdgeHardnessFar"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.DepthBias"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.SlopeBias"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.NormalOffset"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.PlaneGradientLimit"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.PlaneSoftFalloff"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.MaxAnisoTaps"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.EdgeScreenWidth"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.AnisoSpacing"));
            CVarClear(CVAR_DEVELOPER_TOOLS("ShadowMap.ViewSlice"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.MinHardnessScale"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.MinElevation"));
            CVarClear(CVAR_ENHANCEMENT("Graphics.ShadowMap.CasterDrawRadius"));
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        })
        .Options(ButtonOptions().Tooltip("Devolve todas as opções do Shadow Map abaixo aos valores padrão."));
    AddWidget(path, "Intensidade", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.Strength"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("O quão escura fica uma superfície totalmente na sombra.\n\n0% = nenhuma sombra visível; 100% = preto.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.5f) // SHADOW_MAP_DEFAULT_STRENGTH
                     .IsPercentage());
    // Snapped to the offered sizes before the widget draws. The combobox looks its current value up with
    // map::at and throws on anything not in the list, and this CVar is reachable from the console and was a
    // free slider in an earlier build -- so a stray value is a crash on opening the menu, not a stray value.
    AddWidget(path, "Resolução", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.Resolution"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP;
            if (info.isHidden) {
                return;
            }
            const int32_t offered[] = { 512, 1024, 2048, 4096 };
            int32_t current = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowMap.Resolution"), 4096);
            int32_t nearest = offered[0];
            for (int32_t candidate : offered) {
                if (std::abs(candidate - current) < std::abs(nearest - current)) {
                    nearest = candidate;
                }
            }
            if (nearest != current) {
                CVarSetInteger(CVAR_ENHANCEMENT("Graphics.ShadowMap.Resolution"), nearest);
            }
        })
        .Options(ComboboxOptions()
                     .DefaultIndex(4096) // SHADOW_MAP_DEFAULT_RESOLUTION
                     .ComboMap(shadowMapResolutionLabels)
                     .Tooltip("Tamanho do mapa de cada faixa, por lado. É o maior controle de qualidade "
                              "que existe aqui: toda borda de sombra é desenhada nessa grade, então dobrar "
                              "esse número reduz à metade o tamanho dos degraus no contorno da sombra.\n\n"
                              "Também é o maior custo, e ele cresce com o QUADRADO do número. Cada faixa "
                              "ganha o próprio mapa nesse tamanho, e existem dois conjuntos, um para o "
                              "cenário e outro para os personagens: cinco mapas no total. Em 4096 isso dá "
                              "cerca de 168 MB de memória de vídeo, contra 10 MB em 1024. Se o jogo estiver "
                              "pesado, baixe esta opção primeiro."));
    // Same snapping guard as the world layer's, and for the same reason: the combobox throws on a value
    // that is not in its map, and this CVar is reachable from the console.
    AddWidget(path, "Resolução (Personagens)", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.ActorResolution"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                            SHADOW_MODE_SHADOW_MAP;
            if (info.isHidden) {
                return;
            }
            const int32_t offered[] = { 512, 1024, 2048, 4096 };
            int32_t current = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.ShadowMap.ActorResolution"),
                                             SHADOW_MAP_DEFAULT_ACTOR_RESOLUTION);
            int32_t nearest = offered[0];
            for (int32_t candidate : offered) {
                if (std::abs(candidate - current) < std::abs(nearest - current)) {
                    nearest = candidate;
                }
            }
            if (nearest != current) {
                CVarSetInteger(CVAR_ENHANCEMENT("Graphics.ShadowMap.ActorResolution"), nearest);
            }
        })
        .Options(ComboboxOptions()
                     .DefaultIndex(SHADOW_MAP_DEFAULT_ACTOR_RESOLUTION)
                     .ComboMap(shadowMapResolutionLabels)
                     .Tooltip("Tamanho do mapa das sombras dos PERSONAGENS, escolhido à parte do cenário.\n\n"
                              "Vale baixar esta antes da outra. Os mapas dos personagens são redesenhados "
                              "todo quadro, porque os personagens se mexem, enquanto os do cenário são "
                              "reaproveitados enquanto a câmera fica parada — e o custo de um mapa é o "
                              "mesmo quer tenha muita coisa dentro ou pouca, porque limpá-lo já escreve a "
                              "superfície inteira.\n\n"
                              "O que se perde é pouco: personagens são pequenos, ficam perto e projetam no "
                              "chão logo à frente, então o mapa deles já estava sobrando em 4096. Nunca "
                              "passa da resolução do cenário; igualar as duas desliga a separação."));
    AddWidget(path, "Quantidade de Faixas: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.CascadeCount"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(IntSliderOptions()
                     .Tooltip("Quantas das três faixas de distância abaixo são realmente construídas.\n\n"
                              "Isto NÃO é um ajuste de qualidade. As distâncias são absolutas, então "
                              "diminuir a quantidade não espalha o mesmo alcance por menos mapas: ele CORTA o "
                              "alcance na última faixa ativa, e as sombras simplesmente somem dali para "
                              "frente. Diminua para ganhar FPS, sabendo que as sombras distantes vão junto.")
                     .Min(1)
                     .Max(3)
                     .DefaultValue(3) // SHADOW_MAP_DEFAULT_CASCADES
                     .ShowButtons(true)
                     .Format("%d"));

    AddWidget(path, "Distâncias", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Faixa Próxima Termina Em: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.Split0"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Onde a primeira faixa termina, em unidades do mundo (o Link tem cerca de "
                              "60 de altura).\n\n"
                              "Todas as faixas recebem um mapa da mesma resolução, então uma faixa mais "
                              "CURTA gasta esse mapa em menos chão e as sombras dela saem mais nítidas. Esta "
                              "primeira cobre o que está em volta do Link, então é ela que decide o quanto a "
                              "sombra dele fica definida. Diminua para sombras mais nítidas de perto.")
                     .Format("%.0f")
                     .Min(50.0f)
                     .Max(600.0f)
                     .DefaultValue(350.0f)); // SHADOW_MAP_DEFAULT_SPLIT_0
    AddWidget(path, "Faixa Média Termina Em: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.Split1"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Onde a segunda faixa termina. Mantenha as distâncias em ordem crescente: se "
                              "uma ficar abaixo da faixa anterior, o renderizador a empurra de volta para cima "
                              "e aquela faixa acaba não cobrindo nada.\n\n"
                              "Este é o controle de nitidez da distância média. O mapa de uma faixa é "
                              "esticado sobre a largura da visão na borda MAIS DISTANTE dela, então a última "
                              "faixa fica grosseira independentemente de onde comece. Empurrar esta para mais "
                              "longe entrega mais da cena para uma faixa que ainda está boa, ao custo de a "
                              "última começar mais tarde.")
                     .Format("%.0f")
                     .Min(100.0f)
                     .Max(3000.0f)
                     .DefaultValue(2500.0f)); // SHADOW_MAP_DEFAULT_SPLIT_1
    AddWidget(path, "Faixa Distante Termina Em: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.Split2"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Onde a terceira faixa termina. Normalmente é nela que cai a sombra de um "
                              "prédio, então vale apertar se as sombras de média distância estiverem "
                              "grosseiras.\n\n"
                              "É também onde TODA sombra acaba: a última faixa ativa é a distância de "
                              "desenho, e com as três faixas padrão é esta. Aumentar estica o mesmo mapa "
                              "sobre mais chão, então as sombras distantes ficam mais quadriculadas em vez "
                              "de melhores. Diminuir deixa tudo dentro do novo alcance mais nítido e "
                              "simplesmente termina as sombras mais cedo.")
                     .Format("%.0f")
                     .Min(300.0f)
                     .Max(12000.0f)
                     .DefaultValue(6000.0f)); // SHADOW_MAP_DEFAULT_SPLIT_2

    AddWidget(path, "Bordas", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Desfoque", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.FilterWidth"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("O quanto o filtro de suavização se espalha em volta de cada borda de "
                              "sombra, em células do mapa.\n\n"
                              "É isto que esconde os degraus do contorno, e não sai de graça: o que ele "
                              "suaviza é a borda inteira, então exagerar deixa a sombra sem forma nenhuma. "
                              "Aumente só até os degraus sumirem, e depois use a Nitidez abaixo para "
                              "recuperar a definição.")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(1.5f)
                     .DefaultValue(0.6f)); // SHADOW_MAP_DEFAULT_FILTER_WIDTH
    AddWidget(path, "Nitidez (Perto)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.EdgeHardness"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Devolve à borda desfocada um contorno definido, perto da câmera.\n\n"
                              "Ele trabalha sobre o resultado do desfoque em vez de desfazê-lo: o degradê "
                              "suave do filtro é remapeado de modo que o meio dele volte a ser uma borda. "
                              "Isso mantém o posicionamento em fração de célula que o desfoque deu, que é o "
                              "que impede o contorno de parecer uma escada, e ao mesmo tempo devolve uma "
                              "sombra com forma.\n\n"
                              "0 deixa o desfoque como está; 1 deixa a borda quase dura.")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.6f)); // SHADOW_MAP_DEFAULT_EDGE_HARDNESS
    AddWidget(path, "Nitidez (Longe)", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.EdgeHardnessFar"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("O mesmo controle, só que para a faixa mais distante. O valor é interpolado "
                              "entre os dois ao longo das faixas.\n\n"
                              "As células distantes são bem maiores, então lá longe o desfoque cobre muito "
                              "mais chão e uma sombra distante some bem antes de uma sombra perto sumir. Por "
                              "isso este valor costuma ficar mais alto que o de perto.")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.6f)); // SHADOW_MAP_DEFAULT_EDGE_HARDNESS_FAR
    AddWidget(path, "Ajuste de Profundidade", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Ajuste Fixo", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.DepthBias"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Um empurrão fixo na comparação de sombra, em unidades do mundo (o Link tem "
                              "cerca de 60 de altura). A mesma distância física em todas as faixas, "
                              "independentemente do tamanho delas.\n\n"
                              "É o mais grosseiro dos três: ele desliza a comparação ao longo do raio de "
                              "luz, então cada unidade daqui é também uma unidade de sombra se descolando de "
                              "quem a projeta. Aumente até as listras de sombra pararem, e pare aí: dali "
                              "para frente você só está comprando um vão embaixo dos pés do objeto.")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(8.0f)
                     .DefaultValue(0.60f)); // SHADOW_MAP_DEFAULT_DEPTH_BIAS_WORLD
    AddWidget(path, "Ajuste por Inclinação", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.SlopeBias"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Um multiplicador de o quanto a profundidade da própria superfície muda ao "
                              "longo de uma célula do mapa.\n\n"
                              "Este é o que realmente ataca as listras de sombra, porque não custa nada onde "
                              "não há problema: uma superfície virada para a luz quase não tem inclinação e "
                              "quase não recebe empurrão, enquanto uma de lado para a luz, onde a "
                              "profundidade dispara dentro de uma célula e as listras aparecem, recebe "
                              "bastante. Use este antes do Ajuste Fixo.\n\n"
                              "Cada faixa tem seu próprio limite, para que uma célula distante, com várias "
                              "unidades de largura, não saia do controle.")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(16.0f)
                     .DefaultValue(1.0f)); // SHADOW_MAP_DEFAULT_SLOPE_BIAS
    AddWidget(path, "Afastamento pela Normal", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.NormalOffset"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("O quanto a superfície é empurrada na direção da própria normal antes da "
                              "comparação, em células do mapa, proporcional ao quanto ela está de lado para "
                              "a luz.\n\n"
                              "Os outros dois deslizam a amostra AO LONGO do raio de luz, o que a mantém "
                              "dentro do mesmo polígono. Este a move para o lado, para fora da superfície que "
                              "está causando o problema. É por isso que ele resolve superfícies curvas e "
                              "rasantes que os outros não resolvem, e por isso que descola menos a sombra "
                              "pelo que corrige.\n\n"
                              "Exagerar faz as sombras começarem a fugir dos cantos.")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(8.0f)
                     .DefaultValue(0.0f)); // SHADOW_MAP_DEFAULT_NORMAL_OFFSET

    // The receiver-plane gradient's bound. Exposed to be TESTED, not tuned -- see the note where it is read
    // in ToonLighting.cpp. Default is the value it has always had, so leaving both alone changes nothing.
    AddWidget(path, "Limite do Gradiente do Plano", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.PlaneGradientLimit"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Até onde vai a correção que faz cada amostra do filtro comparar contra o "
                              "plano da própria superfície, em vez de contra um único ponto dela.\n\n"
                              "Essa correção precisa de um teto: na silhueta de um objeto o cálculo dela "
                              "não significa nada, e sem teto ela abriria um buraco na sombra. Mas quando o "
                              "teto é atingido, a correção passa a comparar contra um plano mais achatado "
                              "que a superfície real, e as amostras das bordas do filtro discordam do "
                              "centro. Isso vira faixas de auto-sombreamento — acne fabricada justamente "
                              "pelo termo que existe para evitá-la.\n\n"
                              "Este controle existe para VARRER, não para ajustar. Ligue a Visão de "
                              "Diagnóstico 6: o vermelho é exatamente onde este teto está sendo atingido. "
                              "Suba e desça este valor olhando as visões 5 e 6 juntas. Se as faixas "
                              "acompanharem o vermelho, o teto é a causa; se não acompanharem, a causa está "
                              "no próprio mapa de profundidade.\n\n"
                              "O padrão 3.2 equivale a cerca de 83° entre a superfície e a luz.")
                     .Format("%.2f")
                     .Min(0.1f)
                     .Max(64.0f)
                     .DefaultValue(5.50f)); // SHADOW_MAP_DEFAULT_PLANE_GRADIENT_LIMIT
    AddWidget(path, "Espaçamento das Amostras", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.AnisoSpacing"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Distância entre as amostras da fileira acima, em células do mapa.\n\n"
                              "2,0 encosta uma na outra: cada amostra bilinear cobre um bloco de 2x2 "
                              "células, então esse é o maior espaçamento que não deixa célula nenhuma sem "
                              "ser lida. Mas não é o mais LISO — cada amostra pesa sua vizinhança como uma "
                              "barraca, e barracas que apenas se encostam ainda afundam no meio do caminho, "
                              "o que faz a fileira parecer um enfileirado de amostras separadas em vez de "
                              "um borrão só.\n\n"
                              "1,0 sobrepõe as barracas pela metade e some com esse afundamento.\n\n"
                              "O que custa é ALCANCE: a fileira mede amostras × espaçamento células de "
                              "qualquer jeito, então metade do espaçamento cobre metade do trecho com a "
                              "mesma quantidade de amostras. Densidade e alcance se pagam um com o outro "
                              "aqui — para ter os dois, aumente a quantidade de amostras.")
                     .Format("%.2f")
                     .Min(0.25f)
                     .Max(2.0f)
                     .DefaultValue(1.0f)); // SHADOW_MAP_DEFAULT_ANISO_SPACING
    AddWidget(path, "Borda em Largura de Tela", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.EdgeScreenWidth"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Mede a rampa da borda dura em PIXELS DE TELA em vez de em cobertura. Só faz diferença com a "
            "Nitidez da Borda acima de zero.\n\n"
            "A rampa sempre teve largura fixa em cobertura, e cobertura não é o que o olho lê. Quantos "
            "pixels de tela essa rampa ocupa depende de quão rápido a cobertura muda naquela superfície, e "
            "isso varia muito: num chão de frente para a luz pode ser uma fração de pixel, o que serrilha e "
            "formiga em movimento; numa parede rasante à luz o mesmo número se espalha por vários pixels e "
            "vira borrão numa borda que era para ser dura. Um valor só não serve para os dois, porque não "
            "está medindo a coisa que decide a aparência.\n\n"
            "Com isto a rampa fica em cerca de três quartos de pixel em qualquer superfície: borda dura com "
            "antisserrilhado suficiente para não formigar, igual no chão e na parede. Para um visual cel é "
            "a diferença entre uma borda NÍTIDA e uma borda apenas estreita."));
    AddWidget(path, "Piso da Atenuação por Incidência", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.MinHardnessScale"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Quanto da Nitidez da Borda sobrevive nas superfícies de lado para a luz. 1 "
                              "desliga a atenuação e deixa a borda igualmente dura em toda parte.\n\n"
                              "A atenuação existe por um bom motivo: traçar uma linha dura sobre um "
                              "contorno mal amostrado imprime os degraus dele como facetas. Ela é a defesa "
                              "contra os dentes — e a defesa é justamente abrir mão da borda dura ali.\n\n"
                              "Com as Amostras na Direção da Fuga ligadas esse contorno deixa de ser mal "
                              "amostrado, e aí a atenuação passa a cobrar um preço por um problema que já "
                              "foi resolvido de outro jeito. Suba para 1 DEPOIS de ligar as amostras, nunca "
                              "antes: sem elas, 1 devolve os dentes em cheio.")
                     .Format("%.2f")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(1.0f)); // SHADOW_MAP_MIN_EDGE_HARDNESS_SCALE
    AddWidget(path, "Amostras na Direção da Fuga: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.MaxAnisoTaps"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(IntSliderOptions()
                     .Tooltip("Alarga o filtro APENAS na direção em que a superfície foge da luz. 1 desliga "
                              "e deixa o filtro quadrado de sempre.\n\n"
                              "Este é o único controle do painel que ataca os DENTES, e ele é de natureza "
                              "diferente de todos os outros. As listras de acne são uma comparação errada, "
                              "e um ajuste de profundidade resolve — é o que todos os outros fazem. Os "
                              "dentes não são isso: numa superfície virando de lado para a luz, o mapa quase "
                              "não tem resolução na direção em que ela se afasta, então a borda da sombra "
                              "quantiza em degraus de uma célula dividida pelo seno do ângulo. A 83° isso "
                              "são oito células por degrau; a 87°, vinte. Nenhum ajuste move esses degraus, "
                              "porque nada está sendo mal comparado — a borda está sendo desenhada numa "
                              "resolução que não existe.\n\n"
                              "Um degrau desses não pode ser resolvido, mas pode ser MEDIADO: isso troca a "
                              "escadinha dura por um degradê suave na mesma direção — a mesma informação "
                              "faltando, apresentada como penumbra em vez de serrilha. E só nessa direção: "
                              "na transversal o mapa amostra bem, e alargar ali só borraria uma borda que "
                              "estava certa.\n\n"
                              "Custa duas amostras por passo, contra quatro do filtro quadrado inteiro — "
                              "então 4 aqui é o dobro de buscas, e só nos pixels cujo ângulo pede.\n\n"
                              "Comece em 4. Se os dentes viraram um degradê macio, era aliasing de "
                              "amostragem; se não mudaram nada, não era, e o problema está no mapa de "
                              "profundidade.")
                     .Min(1)
                     .Max(8)
                     .DefaultValue(5) // SHADOW_MAP_DEFAULT_MAX_ANISO_TAPS
                     .ShowButtons(true)
                     .Format("%d"));
    AddWidget(path, "Estreitar o Filtro no Limite", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.PlaneSoftFalloff"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "O conserto candidato para as faixas que o controle acima diagnostica. Deixe desligado para o "
            "comportamento de sempre, ligue para comparar os dois lado a lado.\n\n"
            "Cortar o gradiente no teto não torna a correção segura, só a torna errada de um jeito "
            "específico, e o erro é o produto de dois fatores: o quanto o gradiente passou do teto e o "
            "quanto o filtro se afasta do centro. Nada consegue corrigir o primeiro, então isto encolhe o "
            "segundo exatamente na mesma proporção — o filtro é multiplicado por teto/gradiente — e o erro "
            "fica preso no valor que tinha NO teto.\n\n"
            "O ganho é ser contínuo: sem isso, uma face que passa do teto e a vizinha que não passa se "
            "comportam de maneiras diferentes, e a fronteira entre as duas é a aresta entre elas. É assim "
            "que um limiar desenha os triângulos da malha na tela.\n\n"
            "O custo é largura de filtro nas superfícies muito inclinadas em relação à luz: conforme o "
            "gradiente dispara, o filtro encolhe até virar praticamente uma amostra só, e a borda da sombra "
            "fica mais dura ali. São superfícies que quase não recebem luz, que é o mesmo argumento pelo "
            "qual a faixa de incidência já alivia nelas."));

    AddWidget(path, "Luz e Alcance", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    AddWidget(path, "Altura Mínima do Sol", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.MinElevation"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Quão baixo o sol pode ficar antes de as sombras serem projetadas a partir "
                              "dele, medido como o seno do ângulo acima do horizonte (0,5 é trinta graus).\n\n"
                              "Um sol na linha do horizonte estica toda sombra até o infinito, o que parece "
                              "errado bem antes de ser geometricamente errado, e ainda desperdiça os mapas "
                              "numa área muito maior que a cena. Só a altura é levantada: a direção para "
                              "onde as sombras apontam continua a mesma.")
                     .Format("%.2f")
                     .Min(0.1f)
                     .Max(0.95f)
                     .DefaultValue(0.60f)); // SHADOW_MAP_DEFAULT_MIN_ELEVATION
    AddWidget(path, "Alcance Fora da Tela: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.ShadowMap.CasterDrawRadius"))
        .RaceDisable(false)
        .PreFunc(hideUnlessShadowMap)
        .Options(FloatSliderOptions()
                     .Tooltip("Até que distância fora da tela um objeto continua sendo desenhado apenas "
                              "para poder projetar sombra.\n\n"
                              "Normalmente o jogo para de desenhar o que a câmera não vê, mas uma árvore "
                              "atrás de você ainda joga a sombra dela na sua frente. Sem isso, as sombras "
                              "piscariam a cada vez que você virasse a câmera. O teste segue a luz em vez de "
                              "usar um círculo simples, então esta é a margem em volta do caminho da sombra, "
                              "não em volta do objeto.\n\n"
                              "Diminua para ganhar FPS em áreas abertas; o preço é pontos de referência "
                              "distantes perderem a sombra ao sair da tela.")
                     .Format("%.0f")
                     .Min(400.0f)
                     .Max(4000.0f)
                     .DefaultValue(1500.0f)); // SHADOW_MAP_DEFAULT_CASTER_DRAW_RADIUS

AddWidget(path, "Depuração", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessShadowMap);
    // Live readout of which light won the frame. A lot of policy decides the single direction the cascades
    // get, so without this a hierarchy that picked the wrong light is indistinguishable from one that
    // picked the right light and aimed it badly. Same live-name trick as the caster census below.
    // The CVar key still says ShowCascadeBounds because that is what the first view did and renaming it
    // would silently reset everyone's saved value. The label does not, because the slider now selects
    // between nine views and only one of them is about cascade bounds.
    AddWidget(path, "Visão de Diagnóstico: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_DEVELOPER_TOOLS("ShadowMap.ShowCascadeBounds"))
        .PreFunc(hideUnlessShadowMap)
        .Options(IntSliderOptions()
                     .Tooltip("0 = desligado.\n\n"
                              "As visões 1 e 2 mostram o que o sistema de sombras PRODUZIU. As visões 3 a 8 "
                              "mostram o que ele RECEBEU -- use estas quando a sombra sai com a FORMA errada "
                              "(facetada, triangular, escadinha) em vez de no lugar errado. Todos os outros "
                              "controles deste painel agem sobre o resultado da comparação, então conseguem "
                              "deixar esse tipo de defeito menos visível e nunca conseguem dizer de onde ele "
                              "veio.\n\n"
                              "1 = pinta tudo que está FORA da área de uma faixa como totalmente sombreado. "
                              "Uma superfície fora dela é silenciosamente considerada iluminada, então uma "
                              "sombra que para no limite da faixa fica idêntica a uma que nunca foi "
                              "projetada. Esta é a única forma de distinguir as duas.\n\n"
                              "2 = colore as duas camadas de projeção em vez de sombrear com elas. VERDE "
                              "onde o cenário bloqueia a luz, VERMELHO onde um personagem bloqueia. Use para "
                              "descobrir se algo está sendo capturado e em qual camada.\n\n"
                              "3 = a normal da superfície sobre a qual todo o ajuste de profundidade é "
                              "construído, como cor. Manchas chapadas de uma cor só, numa superfície que "
                              "deveria variar suavemente, são os próprios triângulos da malha aparecendo.\n\n"
                              "4 = de onde veio essa normal. VERDE = a normal de vértice do desenho, "
                              "escurecendo conforme a normal interpolada encurta. VERMELHO = o desenho não "
                              "tem normal, então é usada uma normal de face recuperada das derivadas de "
                              "tela -- constante ao longo de um triângulo inteiro.\n\n"
                              "5 = a cobertura crua do filtro, antes de a definição de borda reescrevê-la. "
                              "Compare com a imagem sombreada: facetado aqui também significa que a "
                              "comparação ou o mapa desenhou o defeito; liso aqui significa que a definição "
                              "de borda desenhou.\n\n"
                              "6 = o gradiente do plano da superfície. VERDE cresce com o tamanho dele, "
                              "VERMELHO marca onde o limite interno foi atingido e a correção deixou de ser "
                              "exata.\n\n"
                              "7 = em qual faixa cada pixel caiu: vermelho, verde e azul, da mais próxima "
                              "para a mais distante. Use para incluir ou descartar a escolha de faixa.\n\n"
                              "8 = a definição de borda depois da atenuação por incidência. VERMELHO dura, "
                              "VERDE macia. A atenuação lê a normal, então uma normal por triângulo vira "
                              "uma borda por triângulo aqui.\n\n"
                              "9 = o alcance real do filtro, em células do mapa. VERDE cresce com ele, "
                              "VERMELHO significa ZERO. Com alcance zero as dezesseis amostras colapsam "
                              "numa só, que suaviza DENTRO de uma célula e não faz nada contra a escadinha "
                              "ENTRE células — a grade do mapa chega inteira à tela, e uma escadinha vista "
                              "em diagonal vira uma fileira de dentes. Use quando um controle que deveria "
                              "importar não mudou nada.\n\n"
                              "A névoa é desligada em todas as visões, para a distância não lavar as "
                              "cores.\n\n"
                              "Qualquer valor diferente de zero também preenche a lista abaixo.")
                     .Min(0)
                     .Max(9) // SHADOW_MAP_MAX_DEBUG_VIEW, written out per the note at the top of this panel
                     .DefaultValue(0)
                     .ShowButtons(true)
                     .Format("%d"));
    // SOH [Enhancement] Timing without the debug picture. The slider above always timed the pass as a side
    // effect, but every one of its values also repaints the scene, so the only frames that could be measured
    // were frames that no longer looked like the game -- and the numbers were about those frames. This is the
    // same measurement with nothing drawn differently.
    AddWidget(path, "Medir Custo na GPU (log)", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DEVELOPER_TOOLS("ShadowMap.ProfileGpu"))
        .PreFunc(hideUnlessShadowMap)
        .Options(CheckboxOptions().Tooltip(
            "Mede quanto tempo de GPU o mapa de sombras consome e escreve o resultado no log a cada segundo. "
            "Não altera nada do que é desenhado, então os números valem para o jogo como ele realmente é.\n\n"
            "A linha traz o custo do quadro inteiro, o custo do passe de profundidade dentro dele, em quantos "
            "quadros o passe precisou ser submetido e QUANTAS FATIAS foram redesenhadas por quadro. Essa última "
            "é a que decide o que fazer: com poucas fatias por quadro o passe já está sendo reaproveitado e o "
            "custo restante é geometria; com quase todas, algo está impedindo o reaproveitamento.\n\n"
            "Somente Direct3D 11."));
    AddWidget(path, "Ver o Mapa de Profundidade: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_DEVELOPER_TOOLS("ShadowMap.ViewSlice"))
        .PreFunc(hideUnlessShadowMap)
        .Options(IntSliderOptions()
                     .Tooltip("Desenha uma fatia do mapa de profundidade num canto da tela. 0 = desligado.\n\n"
                              "Todas as Visões de Diagnóstico acima olham o RECEPTOR — o que o pixel sendo "
                              "sombreado recebeu. Esta mostra o que o passo de profundidade GUARDOU, que é a "
                              "outra metade do sistema. Um defeito de sombra pode morar em qualquer uma das "
                              "duas, e até agora só uma podia ser inspecionada.\n\n"
                              "1 em diante são as faixas da camada do MUNDO, em ordem; depois delas vêm as da "
                              "camada de ATORES, que é mais curta.\n\n"
                              "AZUL é célula vazia — nada foi desenhado ali. Isso é diferente de cinza "
                              "escuro, que é algo próximo: os dois são o mesmo número no mapa e significam "
                              "coisas opostas, e confundir um com o outro faz um caster ausente parecer "
                              "presente.\n\n"
                              "A amostragem é por ponto de propósito. Filtrar borraria vizinhos numa imagem "
                              "que a comparação de sombra nunca enxerga.")
                     .Min(0)
                     .Max(5)
                     .DefaultValue(0)
                     .ShowButtons(true)
                     .Format("%d"));
    // Live list of what the world (green) caster layer is actually made of, so a stray green blob in the
    // debug view can be named instead of guessed at. WIDGET_TEXT draws widget.name, and PreFunc runs first,
    // so rewriting the name each frame is what makes it live.
    AddWidget(path, "Cenário projetando (média/frame):", WIDGET_TEXT)
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WorldShadows.Mode"), SHADOW_MODE_VANILLA) !=
                                SHADOW_MODE_SHADOW_MAP ||
                            CVarGetInteger(CVAR_DEVELOPER_TOOLS("ShadowMap.ShowCascadeBounds"), 0) == 0;
            if (!info.isHidden) {
                const char* census = ToonLighting_ShadowMapCasterCensus();
                info.name = std::string("Cenário projetando (média/frame):\n") + (census != nullptr ? census : "");
            }
        });

    // ===========================================================================================
    // Sky — the Wind Waker-style sky replacement: gradient dome + drifting clouds + night stars.
    // ===========================================================================================
    auto hideUnlessSky = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWSky.Enabled"), 0);
    };
    auto hideUnlessSkyGradient = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWSky.Enabled"), 0) ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWSkyGradient.Enabled"), 1);
    };
    auto hideUnlessSkyClouds = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWSky.Enabled"), 0) ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWClouds.Enabled"), 1);
    };
    auto hideUnlessSkyStars = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWSky.Enabled"), 0) ||
                        !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWNightSky.Enabled"), 1);
    };
    path = { "Wind Waker Style", "Sky", SECTION_COLUMN_1 };
    AddSidebarEntry("Wind Waker Style", "Sky", 3);
    AddWidget(path, "Use Sky", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWSky.Enabled"))
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Replaces the overworld sky with a Wind Waker-style one: a gradient sky dome, drifting puffy "
            "clouds with a wispy horizon cloud band, and a twinkling night starfield. Each part can be "
            "toggled and tuned below. Texture packs can swap in different cloud art."));

    AddWidget(path, "Horizon", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessSky);
    AddWidget(path, "Horizon Height", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWClouds.HorizonBandHeight"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSky)
        .Options(FloatSliderOptions()
                     .Tooltip("Raises or lowers the sky's horizon line -- the gradient's haze boundary and "
                              "the horizon cloud band move together. Useful where the visible horizon sits "
                              "below eye level, like the middle of Hyrule Field.")
                     .Format("%.0f")
                     .Min(-2000.0f)
                     .Max(2000.0f)
                     .DefaultValue(-408.0f));
    AddWidget(path, "Horizon Parallax", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWClouds.HorizonBandParallax"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSky)
        .Options(FloatSliderOptions()
                     .Tooltip("How much the sky horizon sinks as the camera climbs. 0% = it follows the "
                              "camera, always at the same height on screen; 100% = it stays at a fixed "
                              "world height, so hilltops rise in front of it and valleys look out over it.")
                     .Min(0.0f)
                     .Max(1.5f)
                     .DefaultValue(0.75f)
                     .IsPercentage());

    AddWidget(path, "Sky Gradient", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessSky);
    AddWidget(path, "Replace Sky Texture", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWSkyGradient.Enabled"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSky)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Replaces the sky texture with a smooth Wind Waker-style gradient, fading from a hazy horizon "
            "up to a deeper sky. The colours shift with the time of day through dawn, dusk and night."));
    AddWidget(path, "Gradient Brightness", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWSkyGradient.Brightness"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSkyGradient)
        .Options(FloatSliderOptions()
                     .Tooltip("Overall brightness of the sky gradient. Raise for a more vivid sky, lower "
                              "for a moodier one.")
                     .Min(0.5f)
                     .Max(1.5f)
                     .DefaultValue(1.0f)
                     .IsPercentage());

    AddWidget(path, "Clouds", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessSky);
    AddWidget(path, "Enable Clouds", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWClouds.Enabled"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSky)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Drifting Wind Waker-style puffy clouds across the sky, plus the wispy cloud band around the "
            "horizon, both riding the wind."));
    AddWidget(path, "Opacity", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWClouds.Opacity"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSkyClouds)
        .Options(FloatSliderOptions()
                     .Tooltip("How opaque the clouds are.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.85f)
                     .IsPercentage());
    AddWidget(path, "Coverage", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWClouds.Coverage"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSkyClouds)
        .Options(FloatSliderOptions()
                     .Tooltip("How much of the sky the clouds fill -- from a few scattered clouds up to "
                              "fully overcast.")
                     .Min(0.0f)
                     .Max(1.0f)
                     .DefaultValue(0.3f)
                     .IsPercentage());
    AddWidget(path, "Drift Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWClouds.DriftSpeed"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSkyClouds)
        .Options(FloatSliderOptions()
                     .Tooltip("How fast the clouds drift across the sky on the wind. 1x is Wind Waker's "
                              "own speed.")
                     .Format("%.1fx")
                     .Min(0.0f)
                     .Max(4.0f)
                     .DefaultValue(1.0f));

    AddWidget(path, "Stars", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessSky);
    AddWidget(path, "Enable Stars", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWNightSky.Enabled"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSky)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "A Wind Waker-style twinkling starfield over the night sky: a fixed bright constellation plus "
            "hundreds of small stars that shimmer, fading in at dusk and out at dawn."));
    AddWidget(path, "Star Count", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWNightSky.StarCount"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSkyStars)
        .Options(IntSliderOptions()
                     .Tooltip("Maximum number of stars at full night (the visible count rises and falls "
                              "with the time of day). Wind Waker uses 1000.")
                     .Min(50)
                     .Max(1000)
                     .DefaultValue(1000)
                     .ShowButtons(true)
                     .Format("%d"));
    AddWidget(path, "Star Brightness", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWNightSky.Brightness"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSkyStars)
        .Options(FloatSliderOptions()
                     .Tooltip("Overall star brightness. Higher = brighter, more prominent stars.")
                     .Min(0.0f)
                     .Max(2.0f)
                     .DefaultValue(1.0f)
                     .IsPercentage());
    AddWidget(path, "Twinkle Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWNightSky.TwinkleSpeed"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSkyStars)
        .Options(FloatSliderOptions()
                     .Tooltip("How fast the stars pulse. 1x is Wind Waker's rate -- about ten seconds per "
                              "cycle.")
                     .Format("%.1fx")
                     .Min(0.1f)
                     .Max(5.0f)
                     .DefaultValue(1.0f));

    AddWidget(path, "Wind Wisps", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessSky);
    AddWidget(path, "Enable Wind Wisps", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWWindWisps.Enabled"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSky)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Wind Waker's white wind streaks curling through the sky -- occasionally pulling a full "
            "loop-de-loop. Their number follows the wind's strength."));
    AddWidget(path, "Wisp Amount", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWWindWisps.Amount"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWSky.Enabled"), 0) ||
                            !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWWindWisps.Enabled"), 1);
        })
        .Options(FloatSliderOptions()
                     .Tooltip("How many wisps ride the wind (their number also rises and falls with the "
                              "wind's strength). 1x is Wind Waker's own count.")
                     .Format("%.1fx")
                     .Min(0.5f)
                     .Max(10.0f)
                     .DefaultValue(1.0f));
    AddWidget(path, "Wisp Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWWindWisps.Speed"))
        .RaceDisable(false)
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWSky.Enabled"), 0) ||
                            !CVarGetInteger(CVAR_ENHANCEMENT("Graphics.WWWindWisps.Enabled"), 1);
        })
        .Options(FloatSliderOptions()
                     .Tooltip("How fast the wisps fly. 1x is Wind Waker's own speed. The whole flight "
                              "path scales together, so the curls and loops keep their shape; slower "
                              "wisps also leave shorter streaks.")
                     .Format("%.2fx")
                     .Min(0.25f)
                     .Max(1.5f)
                     .DefaultValue(1.0f));

    AddWidget(path, "Debug", WIDGET_SEPARATOR_TEXT).PreFunc(hideUnlessSky);
    AddWidget(path, "Split-Screen Compare", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("Graphics.WWSky.SplitDebug"))
        .RaceDisable(false)
        .PreFunc(hideUnlessSky)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Draws the Wind Waker sky only on the left half of the screen, leaving the original sky "
            "visible on the right -- a live side-by-side comparison."));
}

} // namespace SohGui
