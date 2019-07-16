# Glow

An example how to use render targets and later use them as a texture. In this example the model is first rendered with a solid color (green), then it's blurred (fullscreen pass) and finally the textured model is rendered on top of it so it looks like the model has an outline glow. The solid color rendering is done with 25% size compared to the final render target size.

![glow](glow.png?raw=true "glow")
