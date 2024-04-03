#include "trie.h"
#include "lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#define IP_LENGHT 32

trie_node_t *create_trie_node() {
	
	trie_node_t *node = calloc(1, sizeof(*node));
	return node;
}

void insert(trie_node_t *root, uint32_t prefix, struct route_table_entry *data) {
	trie_node_t *curr = root;

	int len = 0;

	for (int i = 0; i < IP_LENGHT; i++)
		len += ((data->mask >> i) & 1);

	for (int i = 0; i < len; i++) {
		int bit = (prefix >> i) & 1;

		if (curr->child[bit] == NULL)
			curr->child[bit] = create_trie_node();

		curr = curr->child[bit];
	}

	curr->data = data;

	curr->is_leaf = 1;
}