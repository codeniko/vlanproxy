//NOTE - NOT CIRCULAR LINKED LIST

#include "LL.h"

//Add to the end of LL
int LLappend(LL *list, void *data)
{
	LLNode *node = (LLNode *)malloc(sizeof(LLNode));
	if (node == NULL)
		return 0;

	node->data = data;
	node->next = NULL;
	node->prev = NULL;

	//If LL empty
	if (list->size == 0)
	{
		list->head = node;
		list->tail = node;
	} else
	{
		list->tail->next = node;
		node->next = NULL;
		node->prev = list->tail;
		list->tail = node;
	}

	list->size++;
	return 1;
}

int LLinsert(LL *list, void *data, int index)
{
	return 0;
}

//remove an element from LL at an index
void *LLremoveIndex(LL *list, int index)
{
	if (list->size == 0)
		return NULL;
	
	if (index == 0)
	{ //remove head
		LLNode *temp = list->head;
		void *data = temp->data;
		if (list->size == 1) {
			list->head = NULL;
			list->tail = NULL;
		} else
			list->head = list->head->next;
		
		free(temp);
		list->size--;
		return data;
	}
	if (index == list->size - 1)
	{ //remove tail, size has to be > 1
		void *data = list->tail->data;
		list->tail->prev->next = NULL;
		free(list->tail);
		list->size--;
		return data;
	}

	//Execution below is for removal of node inbetween
	LLNode *cur = list->head->next;
	int i;
	for (i = 1; i < list->size-1; i++)
	{
		if (i == index)
		{
			void *data = cur->data;
			cur->prev->next = cur->next;
			cur->next->prev = cur->prev;
			list->size--;
			free(cur);
			return data;
		}
		cur = cur->next;
	}
	return NULL;
}

void *LLremove(LL *list, void *data)
{
	LLNode *cur = list->head;
	for (; cur != NULL; cur = cur->next)
	{
		if (data == cur->data)
		{//data found
			if (cur == list->head && list->size == 1) { //size one, make it empty
				list->head = NULL;
				list->tail = NULL;
			} else if (cur == list->head) { 
				list->head = cur->next;
				cur->next->prev = NULL;
			} else if (cur == list->tail) {
				list->tail = cur->prev;
				cur->prev->next = NULL;
			} else { //between head and tail
				cur->prev->next = cur->next;
				cur->next->prev = cur->prev;
			}

			list->size--;
			return data;
		}
	}

	return NULL;
}
