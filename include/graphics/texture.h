#ifndef TEXTURE_H
#define TEXTURE_H

// Textures represent an image on the graphics card
// Note: textures do not contain their own sampler, unlike OpenGL

typedef struct Texture Texture;
#include "vulkan/vulkan.h"

// Creates a sampler
VkSampler sampler_create(VkFilter filterMode, VkSamplerAddressMode wrapMode, float maxAnisotropy);

void sampler_destroy(VkSampler sampler);

// Returns a basic sampler with linear filtering, tiling, and 16 anisotropic filtering
VkSampler sampler_get_linear();
// Returns a basic sampler with nearest/point filtering, tiling, and 16 anisotropic filtering
VkSampler sampler_get_nearest();

// Loads a texture from a file
// The textures name is the full file path
Texture* texture_load(const char* file);
// Creates a texture from low level arguments
// The contents of the texture is undefined until texture_update is called
// If name is not NULL it shall be a unique name to get the texture by name later
// Format specifies the image pixel format. Most common (VK_FORMAT_R8G8B8A8_SRGB)
// Usage specifies the memory usage flags. Most common (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
// Samples specify if the image is multisampled. VK_SAMPLE_1_BIT for normal images
// Layout specifies the pixel layout of the image. Most common (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
// Image aspect specifies the aspect mask of the image view
Texture* texture_create(const char* name, int width, int height, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits samples, VkImageLayout layout,
						VkImageAspectFlags imageAspect);

// Updates the texture data from host memory
void texture_update(Texture* tex, uint8_t* pixeldata);

// Attempts to find a texture by name
Texture* texture_get(const char* name);

void texture_destroy(Texture* tex);

// Destroys all loaded textures
void texture_destroy_all();

void* texture_get_image_view(Texture* tex);
#endif
