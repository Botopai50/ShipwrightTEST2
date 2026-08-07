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
#include <vector>
#include <string>
#include <cstdio>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
// Not declared by variables.h despite living beside everything that is -- every module that wants it
// declares it, the way ToonLighting.cpp does.
extern PlayState* gPlayState;
// Zora's Domain's hard-coded water box: it is not in the collision header, and it is the one that lets the
// player swim under the waterfall (see PushWaterBoxes).
extern WaterBox zdWaterBox;
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

// The half of the snapshot that reads only settings and the backend, and is therefore safe to run at ANY
// point -- including registration, which happens during InitOTR before the game's own state exists.
//
// Split out from the frame hook after learning the difference the hard way: the hook also advances the clock,
// the clock comes from R_UPDATE_RATE, and that is gGameInfo->data[...] with gGameInfo allocated by Regs_Init,
// which runs long after ShipInit. Calling the whole hook at registration dereferenced a null pointer before
// the title screen. ToonLighting already draws this line -- RefreshFrameParams reads settings,
// OnToonFrameUpdate touches game state -- and it is worth stating why rather than just copying the shape.
static void RefreshWaterParams() {
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

    if (!sParams.enabled) {
        sParams.time = 0.0f; // a re-enable starts the surface from a defined phase rather than mid-scroll
    }

    if (auto interp = GetInterpreter()) {
        // `enabled` already folds in the backend capability, so the interpreter can trust it and does not
        // repeat the check -- the same contract the shadow map uses.
        interp->SetWaterParams(sParams.enabled, sParams.quality, sParams.debugView, sParams.time);
    }
}

// Gather the scene's ACTIVE water boxes and hand them to the renderer.
//
// Deliberately mirrors WaterBox_GetSurfaceImpl (z_bgcheck.c) rather than iterating the collision header
// directly, because three things in there are easy to miss and each one is silent when missed:
//
//   * the room filter. The box's property bits carry a room index, and a box belongs to that room only --
//     unless the index is 0x3F, which means every room. Ignore it and the Water Temple gets water from
//     neighbouring rooms cutting through walls.
//   * Zora's Domain has a water box that is NOT in the collision header at all. It is hard-coded (zdWaterBox)
//     and it is the one that lets the player swim under the waterfall. Iterate only the header and one of
//     the design document's primary test scenes has a body of water the feature cannot see.
//   * a collision header whose water box pointer still equals segment 0 has not finished loading. Reading it
//     as a list gives garbage, and scene transitions are exactly when it happens.
static void PushWaterBoxes() {
    static std::vector<Fast::WaterBoxDesc> boxes;
    boxes.clear();

    auto interp = GetInterpreter();
    if (interp == nullptr) {
        return;
    }
    if (!sParams.enabled || gPlayState == NULL) {
        interp->SetWaterBoxes(nullptr, 0);
        return;
    }

    CollisionHeader* col = gPlayState->colCtx.colHeader;
    const s32 scene = gPlayState->sceneNum;
    const s32 curRoom = gPlayState->roomCtx.curRoom.num;

    auto add = [&](const WaterBox* w, uint32_t index) {
        if (boxes.size() >= (size_t)WATER_MAX_BOXES) {
            return;
        }
        Fast::WaterBoxDesc d;
        d.xMin = (float)w->xMin;
        d.zMin = (float)w->zMin;
        d.xLength = (float)w->xLength;
        d.zLength = (float)w->zLength;
        d.ySurface = (float)w->ySurface;
        // Scene in the high bits, box index in the low ones: stable for the same body of water across
        // frames, which is what a mesh cache (F2) and an appearance profile (F16) will be keyed by.
        d.id = ((uint32_t)scene << 16) | (index & 0xFFFFu);
        boxes.push_back(d);
    };

    if (col != NULL && col->numWaterBoxes > 0 &&
        col->waterBoxes != (WaterBox*)PHYSICAL_TO_VIRTUAL(gSegments[0])) {
        for (s32 i = 0; i < col->numWaterBoxes; i++) {
            const WaterBox* w = &col->waterBoxes[i];
            const u32 room = (w->properties >> 13) & 0x3F;
            if (room != 0x3F && room != (u32)curRoom) {
                continue;
            }
            add(w, (uint32_t)i);
        }
    }

    if (scene == SCENE_ZORAS_DOMAIN) {
        // Index past anything the header can hold, so its stable id cannot collide with a real box's.
        add(&zdWaterBox, 0xFFFFu);
    }

    interp->SetWaterBoxes(boxes.empty() ? nullptr : boxes.data(), (int)boxes.size());
}

static void OnWaterFrameUpdate() {
    // One game frame's worth of time, advanced before the snapshot so the value pushed below is this frame's.
    // R_UPDATE_RATE is how many 60Hz ticks the game advances per draw, so this is the same clock the rest of
    // the world moves on -- pausing the game freezes the water with it, which is the behaviour anything
    // sitting next to a paused actor needs to have.
    //
    // Guarded on gGameInfo because that is what R_UPDATE_RATE reads through, and it is null until Regs_Init.
    // The hook should never fire that early, but a null deref here costs the whole program and the check
    // costs a comparison.
    if (sParams.enabled && gGameInfo != NULL) {
        sParams.time += (float)R_UPDATE_RATE / 60.0f;
    }
    RefreshWaterParams();
    // After the snapshot, so a frame in which the feature was just switched off pushes an empty list rather
    // than leaving the previous scene's boxes live for one more frame.
    PushWaterBoxes();
}

// The identification census as printable text, for the debug section of the menu. Same instrument as the
// shadow map's caster list, and for the same reason: it answers "is it finding this water, and how much of
// it" from the code that decides, instead of from a screenshot.
static std::string sCensusText;

extern "C" const char* WaterRendering_Census(void) {
    if (!sParams.enabled) {
        sCensusText = "(water material inactive)";
        return sCensusText.c_str();
    }
    auto interp = GetInterpreter();
    if (interp == nullptr) {
        sCensusText = "(no renderer)";
        return sCensusText.c_str();
    }
    int tris = 0, boxesHit = 0, boxesTotal = 0, breakdown[5] = {};
    interp->GetWaterCensus(&tris, &boxesHit, &boxesTotal, breakdown);

    char buf[320];
    if (boxesTotal == 0) {
        snprintf(buf, sizeof(buf), "no water boxes in this room");
    } else {
        // The breakdown is the point. "0 of 2 boxes" says the identification failed; these say WHICH
        // condition failed it, which is the difference between a fix and a guess.
        snprintf(buf, sizeof(buf),
                 "%d tris from %d of %d box%s\n"
                 "  in footprint : %d\n"
                 "  wrong height : %d\n"
                 "  not flat     : %d\n"
                 "  taken (xlu)  : %d\n"
                 "  taken (opa)  : %d",
                 tris, boxesHit, boxesTotal, boxesTotal == 1 ? "" : "es", breakdown[0], breakdown[1],
                 breakdown[2], breakdown[3], breakdown[4]);
    }
    sCensusText = buf;
    return sCensusText.c_str();
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
    // The settings half only. This runs during InitOTR, where the game's own state does not exist yet.
    RefreshWaterParams();
}

static RegisterShipInitFunc waterInitFunc(RegisterWaterRendering, { CVAR_ENHANCEMENT("Graphics.Water.Enabled") });
