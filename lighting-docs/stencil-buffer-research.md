The people in Hyrule have a lot free time, which some fill with catching fireflies. While running behind a shiny firefly, you may notice, that they seem to have dynamic light sources which could be kind of a performance hit.

![Screenshot from Wind Waker of link standing in a field with small fire flies, each casting a light on to the ground beneath them in the shape of what appears to be a hexagon (or other many sided polygon)](./lightcount01.jpg)

The light tech was one of the most discussed topics, here’s an update. Thanks very much everyone who helped me with the investigation. Here are the results:

Instead of using Deferred lighting, they used a stencil buffer – which is a full resolution texture, masking out (using bright and black pixels), where the light actually needs to be rendered. To create this mask, they render 3 geometry spheres per light – their presence/intersection with the world create the final mask. THe following is speculation – for the 100%-truth you have to ask Nintendo.

![Screenshot from Wind Waker of a torch mounted on a wall. There is a wire frame of an icosphere shown around the torch. The torch casts light on the wall next to it, but only within the area of the icosphere. This makes the casted light appear in the shape of a hexagon on the wall](./icosphere-over-wall-torch.png)

### 1st Sphere
Only the back faces are taken from the 1st sphere. And only its obstructed geometry (behind the torch, wood, wall, ground) is rendered into the mask (which means that even light spheres behind walls are taken into account):

![Two images stitched together showing the same scene from Wind Waker. The first image, labeled "Diffuse" is a torch behind thick jail cell bars with an icosphere around it, the second, labeled "Stencil Buffer" the same scene but in black and gray shapes. It appears that the gray shape represents any geometry that was within the icosphere, everything else is black. The gray includes the ground below the torch and the jail cell bars, even though the bars are in the foreground and their faces are pointing away from the torch.](./lightcount_stencilbuffer_example_01.jpg)

### 2nd Sphere
The 2nd sphere is rendered – this time the front faces are used when they are not obstructed (which means that this time hidden lights are not added to the mask). If you look separately at this draw call, it looks like this.

![Two images stitched together showing the same scene from Wind Waker. The first image, labeled "Diffuse" is a torch behind thick jail cell bars with an icosphere around it, the second, labeled "Stencil Buffer" the same scene but in black and gray shapes. The gray shape appears to highlight the entire area that was within the icophere regardless of the geometry that was within it, as in the air where there is no ground is highlighted. The gray shapes are obscured by the jail cell bars in front of it](./lightcount_stencilbuffer_example_02.jpg)

In the stencil buffer this mask is overlayed above the one which was rendered first. The result looks then like this. The areas where the first and this draw call overlap, get even brighter. This bright area is exactly what we want.

![An image labeled "Stencil Buffer" showing the two previous stencil buffer gray shapes overlayed together forming a brighter gray shape where the two previous overlapped](./buffers-added.png)

### 3rd Sphere
Last but not least, the 3rd sphere is added to the scene. And within its range, the darker values are clamped and only the brighter values stay in the buffer. This means, that hidden lights are clamped to black since they were not brightened up since they didn’t make it into the mask during the 2nd step.

![An image labeled "Stencil Buffer" that shows a gray shape where the brighter gray shape of the previous image was, everything else is black](./lightcount_stencilbuffer_example_04.jpg)

After those 3 spheres, a fullscreen quad is rendered above the whole scene with the color of the light. Of course, you don’t want to have the whole scene lightened up! Therefore you render the light only where the just rendered stencil buffer mask has bright pixels.

![A screenshot from Wind Waker showing the same torch behind the jail cell bars properly lighting the ground beneath it](./lightcount_stencilbuffer_example_05.jpg)

Here’s an example of the whole process. Notice, that some lights “disappear” from the scene, since they are hidden by geometry. Like mentioned above, everything is rendered in Zelda. There’s no distance culling.

    A video was inserted here showing draw calls. It is a zoomed out and shows two different torches in the same jail cell. In the game this full scene has many torches, most of which are not visibl from this position and exist behind the back wall of this jail cell. The video steps through icospheres being added in the Diffuse buffer. Inner geometry, then outer geometry, then clamp. The torches that were behind the wall don't make it to the end because they only show up on the "inner geometry" pass, so their "clamp" pass removes them. In the end, the stencil buffer is left with two gray shapes representing the correct areas to apply lighting in the scene.

