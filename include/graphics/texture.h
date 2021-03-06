#ifndef TEXTURE_H
#define TEXTURE_H
#include "handle.h"
#include <stdbool.h>

// Textures represent an image on the graphics card
// Note: textures do not contain their own sampler, unlike OpenGL

DEFINE_HANDLE(Sampler);

typedef enum
{
	SAMPLER_FILTER_LINEAR,
	SAMPLER_FILTER_NEAREST
} SamplerFilterMode;

typedef enum
{
	SAMPLER_WRAP_REPEAT,
	SAMPLER_WRAP_REPEAT_MIRROR,
	SAMPLER_WRAP_CLAMP_EDGE,
	SAMPLER_WRAP_CLAMP_BORDER
} SamplerWrapMode;

#include "vulkan/vulkan.h"

// Returns a sampler with the specified options
// If a sampler with options doesn't exist it is created and stored
Sampler sampler_get(SamplerFilterMode filterMode, SamplerWrapMode wrapMode, int maxAnisotropy);

VkSampler sampler_get_vksampler(Sampler sampler);

void sampler_destroy(Sampler sampler);

// Destroys all samplers
void sampler_destroy_all();

DEFINE_HANDLE(Texture);
// Loads a texture from a file
// The textures name is the full file path
Texture texture_load(const char* file);
// Creates a texture from low level arguments
// The contents of the texture is undefined until texture_update is called
// If name is not NULL it shall be a unique name to get the texture by name later
// Format specifies the image pixel format. Most common (VK_FORMAT_R8G8B8A8_SRGB)
// Usage specifies the memory usage flags. Most common (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
// Samples specify if the image is multisampled. VK_SAMPLE_1_BIT for normal images
// Layout specifies the pixel layout of the image. Most common (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
// Only transitions the layout if it is not VK_IMAGE_FORMAT_UNDEFINED
// Image aspect specifies the aspect mask of the image view
Texture texture_create(const char* name, int width, int height, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits samples, VkImageLayout layout, VkImageAspectFlags imageAspect);

// Creates a texture and image view from already existing image
// This can be used when getting image from swapchain
Texture texture_create_existing(const char* name, int width, int height, VkFormat format, VkSampleCountFlagBits samples, VkImageLayout layout, VkImage image, VkImageAspectFlags imageAspect);

// Updates the texture data from host memory
void texture_update(Texture tex, uint8_t* pixeldata);

// Resizes a texture
// This will destroy all data previously in the texture
// Similar to destroy and recreate except it keeps the same handle
// If the texture doesn't own its image, then only the view is recreated, the image is expected to be the correct size or resupplied
void texture_resize(Texture tex, int width, int height);

// Swaps the internal image used in the texture
// Call this before resize if texture doesn't own image
void texture_supply_image(Texture tex, VkImage image);

bool texture_owns_image(Texture tex);

// Attempts to find a texture by name
Texture texture_get(const char* name);

// This will remove the texture if it exists from the internal texture table
// The resource needs to be freed explicitely

void texture_destroy(Texture tex);

// Destroys all loaded textures
void texture_destroy_all();

void* texture_get_image_view(Texture tex);
#endif
