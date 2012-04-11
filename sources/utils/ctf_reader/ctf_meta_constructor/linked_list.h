/*
 * One-linked list, which optimized for next operations:
 * 
 * 0. Embedding in other structures.
 * 1. Adding element to the end of list.
 * 2. Removing all elements from the first one.
 *         NOTE: order of removing is differ from order of adding.
 * 3. Iteration of all elements from the first one.
 */

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdio.h> /* NULL constant */

/* container_of() macro should be defined before */

struct linked_list_elem
{
    struct linked_list_elem* next;
};

static inline void linked_list_elem_init(struct linked_list_elem* elem)
{
    elem->next = NULL;
}

struct linked_list_head
{
    struct linked_list_elem *first;
    struct linked_list_elem **last_p;
};

static inline void linked_list_head_init(struct linked_list_head* head)
{
    head->first = NULL;
    head->last_p = &head->first;
}

/* Add element to the end of the list. */
static inline void linked_list_add_elem(struct linked_list_head* head,
    struct linked_list_elem* elem)
{
    *head->last_p = elem;
    head->last_p = &elem->next;
}

/* Iterate over the list(do not remove elements!) */
#define linked_list_for_each_entry(head, entry, member)                 \
    for(entry = container_of((head)->first, typeof(*entry), member);    \
        &entry->member != NULL;                                         \
        entry = container_of(entry->member.next, typeof(*entry), member))

/* Return non-zero if list is not empty */
static inline int linked_list_is_empty(struct linked_list_head* head)
{
    return head->first == NULL;
}

/* 
 * Remove first entry in the list. After call 'entry' will contain
 * entry deleted.
 * 
 * List shouldn't be empty.
 */
#define linked_list_remove_first_entry(head, entry, member) do {    \
    entry = container_of((head)->first, typeof(*entry), member);      \
    (head)->first = (head)->first->next;                                \
    if((head)->first == NULL) (head)->last_p = &(head)->first;} while(0)

/* 
 * Remove given element from the list. O(n).
 * 
 * Return non-zero if element was deleted and 0 if list doesn't contain
 * this element.
 * 
 */
static inline int linked_list_remove(struct linked_list_head* head,
    struct linked_list_elem* elem)
{
    struct linked_list_elem** elem_tmp_p;
    for(elem_tmp_p = &head->first;
        *elem_tmp_p != NULL;
        elem_tmp_p = &(*elem_tmp_p)->next)
    {
        if(*elem_tmp_p == elem)
        {
            *elem_tmp_p = elem->next;
            if(head->last_p == &elem->next)
                head->last_p = elem_tmp_p;
            return 1;
        }
    }
    return 0;
}

#endif /* LINKED_LIST_H */