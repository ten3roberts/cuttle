#include "graphics/rendertree.h"
#include "defines.h"
#include "mempool.h"
#include "log.h"
#include "graphics/vulkan_members.h"
#include "graphics/renderer.h"
#include <string.h>

static mempool_t* node_pool = NULL;
static VkDescriptorSetLayout entity_data_layout = VK_NULL_HANDLE;
static VkDescriptorSetLayoutBinding entity_data_binding =
	(VkDescriptorSetLayoutBinding){.binding = 0,
								   .descriptorCount = 1,
								   .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
								   .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
								   .pImmutableSamplers = 0};

VkDescriptorSetLayoutBinding* rendertree_get_descriptor_bindings(void)
{
	return &entity_data_binding;
}

uint32_t rendertree_get_descriptor_binding_count(void)
{
	return 1;
}

VkDescriptorSetLayout rendertree_get_descriptor_layout(void)
{
	// Create entity data layout if it doesn't exist
	if (entity_data_layout == VK_NULL_HANDLE)
	{
		descriptorlayout_create(&entity_data_binding, 1, &entity_data_layout);
	}

	return entity_data_layout;
}

RenderTreeNode* rendertree_create(float halfwidth, vec3 center, uint32_t thread_idx)
{
	// Pool allocations for faster deletion and creation without fragmenting
	// memory
	if (node_pool == 0)
	{
		node_pool = mempool_create(sizeof(RenderTreeNode), 64);
	}

	RenderTreeNode* node = mempool_alloc(node_pool);

	node->parent = NULL;
	node->depth = 0;
	node->center = center;
	node->halfwidth = halfwidth;
	node->changed = 0;
	node->depth = 0;

	for (uint8_t i = 0; i < 8; i++)
		node->children[i] = NULL;

	node->entity_count = 0;

	// Create command buffers for eac swapchain image
	for (uint8_t i = 0; i < 3; i++)
	{
		node->commandbuffers[i] = commandbuffer_create_secondary(thread_idx, i, renderPass, framebuffers[i]);
		node->commandbuffers[i].frame = i;
	}

	// Create shader data
	node->entity_data = ub_create(RENDER_TREE_LIM * sizeof(struct EntityData), 0, node->thread_idx);

	descriptorpack_create(rendertree_get_descriptor_layout(), &entity_data_binding, 1, &node->entity_data, NULL, &node->entity_data_descriptors);

	return node;
}

void rendertree_split(RenderTreeNode* node)
{
	float new_width = (node->halfwidth) / 2;

	for (uint32_t i = 0; i < 8; i++)
	{
		vec3 center = vec3_zero;
		if (i & 2)
		{
			center.x = new_width;
		}
		else
		{
			center.x = -new_width;
		}
		if (i & 4)
		{
			center.y = new_width;
		}
		else
		{
			center.y = -new_width;
		}
		if (i & 8)
		{
			center.z = new_width;
		}
		else
		{
			center.z -= -new_width;
		}

		node->children[i] = rendertree_create(new_width, center, node->thread_idx);
		node->children[i]->parent = node;
		node->children[i]->depth = node->depth + 1;
	}
}

void rendertree_update(RenderTreeNode* node, uint32_t frame)
{
	// Map entity info
	void* p_entity_data = ub_map(node->entity_data, 0, node->entity_count * sizeof(struct EntityData), frame);

	for (uint32_t i = 0; i < node->entity_count; i++)
	{
		Entity* entity = node->entities[i];

		// Check if entity still fits
		if (rendertree_fits(node, entity) == false)
		{
			// Remove entity
			memmove(node->entities + i, node->entities + i + 1, (node->entity_count - i - 1) * sizeof *node->entities);
			node->entity_count--;

			// Replace
			rendertree_place(node, entity);
		}
		// Check if entity can fit in any child node
		else
		{
			for (uint32_t i = 0; node->children[0] && i < 8; i++)
			{
				if (rendertree_fits(node->children[i], entity))
				{
					// Remove entity
					memmove(node->entities + i, node->entities + i + 1, (node->entity_count - i - 1) * sizeof *node->entities);
					node->entity_count--;

					// Replace
					rendertree_place(node, entity);
					break;
				}
			}
		}

		// Update entity normally
		entity_update(entity);
		entity_update_shaderdata(entity, p_entity_data, i);
	}

	ub_unmap(node->entity_data, frame);

	// Recurse children
	for (uint32_t i = 0; node->children[0] && i < 8; i++)
	{
		rendertree_update(node->children[i], frame);
	}
}

void rendertree_render(RenderTreeNode* node, CommandBuffer* primary, Camera* camera, uint32_t frame)
{
	// Begin recording
	commandbuffer_begin(&node->commandbuffers[frame]);

	// Map entity info
	for (uint32_t i = 0; i < node->entity_count; i++)
	{
		Entity* entity = node->entities[i];
		entity_render(entity, &node->commandbuffers[frame], i, node->entity_data_descriptors.sets[frame]);
	}

	// End recording
	commandbuffer_end(&node->commandbuffers[frame]);

	// Record into primary

	vkCmdExecuteCommands(primary->buffer, 1, &node->commandbuffers[frame].buffer);

	// For debug mode, show tree

	renderer_draw_cube(node->center, quat_identity, (vec3){node->halfwidth, node->halfwidth, node->halfwidth}, (vec4){0.5f, 0, 0, 1});

	// Recurse children
	for (uint32_t i = 0; node->children[0] && i < 8; i++)
	{
		rendertree_render(node->children[i], primary, camera, frame);
	}
}

bool rendertree_fits(RenderTreeNode* node, Entity* entity)
{
	const SphereCollider* e_bound = entity_get_boundingsphere(entity);

	// Left bound (-x)
	if (e_bound->base.transform->position.x + e_bound->radius < node->center.x - node->halfwidth)
	{
		return false;
	}
	// Right bound (+x)
	if (e_bound->base.transform->position.x + e_bound->radius > node->center.x + node->halfwidth)
	{
		return false;
	}
	// Bottom bound (-y)
	if (e_bound->base.transform->position.y + e_bound->radius < node->center.y - node->halfwidth)
	{
		return false;
	}
	// Top bound (+y)
	if (e_bound->base.transform->position.y + e_bound->radius > node->center.y + node->halfwidth)
	{
		return false;
	}

	// Front bound (z)
	if (e_bound->base.transform->position.y + e_bound->radius < node->center.z - node->halfwidth)
	{
		return false;
	}
	// back bound (-z)
	if (e_bound->base.transform->position.y + e_bound->radius > node->center.z + node->halfwidth)
	{
		return false;
	}

	return true;
}
void rendertree_place(RenderTreeNode* node, Entity* entity)
{
	if (rendertree_fits(node, entity))
	{
		// Check if it could fit in  children

		for (uint32_t i = 0; node->children[0] && i < 8; i++)
		{
			// Fits in a child node as well, go down
			if (rendertree_fits(node->children[i], entity))
			{
				rendertree_place(node, entity);
				return;
			}
		}
		node->entities[node->entity_count++] = entity;

		// Split leaf node if full
		if (node->children[0] == NULL && node->entity_count == RENDER_TREE_LIM)
		{
			rendertree_split(node);
			// rendertree_update(node, frame);
		}
	}

	else
	{
		LOG_E("Entity outside octree bounds");
		// Doesn't fit in current node, go up
		rendertree_place(node->parent, entity);
	}
}
void rendertree_destroy(RenderTreeNode* node)
{
	for (uint32_t i = 0; node->children[0] && i < 8; i++)
	{
		rendertree_destroy(node->children[i]);
	}

	ub_destroy(node->entity_data);
	descriptorpack_destroy(&node->entity_data_descriptors);
	for (uint32_t i = 0; i < 3; i++)
	{
		commandbuffer_destroy(&node->commandbuffers[i]);
	}
	mempool_free(node_pool, node);
	if (mempool_get_count(node_pool) == 0)
	{
		vkDestroyDescriptorSetLayout(device, entity_data_layout, NULL);
		entity_data_layout = VK_NULL_HANDLE;
		mempool_destroy(node_pool);
		node_pool = 0;
	}
}