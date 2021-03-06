#include "vulkan/vulkan.h"
#include "graphics/texture.h"
#include <stdbool.h>
#include "handle.h"

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

// Color attachemnt
#define FRAMEBUFFER_COLOR_ATTACHMENT 1
// Depth attachment
#define FRAMEBUFFER_DEPTH_ATTACHMENT 2
// Attachment with 1 sample that resolve samples for presentation
#define FRAMEBUFFER_RESOLVE_ATTACHMENT	 4
#define FRAMEBUFFER_ATTACHMENT_MAX_COUNT 3

struct FramebufferInfo
{
	// If set to true, the framebuffer width and height will match that of the swapchain
	bool swapchain_target;
	// If swapchain_target, indicates which image in the swapchain to target
	uint32_t swapchain_target_index;
	// Width of all attachments in the framebuffer
	uint32_t width;
	// Height of all attachments in the framebuffer
	uint32_t height;

	// The amount of sampler in the framebuffer
	// Note: if framebuffer is swapchain target, the samples need to be resolved to 1
	uint32_t sampler_count;

	// Bit field of the attachments to create for this framebuffer
	uint32_t attachments;
};

DEFINE_HANDLE(Framebuffer);

// Creates a framebuffer with according to the passed info
// Info is copied and stored in the framebuffer which allows recreation
Framebuffer framebuffer_create(const struct FramebufferInfo* info);
void framebuffer_destroy(Framebuffer framebuffer);

Texture framebuffer_get_attachment(Framebuffer framebuffer, uint32_t attachment);

VkFramebuffer framebuffer_vk(Framebuffer framebuffer);

// Resizes the framebuffer and all attachments
// If framebuffer is swapchain target, supplied with and height will be ignored and swapchain extent will be used instead
void framebuffer_resize(Framebuffer framebuffer, int width, int height);

#endif