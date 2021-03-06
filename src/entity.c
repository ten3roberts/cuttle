#include "entity.h"
#include "magpie.h"
#include "graphics/material.h"
#include "log.h"
#include "mempool.h"
#include "scene.h"
#include "graphics/model.h"
#include "graphics/rendertree.h"
#include <stdio.h>

struct Entity
{
	char name[256];
	Transform transform;
	Rigidbody rigidbody;
	Material material;
	vec4 color;
	Mesh* mesh;
	SphereCollider boundingsphere;
};

// Pool entity creation to allow for faster allocations and reduce memory fragmentation
static mempool_t entity_pool = MEMPOOL_INIT(sizeof(Entity), 128);

Entity* entity_create(const char* name, const char* material_name, const char* mesh_name, Transform transform, Rigidbody rigidbody)
{
	Entity* entity = mempool_alloc(&entity_pool);
	snprintf(entity->name, sizeof entity->name, "%s", name);

	entity->transform = transform;
	entity->rigidbody = rigidbody;
	entity->material = material_get(material_name);

	if (!HANDLE_VALID(entity->material))
	{
		LOG_W("Unknown material %s. Using default material", material_name);
		entity->material = material_get_default();
	}

	entity->mesh = mesh_find(mesh_name);
	if (entity->mesh == NULL)
	{
		LOG_E("Unknown mesh %s", mesh_name);
	}

	// Create bounding sphere from model and bind the transform to it
	entity->boundingsphere = spherecollider_create(mesh_max_distance(entity->mesh), vec3_zero, &entity->transform);

	entity->color = vec4_white;

	// Add to scene
	scene_add_entity(scene_get_current(), entity);
	return entity;
}

const char* entity_get_name(Entity* entity)
{
	return entity->name;
}
Transform* entity_get_transform(Entity* entity)
{
	return &entity->transform;
}
Rigidbody* entity_get_rigidbody(Entity* entity)
{
	return &entity->rigidbody;
}
Material entity_get_material(Entity* entity)
{
	return entity->material;
}
Mesh* entity_get_mesh(Entity* entity)
{
	return entity->mesh;
}

const SphereCollider* entity_get_boundingsphere(Entity* entity)
{
	return &entity->boundingsphere;
}

vec4 entity_get_color(Entity* entity)
{
	return entity->color;
}

void entity_set_color(Entity* entity, vec4 color)
{
	entity->color = color;
}

void entity_update(Entity* entity)
{
	rigidbody_update(&entity->rigidbody, &entity->transform);
	transform_update(&entity->transform);
}

void entity_update_shaderdata(Entity* entity, void* data_write, uint32_t index)
{
	struct EntityData data = {0};
	data.model_matrix = entity->transform.model_matrix;
	data.color = entity->color;

	memcpy((struct EntityData*)data_write + index, &data, sizeof(struct EntityData));
}

// Is only called irreguraly when command buffers are rebuilt
void entity_render(Entity* entity, Commandbuffer commandbuffer, uint32_t index, VkDescriptorSet data_descriptors)
{
	// Binding is done by renderer
	material_bind(entity->material, commandbuffer, data_descriptors);
	mesh_bind(entity->mesh, commandbuffer);

	// Set push constant for model matrix
	material_push_constants(entity->material, commandbuffer, 0, &index);
	mesh_draw(entity->mesh, commandbuffer);
}

void entity_destroy(Entity* entity)
{
	mempool_free(&entity_pool, entity);
	//scene_remove_entity(scene_get_current(), entity);
}