# Replacing the cloud textures (artist guide)

The Wind Waker-style sky draws its clouds from **five images**. The built-in ones are simple
generated stand-ins — they're meant to be replaced by better art. This guide is written for artists:
no programming or game-modding experience needed.

> The preview images below are shown composited over a blue sky so you can see them — the real
> files are white clouds on a **transparent** background.
>
> Source files (the actual PNGs the game is built from):
> [`soh/assets/custom/textures/wind-waker/clouds/`](../soh/assets/custom/textures/wind-waker/clouds/)

## The drifting clouds — `cloudtx_01` / `cloudtx_02` / `cloudtx_03`

| `cloudtx_01.rgba32.png` | `cloudtx_02.rgba32.png` | `cloudtx_03.rgba32.png` |
|:---:|:---:|:---:|
| ![cloudtx_01](images/cloudtx_01-preview.png) | ![cloudtx_02](images/cloudtx_02-preview.png) | ![cloudtx_03](images/cloudtx_03-preview.png) |

64×64 each. These are the puffy clouds that drift across the sky. **Every cloud in the sky is drawn
from all three of these layered on top of each other** (slightly offset), so make them three
different-shaped siblings — not identical copies. In Wind Waker itself these are lumpy hand-drawn
puffs with a soft shadowed underside.

## The horizon cloud ring — `cloud_mae` / `cloud_naka`

`cloud_mae.rgba32.png` — the **front** layer: separate cloud clusters with gaps between them.

![cloud_mae](images/cloud_mae-preview.png)

`cloud_naka.rgba32.png` — the **back** layer: one continuous bank of clouds.

![cloud_naka](images/cloud_naka-preview.png)

256×64 each. These two strips wrap all the way around the horizon, layered over each other, slowly
scrolling and evolving with the wind. The game draws each strip twice (one copy sliding over the
other, their transparencies multiplied) — that's what makes the band slowly morph instead of just
sliding sideways, and it means clean silhouettes work better than fine internal detail.

## Rules

1. **Keep the exact filenames**, including the `.rgba32.png` part.
2. **Transparency is the cloud shape.** The visible part should stay near-white / light grey — the
   game tints the clouds automatically for time of day and weather. If you paint them pink, they
   will be pink at midnight too. A subtle grey-blue shadow on the undersides is fine (the originals
   have one).
3. Sizes must be **powers of two** (64, 128, 256, 512), max 512 in either direction. Bigger =
   sharper in game. The strips can change height too (e.g. 512×128).
4. The two horizon strips must **tile seamlessly left-to-right** — the right edge continues onto the
   left edge. The **bottom edge of the strip sits on the horizon line**.
5. Soft, fuzzy edges look best — hard edges read as cardboard cutouts once they're stretched across
   the sky.

## Getting your clouds into the game

Send the finished PNGs back and they'll be packed into a small mod file (`ww_clouds.o2r`). Drop that
file into your game's `mods` folder — the game then uses your clouds instead of the built-in ones.
No reinstalling; deleting the file brings the originals back.

*(For the person doing the packing: stage the PNGs under
`<staging>/textures/wind-waker/clouds/` and run the ZAPD custom-o2r command from
[wind-waker-clouds-investigation.md](wind-waker-clouds-investigation.md#shipping-without-nintendo-assets--the-texture-override-contract).)*
