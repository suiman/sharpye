#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node {
	struct node *par;
	struct node **children;
	int count;
	char c;
	int end;
} node;

typedef struct tree {
	node *root;
} tree;

tree *t_new();
void t_add(tree *t, char *w);
void t_flush(tree *t);
int hit(tree *t, char *w);

node *n_new(node *p, char c) {
	node *n = malloc(sizeof(node));
	n->par = p;
	n->c = c;
	n->count = 0;
	n->children = NULL;
	n->end = 0;
	return n;
}

tree *t_new() {
	node *r = n_new(NULL, '\0');
	tree *t = malloc(sizeof(tree));
	t->root = r;
	return t;
}

void t_add(tree *t, char *w) {
	node *par = t->root;
	int len = strlen(w);
	for(int i = 0; i < len; i++) {
		char c = w[i];
		int found = 0;
		for(int j = 0; (j < par->count) && !found; j++) {
			if(par->children[j]->c == c) {
				par = par->children[j];
				found = 1;
			}
		}
		if(found) continue;
		par->count++;
		par->children = realloc(par->children, sizeof(node *) * par->count);
		node *new_node = n_new(par, c);
		par->children[par->count - 1] = new_node;
		par = new_node;
	}
	par->end = 1;
}

void n_del(node *n) {
	if(n->count > 0) {
		for(int i = 0; i < n->count; i++) {
			n_del(n->children[i]);
		}
		free(n->children);
	}else {
		free(n);
	}
}

void t_flush(tree *t) {
	n_del(t->root);
}

node *get_match_child(node *n, char c) {
	int len = n->count;
	for(int i = 0; i < len; i++) {
		if(n->children[i]->c == c) {
			return n->children[i];
		}
	}
	return NULL;
}

int hit_once(tree *t, char *w) {
	node *n = t->root;
	int len = strlen(w);
	int hit = 0;
	for(int i = 0; i < len; i++) {
		node *child = get_match_child(n, w[i]);
		if(!child) break;
		if(child->end == 1) { hit = 1; break; }
		n = child;
	}
	return hit;
}

int hit(tree *t, char *w) {
	int len = strlen(w);
	int hit = 0;
	while(!hit && len--) {
		hit = hit_once(t, w);
		w++;
	}
	return hit;
}

void t_print(node *n) {
	for(int i = 0; i < n->count; i++) {
		printf("%c->", n->children[i]->c);
		if(n->children[i]->count > 0) {
			t_print(n->children[i]);
		}else {
			putchar('\n');
		}
	}
}

