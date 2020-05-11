// This file should only be used by the renderer
#ifndef RENDER_PRUNE_H
#define RENDER_PRUNE_H
#include "graphics/uniforms.h"
#include "graphics/shadertypes.h"
#include "graphics/commandbuffer.h"
#include "entity.h"
#include "defines.h"

// A node of the render pruning octree
// Handled by scene on update
typedef struct RenderTreeNode
{
	// Tree data
	// The bounds for the tree
	float left, right, bottom, top;
	uint8_t depth;

	struct RenderTreeNode* children[8];
	struct RenderTreeNode* parent;

	// Entity data
	uint32_t entity_count;
	Entity* entities[RENDER_TREE_LIM];
	// Contains all RENDER_TREE_LIM entities data
	CommandBuffer commandbuffers[3];
	// A bit field of which frames should be rebuilt
	uint8_t changed;
	uint8_t thread_idx;
} RenderTreeNode;

// Creates a rendertree root node for a thread
// Note, only the render thread with the correct index should use this
// All children inherit the thread index
RenderTreeNode* rendertree_create(float halfwidth, vec3 origin, uint32_t thread_idx);

void rendertree_destroy(RenderTreeNode* node);

// Updates the tree recursively from node(root)
// Re-places entities
// Queues swapchain recreation
void rendertree_update(RenderTreeNode* node);

// Splits the node into 8 children
// If the node is root, the children are assigned separate threads
void rendertree_split(RenderTreeNode* node);
// Joins all children
void rendertree_join(RenderTreeNode* node);

// Returns true if an enitty is fully contained given node
// Entity cannot exist in any node when performing calling this function
bool rendertree_fits(RenderTreeNode* node, Entity* entity);

// Places an entity into the tree
// Check if it fits in the start node
// If not it goes up until it fits
// It then goes down to the child it fits in recursively
// Inserts into the smallest node that fully contains the entity
// May split the tree
void rendertree_place(RenderTreeNode* node, Entity* entity);

#endif