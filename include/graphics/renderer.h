#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include "entity.h"
#include "graphics/shadertypes.h"
#include "scene.h"

// Initializes the renderer
int renderer_init();

// Initializes rendering for the next frame
// Acquires the next image in the swapchain
// Render queue can be rerecorded
// Uniform buffers and other shader resources should now be updated
void renderer_begin();

// Submits a scene for rendering
// Note: scene needs to be explicitely updated
void renderer_submit(Scene* scene);

// This function shall be called after a resize event
// Does not explicitely resize the window, but recreates the swapchain and pipelines
// Will not immidiately resize until resize events stop
// This is to avoid resizing every frame when user drag-resizes window
void renderer_hint_resize();

// Will frag to the renderer that the command buffers need to be rebuilt
// The command buffers will be redrawn on submit
// Later versions will use secondary cmd buffer to allow for partial reconstruction
void renderer_flag_rebuild();

// Retrieves the index of the current image to render to
uint32_t renderer_get_frameindex();

// Returns the main framebuffer outputting to the swapchain
Framebuffer* renderer_get_framebuffers();

// @Functions to draw shape for one frame@
void renderer_draw_custom(Mesh* mesh, vec3 position, quaternion rotation, vec3 scale, vec4 color);
void renderer_draw_cube(vec3 position, quaternion rotation, vec3 scale, vec4 color);
void renderer_draw_cube_wire(vec3 position, quaternion rotation, vec3 scale, vec4 color);

// Deinitializes the renderer and frees all resources
void renderer_terminate();
#endif
