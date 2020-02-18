#include "xmlparser.h"
#include "log.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "strmap.h"

struct XMLNode
{
	struct XMLNode* parent;
	char* tag;
	char* content;
	uint32_t child_count;
	strmap* children;
};

XMLNode* xml_loadfile(const char* filepath)
{
	FILE* file = fopen(filepath, "r");
	if (file == NULL)
	{
		LOG_E("Failed to open file %s", filepath);
		return NULL;
	}
	char* buf = NULL;
	size_t size;
	fseek(file, 0L, SEEK_END);
	size = ftell(file);
	buf = malloc(size);
	fseek(file, 0L, SEEK_SET);
	size_t i = 0;
	// Loads the xml file into memory, taking escaping into account
	fread(buf, 1, size, file);

	// Loads the root node
	XMLNode* root = malloc(sizeof(XMLNode));
	root->parent = NULL;
	char* body = buf;
	body = xml_load(root, buf);
	free(buf);
	fclose(file);
	return root;
}

char* xml_load(XMLNode* node, char* str)
{
	// Step pointer so that the tag opening is at pos 0
	do {
		str = strchr(str, '<');

		if (str == NULL)
			return NULL;
		if (str[1] == '?')
		{
			str++;
		}
	} while (*str == '?');
	if (str[1] == '/') return NULL;
	// Close tag is the distance to the > after the <
	// Indicates the end of the opening tag
	size_t tag_end = strchr(str, '>') - str;

	// Copy the tag into node
	node->tag = malloc(tag_end);
	memcpy(node->tag, str + 1, tag_end - 1);
	node->tag[tag_end - 1] = '\0';

	// Empty node
	if (node->tag[tag_end - 2] == '/')
	{
		node->content = NULL;
		node->children = NULL;
		return str + tag_end + 1;
	}
	// Move str to the start of the body
	str += tag_end + 1;
	char* tmp_tag = malloc(tag_end + 3);
	tmp_tag[0] = '<';
	tmp_tag[1] = '/';
	memcpy(tmp_tag + 2, node->tag, tag_end);
	tmp_tag[tag_end + 1] = '>';
	tmp_tag[tag_end + 2] = '\0';

	// Find the closing match of the tag
	char* tmp = strstr(str, tmp_tag);
	free(tmp_tag);
	// No closing part was found
	if (tmp == NULL)
	{
		free(node->tag);
		return NULL;

	}
	size_t tag_close = tmp - str;

	node->content = malloc(tag_close + 1);
	memcpy(node->content, str, tag_close + 1);
	node->content[tag_close] = '\0';

	// Load children
	node->children = strmap_create();
	while (1)
	{
		XMLNode* new_node = malloc(sizeof(XMLNode));
		new_node->parent = node;
		char* buf = xml_load(new_node, str);
		if (buf == NULL)
		{
			free(new_node);
			break;
		}
		str = buf;
		strmap_insert(node->children, new_node->tag, &new_node, sizeof(XMLNode*));
	}

	str += tag_close + 1 + tag_end + 1;
	return str;
}

void xml_destroy(XMLNode* node)
{
	if (node->children)
	{
		uint32_t size = strmap_count(node->children);
		// Destroy the children
		for (uint32_t i = 0; i < size; i++)
		{
			xml_destroy(*(XMLNode**)strmap_index(node->children, i)->data);
		}

		// Free the children
		strmap_destroy(node->children);
	}
	free(node->tag);
	free(node->content);
	free(node);
}