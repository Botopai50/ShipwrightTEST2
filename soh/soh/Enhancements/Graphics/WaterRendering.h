#ifndef WATER_RENDERING_H
#define WATER_RENDERING_H

// SOH [Enhancement] Breath of the Wild-style water.
//
// The game-side half of the feature. The renderer half lives in libultraship (fast/water.h and the Direct3D
// 11 backend); this decides whether the feature is on, gathers the things only the game knows -- where the
// water bodies are, what the sun is doing, what the fog is set to -- and emits the display-list markers that
// tell the renderer when to act.
//
// Split exactly like the cel shading and the shadow map before it: policy here, mechanism there. The renderer
// never reaches into the game's config, and the game never has to know what a render target is.

#ifdef __cplusplus
extern "C" {
#endif

struct GraphicsContext;

// Whether the new water material is active this frame: the user turned it on AND the running backend can
// render the capture passes. Everything else in this header is meaningless when this is false, and the game
// must keep drawing its original water.
//
// Cached per frame -- this is called from draw paths, so it does no CVar lookups of its own.
int WaterRendering_Enabled(void);

// Emit the scene-capture marker (gSPWaterCapture) into the given display list. Call at the point in the
// stream where the water will be drawn: what the water refracts is whatever was drawn before this marker.
void WaterRendering_EmitCapture(struct GraphicsContext* gfxCtx);

// Debug: how much of the current room's water the identification is actually finding, as a one-line summary
// ("N surface tris from M of K boxes"). Never null. Shown in the menu, so that "does it see this lake" is
// answered by the code that decides rather than inferred from a screenshot -- the lesson the shadow map's
// caster census taught.
const char* WaterRendering_Census(void);

// Whether this actor draws a genuine water SURFACE, and so must be exempt from the veto that covers the
// actor draw loop. Two of them: the Water Temple's rising water and the Shadow Temple's. Everything else an
// actor puts at water height is on the water rather than being it.
struct Actor;
int WaterRendering_ActorDrawsWater(struct Actor* actor);

#ifdef __cplusplus
}
#endif

#endif // WATER_RENDERING_H
