#ifndef TOON_LIGHTING_H
#define TOON_LIGHTING_H

// C-callable surface of the Wind Waker-style cel/shadow module (implementation in ToonLighting.cpp).
// Kept minimal: the decompiled game code (C) only needs the shadow-receiver predicate, the cached
// feature switches (so the per-actor draw path never does string-keyed CVar lookups), and the lens
// bracket helpers for the one draw pass that runs outside the main actor loop.

#ifdef __cplusplus
extern "C" {
#endif

struct Actor;
struct GraphicsContext;

// True for the small curated set of actors that are really walkable floors (the castle-town drawbridge,
// the Gerudo Valley bridge, certain dungeon platforms). These are drawn before the actor-shadow volumes
// flush so shadows land on them like the static scene, and they never cast a shadow of their own.
int ToonLighting_IsShadowReceiver(struct Actor* actor);

// Shadow system selector (CVar Graphics.WorldShadows.Mode). The three modes are mutually exclusive:
// exactly one shadow system draws at a time. Values are persisted in the config, so only append.
typedef enum {
    SHADOW_MODE_VANILLA = 0,   // the original game's blob/feet/circle shadows, untouched
    SHADOW_MODE_ACTOR = 1,     // Wind Waker-style stencil-volume silhouettes (FlushToonShadow)
    SHADOW_MODE_SHADOW_MAP = 2 // cascaded depth-map shadows (D3D11 only; falls back to vanilla elsewhere)
} ShadowMode;

// Cached once-per-frame feature switches (refreshed from the CVars at the top of each game frame).
// The draw code asks these instead of reading CVars per actor: a CVarGet* is a string-keyed hash
// lookup, far too expensive to repeat for every drawn actor every frame.
int ToonLighting_FeaturesActive(void);         // cel relight OR a non-vanilla shadow mode (gates the draw hook)
int ToonLighting_CelEnabled(void);             // cel relight on (gates the toon bracket)
int ToonLighting_ShadowsEnabled(void);         // stencil-volume (Actor Shadows) mode on, specifically --
                                               // callers that mean "any system that needs casters marked"
                                               // must also test ToonLighting_ShadowMapEnabled()
int ToonLighting_ShadowMapEnabled(void);       // shadow-map mode on (gates the depth-pass capture)
// Radius around the camera within which actors are drawn purely so they can cast (0 = off). Read per
// actor by the draw-culling test, so it comes from the frame snapshot rather than a CVar lookup.
float ToonLighting_ShadowMapCasterDrawRadius(void);
int ToonLighting_ShadowMode(void);             // raw ShadowMode value
int ToonLighting_SuppressVanillaShadows(void); // a non-vanilla mode is on AND set to hide the vanilla shadows

// Wrap Actor_DrawLensActors with these: lens actors draw through Actor_Draw after the main toon
// bracket closed, so the bracket must re-open around them (and the shadow capture must be disarmed
// after the last one) to keep the module's stream tracking in sync.
void ToonLighting_LensBracketBegin(struct GraphicsContext* gfxCtx);
void ToonLighting_LensBracketEnd(struct GraphicsContext* gfxCtx);

#ifdef __cplusplus
}
#endif

#endif // TOON_LIGHTING_H
