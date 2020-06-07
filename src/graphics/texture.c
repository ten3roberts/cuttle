#include "graphics/texture.h"
#include "buffer.h"
#include "log.h"
#include "vulkan_members.h"
#include "magpie.h"
#include "stb_image.h"
#include "hashtable.h"

static hashtable_t* sampler_table;

struct SamplerInfo
{
	SamplerFilterMode filterMode;
	SamplerWrapMode wrapMode;
	int maxAnisotropy;
};

struct Sampler
{
	VkSampler vksampler;
	struct SamplerInfo info;
};

static uint32_t hash_sampler(const void* pkey)
{
	struct SamplerInfo* info = (struct SamplerInfo*)pkey;
	uint32_t result = 0;
	result += hashtable_hashfunc_uint32((uint32_t*)&info->filterMode);
	result += hashtable_hashfunc_uint32((uint32_t*)&info->wrapMode);
	result += hashtable_hashfunc_uint32((uint32_t*)&info->maxAnisotropy);

	return result;
}

static int32_t comp_sampler(const void* pkey1, const void* pkey2)
{
	struct SamplerInfo* info1 = (struct SamplerInfo*)pkey1;
	struct SamplerInfo* info2 = (struct SamplerInfo*)pkey2;

	return (info1->filterMode == info2->filterMode && info1->wrapMode == info2->wrapMode && info1->maxAnisotropy == info2->maxAnisotropy) == 0;
}

// Creates a sampler
Sampler* sampler_get(SamplerFilterMode filterMode, SamplerWrapMode wrapMode, int maxAnisotropy)
{
	// Create table if it doesn't exist
	if (sampler_table == NULL)
		sampler_table = hashtable_create(hash_sampler, comp_sampler);

	struct SamplerInfo samplerInfo = {.filterMode = filterMode, .wrapMode = wrapMode, .maxAnisotropy = maxAnisotropy};

	// Attempt to find sampler by info if it already exists
	Sampler* sampler = hashtable_find(sampler_table, (void*)&samplerInfo);

	if (sampler)
		return sampler;

	// Create sampler
	LOG("Creating new sampler");
	sampler = malloc(sizeof(Sampler));
	sampler->info = samplerInfo;

	// Insert into table
	hashtable_insert(sampler_table, &sampler->info, sampler);

	VkSamplerCreateInfo samplerCreateInfo = {0};

	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	// Filtering
	switch (filterMode)
	{
	case SAMPLER_FILTER_LINEAR:
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		break;
	case SAMPLER_FILTER_NEAREST:
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		break;
	default:
		LOG_E("Invalid sampler filter mode %d", filterMode);
	}

	samplerCreateInfo.minFilter = samplerCreateInfo.magFilter;

	// Tiling
	switch (wrapMode)
	{
	case SAMPLER_WRAP_REPEAT:
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		break;
	case SAMPLER_WRAP_REPEAT_MIRROR:
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		break;
	case SAMPLER_WRAP_CLAMP_EDGE:
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		break;
	case SAMPLER_WRAP_CLAMP_BORDER:
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		break;
	default:
		LOG_E("Invalid sampler wrap mode %d", wrapMode);
	}

	samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
	samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;

	// Anisotropic filtering
	samplerCreateInfo.anisotropyEnable = maxAnisotropy > 1 ? VK_TRUE : VK_FALSE;
	samplerCreateInfo.maxAnisotropy = maxAnisotropy;

	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	// Texels are adrees using 0 - 1, not 0 - texWidth
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

	// Can be used for precentage-closer filtering
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	// Mipmapping
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;

	// Samplers are not combined with one specific image
	VkResult result = vkCreateSampler(device, &samplerCreateInfo, NULL, &sampler->vksampler);
	if (result != VK_SUCCESS)
	{
		LOG_E("Failed to create image sampler - code %d", result);
		return NULL;
	}
	return sampler;
}

VkSampler sampler_get_vksampler(Sampler* sampler)
{
	return sampler->vksampler;
}

void sampler_destroy(Sampler* sampler)
{
	hashtable_remove(sampler_table, &sampler->info);

	// Last texture was removed
	if (hashtable_get_count(sampler_table) == 0)
	{
		hashtable_destroy(sampler_table);
		sampler_table = NULL;
	}

	vkDestroySampler(device, sampler->vksampler, NULL);

	free(sampler);
}

void sampler_destroy_all()
{
	Sampler* sampler = NULL;
	while (sampler_table && (sampler = hashtable_pop(sampler_table)))
	{
		sampler_destroy(sampler);
	}
}

typedef struct Texture_raw
{
	Texture handle;
	// Name should not be modified after creation
	char name[256];
	int width;
	int height;
	// The allocated size of the image
	// May be slightly larger than width*height*channels due to alignment requirements
	VkDeviceSize size;
	VkImageLayout layout;
	VkFormat format;
	VkImage vkimage;
	VkDeviceMemory memory;
	VkImageView view;
	// If set to true, the texture owns the vkimage and will free it on destruction
	bool owns_image;
} Texture_raw;

// Contains all textures one after another in memory
// May resize and the pointer may change
// Access by handles
// Slots can be free
static Texture_raw* texture_array = NULL;
// The size of the texture array
static uint32_t texture_array_size = 0;
// The currently count of active textures
static uint32_t texture_array_count = 0;

// Finds memory for a texture
// Initilizes handle member
// Only handle should be returned outside this file
// The returned value should not be stored across function calls
static Texture_raw* texture_alloc()
{
	if (texture_array_count != texture_array_size)
	{
		for (uint32_t i = 0; i < texture_array_size; i++)
		{
			if (texture_array[i].handle.index == HANDLE_INDEX_FREE)
			{
				texture_array[i].handle.index = i;
				// Generation count
				texture_array[i].handle.pattern += 1;
				return texture_array + i;
			}
		}
	}

	// Resize for a new slot
	texture_array_size++;
	texture_array_count++;
	if(texture_array_size >= 2 << MAX_HANDLE_INDEX_BITS)
	{
		LOG_E("Maximum number of textures reached");
		return NULL;
	}
	Texture_raw* tmp = realloc(texture_array, texture_array_size * sizeof(*texture_array));
	if (tmp == NULL)
	{
		LOG_E("Failed to allocate memory for texture");
		return NULL;
	}
	texture_array = tmp;
	// Set the index to the array index
	texture_array[texture_array_size - 1].handle.index = texture_array_size - 1;
	// Start with 0, first generation
	texture_array[texture_array_size - 1].handle.pattern = 0;

	return texture_array + texture_array_size - 1;
}

// Marks the slot pointed to by handle as free
// Frees internals
// Throws an error on double free
static void texture_free(Texture handle)
{
	if (handle.index >= texture_array_size)
	{
		LOG_E("Texture handle %d is not a valid slot index", handle.index);
		return;
	}

	if (texture_array[handle.index].handle.index == HANDLE_INDEX_FREE || *(uint32_t*)&texture_array[handle.index].handle != *(uint32_t*)&handle)
	{
		LOG_E("Texture handle %d has already been freed", handle.index);
		return;
	}
	Texture_raw* raw = texture_array + handle.index;
	raw->handle.index = HANDLE_INDEX_FREE;
	if (raw->owns_image)
	{
		vkDestroyImage(device, raw->vkimage, NULL);
		vkFreeMemory(device, raw->memory, NULL);
	}
	vkDestroyImageView(device, raw->view, NULL);

	texture_array_count--;
	if (texture_array_count == 0)
	{
		free(texture_array);
		texture_array = NULL;
		texture_array_size = 0;
		texture_array_count = 0;
	}
}

static Texture_raw* texture_from_handle(Texture handle)
{
	if (handle.index >= texture_array_size)
	{
		LOG_E("Texture handle %d is not a valid slot index", handle.index);
		return NULL;
	}

	if (texture_array[handle.index].handle.index == HANDLE_INDEX_FREE || *(uint32_t*)&texture_array[handle.index].handle != *(uint32_t*)&handle)
	{
		LOG_E("Texture handle %d is not valid", handle.index);
		return NULL;
	}

	return texture_array + handle.index;
}

// Loads a texture from a file
// The textures name is the full file path
Texture texture_load(const char* file)
{
	LOG_S("Loading texture %s", file);

	uint8_t* pixels;
	int width = 0, height = 0, channels = 0;

	// Solid color requested
	// Create a solid white 256*256 texture
	if (strcmp(file, "col:white") == 0)
	{
		width = 256;
		height = 256;
		pixels = calloc(4 * width * height, sizeof(*pixels));
		memset(pixels, 255, 4 * width * height);
	}
	else
	{
		pixels = stbi_load(file, &width, &height, &channels, STBI_rgb_alpha);
	}

	if (pixels == NULL)
	{
		LOG_E("Failed to load texture %s", file);
		stbi_image_free(pixels);
		return (Texture){.index = HANDLE_INDEX_FREE, .pattern = -1};
	}

	// Save the name
	char name[256];
	snprintf(name, sizeof name, "%s", file);
	Texture tex = texture_create(name, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT,
								 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	texture_update(tex, pixels);

	stbi_image_free(pixels);
	return tex;
}

// Creates a texture with no data
Texture texture_create(const char* name, int width, int height, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits samples, VkImageLayout layout,
					   VkImageAspectFlags imageAspect)
{
	Texture_raw* raw = texture_alloc();

	if (name)
	{
		snprintf(raw->name, sizeof raw->name, "%s", name);
	}
	else
	{
		snprintf(raw->name, sizeof raw->name, "%s", "unnamed");
	}

	raw->width = width;
	raw->height = height;
	raw->layout = layout;
	raw->format = format;
	raw->owns_image = true;

	raw->size = image_create(raw->width, raw->height, format, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &raw->vkimage, &raw->memory, samples);

	// Create image view
	raw->view = image_view_create(raw->vkimage, format, imageAspect);

	// Transition image if format is specified
	if (layout != VK_IMAGE_LAYOUT_UNDEFINED)
		transition_image_layout(raw->vkimage, format, VK_IMAGE_LAYOUT_UNDEFINED, layout);

	return raw->handle;
}

Texture texture_create_existing(const char* name, int width, int height, VkFormat format, VkSampleCountFlagBits samples, VkImageLayout layout, VkImage image, VkImageAspectFlags imageAspect)
{
	Texture_raw* raw = texture_alloc();

	if (name)
	{
		snprintf(raw->name, sizeof raw->name, "%s", name);
	}
	else
	{
		snprintf(raw->name, sizeof raw->name, "%s", "unnamed");
	}

	raw->width = width;
	raw->height = height;
	raw->layout = layout;
	raw->format = format;
	raw->owns_image = false;

	// Get image size
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	raw->size = memRequirements.size;
	raw->vkimage = image;

	// Create image view
	raw->view = image_view_create(raw->vkimage, format, imageAspect);

	return raw->handle;
}

// Updates the texture data from host memory
void texture_update(Texture tex, uint8_t* pixeldata)
{ // Create a staging bufer
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	Texture_raw* raw = texture_from_handle(tex);
	VkDeviceSize image_size = raw->size;

	buffer_create(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer, &staging_buffer_memory,
				  NULL, NULL);

	// Transfer image data to host visible staging buffer
	void* data;
	vkMapMemory(device, staging_buffer_memory, 0, image_size, 0, &data);
	memcpy(data, pixeldata, image_size);
	vkUnmapMemory(device, staging_buffer_memory);

	// Transition image layout
	transition_image_layout(raw->vkimage, raw->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copy_buffer_to_image(staging_buffer, raw->vkimage, raw->width, raw->height);

	// Transition image layout
	transition_image_layout(raw->vkimage, raw->format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, raw->layout);

	// Clean up staging buffer
	vkDestroyBuffer(device, staging_buffer, NULL);
	vkFreeMemory(device, staging_buffer_memory, NULL);
}

Texture texture_get(const char* name)
{
	for (uint32_t i = 0; i < texture_array_size; i++)
	{
		if (strcmp(name, texture_array[i].name) == 0)
		{
			return texture_array[i].handle;
		}
	}

	// Load from file
	return texture_load(name);
}

void texture_destroy(Texture tex)
{
	texture_free(tex);
}

void texture_destroy_all()
{
	for (uint32_t i = 0; i < texture_array_size; i++)
	{
		texture_free(texture_array[i].handle);
	}
}

void* texture_get_image_view(Texture tex)
{
	return texture_from_handle(tex)->view;
}