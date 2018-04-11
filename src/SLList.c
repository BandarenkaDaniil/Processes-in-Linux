#include "SLList.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sll_init(struct SLList* source_list)
{
    source_list->head        = (struct Node*)malloc(sizeof(struct Node*));
    source_list->head->value = NULL;
    source_list->head->next  = NULL;

    source_list->size = 0;
}

void sll_terminate(struct SLList* source_list)
{
    while (source_list->head->next != NULL)
    {
    	struct Node *temp = source_list->head->next;

    	free(source_list->head);
    	source_list->head = temp;
    }
}

void sll_push(struct SLList* source_list, char* new_string)
{
    struct Node *new_node = (struct Node*)malloc(sizeof(struct Node*));

    new_node->value = (char*)malloc(sizeof(char*) * strlen(new_string));
    strcpy(new_node->value, new_string);
    new_node->next = NULL;

    struct Node *temp = source_list->head;

    while (temp->next != NULL)
    {
        temp = temp->next;
    }
    temp->next = new_node;

    source_list->size++;
}

char* sll_pop(struct SLList* source_list)
{
    struct Node *temp = source_list->head;

    while (temp->next->next != NULL)
    {
        temp = temp->next;
    }

    free(temp->next->value);
    free(temp->next);
    temp->next = NULL;

    source_list->size--;

    return "a";
}

bool sll_is_empty(struct SLList* source_list)
{
    return (source_list->size == 0);
}

int sll_size(struct SLList* source_list)
{
  return source_list->size;
}
