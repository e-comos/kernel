/*
	E-com_os Kernel - Doubly-linked List
	Copyright (C) 2025,2026  Saladin5101
*/

#ifndef KERNEL_INTERNAL_LIST_H
#define KERNEL_INTERNAL_LIST_H

#include <stddef.h>

typedef struct list_node {
	struct list_node* next;
	struct list_node* prev;
} list_node;

typedef struct {
	list_node* first;
	list_node* last;
	size_t count;
} list_head;

void list_init(list_head* head);
void list_add(list_head* head, list_node* node);
void list_remove(list_head* head, list_node* node);

#endif
