# Motion blur

An example how to create a simple motion blur on camera movement. First the object's color and depth are rendered. Then the depth is used to calculate the world position of the pixel so that the view-projection matrix of the previous frame can be applied to create the motion vector texture (i.e. how much the pixel has moved since last frame). In the last step the renderer color texture and the motion vector texture is used to create the motion blur effect by sampling and averaging according to motion vector. This example is based on [GPU Gems 3](https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch27.html).

![motionblur](motionblur.png?raw=true "motionblur")
