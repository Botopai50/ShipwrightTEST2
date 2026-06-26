# Wind Waker Style Light Casting

## Observations
In Wind Waker, most point light sources will cast light onto the world geometry around them. Notably not onto other placed objects around them. Those objects have their own cel shading.

The light that is displayed on the environment usually appears as a many sided polygon that is slowly animating its edges.

When two sources of light are close together the area where both lights are projected is brighter than the area lit by only one light source.

The light displayed on the environment matches the color of the light source.

## Stencil Buffer technique
The technical details of how they likely achieved this in the original Game Cube game are described [this document](./stencil-buffer-research.md)

## What we want in SoH
We want to replicate this look the best we can in SoH

The animated polygon edges are likely achieved by rotating the icosphere mentioned in the stencil buffer doc.

We should add new GUI slider params for this. One for a sphere size multiplier that is similar to our "Point light range" slider for cell shading. This however should be its own slider since we will likely want to use a smaller sphere size than we do for the object cel shading. Another slider for sphere rotation speed. Another for lighting intensity.

The icosphere appears to be a "level 2" icosphere with 42 verticles.
