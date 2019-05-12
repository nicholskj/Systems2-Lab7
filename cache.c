#include <stdlib.h>
#include "csapp.h"
#include "cache.h"

//cache items are kept in a doubly linked list
typedef struct cacheList cacheList;
struct cacheList
{
   cacheItem * item;
   cacheList * prev;
   cacheList * next;
};

//pointer to the first node in the list
static cacheList * first;   
//pointer to the last node in the list
static cacheList * last;
//sum of the sizes of all of the items in the list
static int currSize;

//functions that only accessible to functions
//in this file
static void evict(int size);
static void moveToFront(cacheList * ptr);

//cacheInit
//Initialize first, last and currSize
void cacheInit()
{
    first = NULL;
    last = NULL;
    currSize = 0;
    return;
}

//findCacheItem
//Search through the cacheList to try to find
//the item with the provided url.
//If found, move the node containing the cacheItem
//to the front of the list (to implement the LRU policy)
//and return the pointer to the cacheItem.
//If not found, return NULL.
cacheItem * findCacheItem(char * url)
{
    if (first == NULL)
        return NULL;
    cacheList * curr = first;
    if (!strcmp(curr->item->url, url))
        return curr->item;
    while (curr != last) {
        curr = curr->next;
        if (!strcmp(curr->item->url, url)) {
            moveToFront(curr);   
            return curr->item;
        }
    }
    return NULL;
}

//moveToFront
//Move the node pointed to by cacheList to the
//front of the cacheList. This is used to
//implement the LRU policy.  The most recently
//used cache item is in the front of the list.
//The least recently used is at the end.
void moveToFront(cacheList * ptr)
{
    if (ptr == last) {
        last = last->prev;
        last->next = NULL;
    }
    else {
        ptr->prev->next = ptr->next;
        ptr->next->prev = ptr->prev;
    }
    first->prev = ptr;
    ptr->next = first;
    ptr->prev = NULL;
    first = ptr;
    return;
}

//addCacheItem
//This function takes a new item to add to the cache.
//If the adding the item to the cache would cause the
//size of the cache to exceed MAX_CACHE_SIZE, evict is
//called to evict one or more items from the cache to make
//room.
void addCacheItem(char * url, char * content, int size)
{
    int newSize = size + currSize;
    if (newSize > MAX_CACHE_SIZE)
        evict(size);
	
    currSize += size;
    cacheItem * newItem = malloc(sizeof *newItem);
    cacheList * new = malloc(sizeof *new);
	new->next = first;
	if (first==NULL)
		last = new;
    else
        first->prev = new;
    new->prev = NULL;
    first = new;
    newItem->size = size;
    newItem->content = content;
    int i = 0;
    while (i < MAXLINE - 1 && url[i] != '\0') {
        newItem->url[i] =  url[i];
        i++;
    }
    newItem->url[i] = '\0';
    new->item = newItem;
    return;
}

//evict
//This function is called in order to evict
//items from the cache to make room for an item
//with the provided size.
//In order to the implement the LRU policy, nodes
//are deleted from the end of the list until
//the item with the provided size can be added
//so that the currSize is <= MAX_CACHE_SIZE
void evict(int size)
{
    cacheList * end;
    while (currSize + size > MAX_CACHE_SIZE) {
        end = last;
        last = last->prev;
        last->next = NULL;
        currSize -= end->item->size;
        free(end);
    }
    return;
}

//useful for debugging
//Print the list (url and size) in forward and backward direction
//to make sure the prev and next pointers are both correct.
void printCacheList()
{
    cacheList * index = first;
	if(index == NULL)
		return;
    while (index != last) {
        printf("(%s, %d)\n", index->item->url, index->item->size);
        index = index->next;
    }
    printf("(%s, %d)\n", index->item->url, index->item->size);
    while (index != first) {
        printf("(%s, %d)\n", index->item->url, index->item->size);
        index = index->prev;
    }
    printf("(%s, %d)\n", index->item->url, index->item->size);
    return;
}   
