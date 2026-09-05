/*
	E-comOS Kernel - Doubly-linked List
	Copyright (C) 2025,2026  Saladin5101
*/

#include <kernel/internal/list.h>

void list_init(list_head* head) {
	head->first = 0;
	head->last = 0;
	head->count = 0;
}

void list_add(list_head* head, list_node* node) {
	node->next = 0;
	node->prev = head->last;
	if (head->last)
		head->last->next = node;
	else
		head->first = node;
	head->last = node;
	head->count++;
}

void list_remove(list_head* head, list_node* node) {
	if (node->prev)
		node->prev->next = node->next;
	else
		head->first = node->next;
	if (node->next)
		node->next->prev = node->prev;
	else
		head->last = node->prev;
	node->next = 0;
	node->prev = 0;
	head->count--;
}
