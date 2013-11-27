#ifndef _LL_H_
#define _LL_H_

#include <stdlib.h>

typedef struct LLNode LLNode;
struct LLNode {
	void *data;
	LLNode *next;
	LLNode *prev;
};

struct LL {
	LLNode *head;
	LLNode *tail;
	int size;
};
typedef struct LL LL;

int LLappend(LL *list, void *data);
int LLinsert(LL *list, void *data, int index);
void *LLremoveIndex(LL *list, int index);
void *LLremove(LL *list, void *data);

#endif
