#ifndef TRIE_H
#define TRIE_H

#include <stdint.h>

typedef struct trie_node {
	struct trie_node *child[2];
	struct route_table_entry *data;
	int is_leaf;

} trie_node_t;

trie_node_t *create_trie_node();

void insert(trie_node_t *root, uint32_t prefix, struct route_table_entry *data);

#endif /* TRIE_H */