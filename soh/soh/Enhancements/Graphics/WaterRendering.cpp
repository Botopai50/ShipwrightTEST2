// SOH [Enhancement] Breath of the Wild-style water -- game-side policy.
//
// See WaterRendering.h for the split, and libultraship's fast/water.h for what the renderer does with any of
// this. This file is F0 of the staged implementation: it turns the feature on, hands the renderer the frame's
// tuning, and drops the capture marker into the display list so the scene copy and the linearised depth
// actually get taken. Nothing about the water's APPEARANCE is decided yet -- that starts at F1, when the
// original draw is intercepted.

#include <libultraship/bridge.h>
#include <ship/Context.h>
#include <fast/Fast3dWindow.h>
#include <fast/interpreter.h>
#include <fast/water.h>

#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

// Must precede any OPEN_DISPS use, exactly as in ToonLighting.cpp: the interpolation macros have to be the
// ones this translation unit's references resolve against.
#include "soh/frame_interpolation.h"

#include "soh/Enhancements/Graphics/WaterRendering.h"

#include <memory>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
}

// The one-per-frame snapshot. Read by every entry point below, refreshed once at the top of the game frame --
// the same shape ToonLighting uses, for the same reason: these are consulted from draw paths that run many
// times per frame and a CVar lookup in there is a hash of a string.
static struct WaterParams {
    bool enabled = false;   // user switch AND backend capability
    int quality = WATER_DEFAULT_QUALITY;
    int debugView = WATER_DEBUG_OFF;
    // Seconds of GAME time since the feature started running. Advanced here rather than inside the renderer
    // because the frame interpolator re-runs the same display list several times per game frame; a clock
    // ticking on the renderer's side would scroll the water at the interpolated rate while everything beside
    // it moved at the game's.
    float time = 0.0f;
} sParams;

static std::shared_ptr<Fast::Interpreter> GetInterpreter() {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    if (wnd == nullptr) {
        return nullptr;
    }
    return wnd->GetInterpreterWeak().lock();
}

static Fast::GfxRenderingAPI* GetRenderingApi() {
    auto interpreter = GetInterpreter();
    return interpreter == nullptr ? nullptr : interpreter->GetCurrentRenderingAPI();
}

extern "C" int WaterRendering_Enabled(void) {
    return sParams.enabled ? 1 : 0;
}

static void OnWaterFrameUpdate() {
    const bool wanted = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.Water.Enabled"), 0) != 0;

    int quality = CVarGetInteger(CVAR_ENHANCEMENT("Graphics.Water.Quality"), WATER_DEFAULT_QUALITY);
    if (quality < WATER_QUALITY_OFF || quality >= WATER_QUALITY_COUNT) {
        quality = WATER_DEFAULT_QUALITY;
    }
    int debugView = CVarGetInteger(CVAR_DEVELOPER_TOOLS("Water.DebugView"), WATER_DEBUG_OFF);
    if (debugView < WATER_DEBUG_OFF || debugView >= WATER_DEBUG_COUNT) {
        debugView = WATER_DEBUG_OFF;
    }

    // Only worth asking the backend when the mode is actually selected; otherwise leave it false so every
    // per-draw getter short-circuits on the cheap check.
    bool supported = false;
    if (wanted && quality != WATER_QUALITY_OFF) {
        Fast::GfxRenderingAPI* rapi = GetRenderingApi();
        supported = rapi != nullptr && rapi->SupportsWater();
    }

    sParams.enabled = wanted && supported && quality != WATER_QUALITY_OFF;
    sParams.quality = quality;
    sParams.debugView = debugView;

    // One game frame's worth of time. R_UPDATE_RATE is how many 60Hz ticks the game advances per draw, so
    // this is the same clock the rest of the world moves on -- pausing the game freezes the water with it,
    // which is the behaviour anything sitting next to a paused actor needs to have.
    if (sParams.enabled) {
        sParams.time += (float)R_UPDATE_RATE / 60.0f;
    } else {
        sParams.time = 0.0f; // a re-enable starts the surface from a defined phase rather than mid-scroll
    }

    if (auto interp = GetInterpreter()) {
        // `enabled` already folds in the backend capability, so the interpreter can trust it and does not
        // repeat the check -- the same contract the shadow map uses.
        interp->SetWaterParams(sParams.enabled, sParams.quality, sParams.debugView, sParams.time);
    }
}

extern "C" void WaterRendering_EmitCapture(GraphicsContext* gfxCtx) {
    if (!sParams.enabled || gfxCtx == NULL) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    // Into the OPAQUE list. Every command in POLY_OPA runs before the first one in POLY_XLU, so a marker at
    // the end of the opaque stream is guaranteed to see a complete opaque scene -- the terrain, the walls,
    // the lake bed, the actors' solid geometry -- and nothing translucent yet. That is precisely the image
    // the water has to refract and reflect.
    //
    // Which is also why this cannot simply be "once per frame at the top": the capture has to sit at a
    // position in the stream, not at a position in the renderer's frame.
    gSPWaterCapture(POLY_OPA_DISP++);
    CLOSE_DISPS(gfxCtx);
}

void RegisterWaterRendering() {
    // Registered unconditionally, like the toon module: the per-frame snapshot gates all the work, which is
    // what lets a console `set` of the CVar take effect without this having to re-run.
    COND_HOOK(OnGameFrameUpdate, true, OnWaterFrameUpdate);
    OnWaterFrameUpdate();
}

static RegisterShipInitFunc waterInitFunc(RegisterWaterRendering, { CVAR_ENHANCEMENT("Graphics.Water.Enabled") });
