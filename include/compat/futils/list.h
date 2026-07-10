/******************************************************************************
 * Copyright (c) 2015 Parrot S.A.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Parrot Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file list.h
 *
 * @brief double chained list
 *
 *****************************************************************************/

#ifndef _LIST_H_
#define _LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#if (!defined list_head_init) && (!defined LIST_HEAD_INIT)

#define FUTILS_LIST 1

#define FUTILS_LIST_POISON1 ((void *)0xDEADBEEF)
#define FUTILS_LIST_POISON2 ((void *)0xDEADDEAD)

/**
 * FUTILS_CONTAINER_OF
 * cast a member of a structure out to the containing structure
 *
 * @param ptr the pointer to the member.
 * @param type the type of the container struct this is embedded in.
 * @param member the name of the member within the struct.
 * @return base address of member containing structure
 **/
#define FUTILS_CONTAINER_OF(ptr, type, member)				\
({									\
	const __typeof__(((type *)0)->member) * __mptr = (ptr);		\
	(type *)((uintptr_t)__mptr - offsetof(type, member));	\
})

/**
 * list node
 */
struct list_node {
	struct list_node *next, *prev;
};

static inline void
list_node_unref(struct list_node *node)
{
	node->next = (struct list_node *)FUTILS_LIST_POISON1;
	node->prev = (struct list_node *)FUTILS_LIST_POISON2;
}

static inline int
list_node_is_unref(struct list_node *node)
{
	return (node->next == (struct list_node *)FUTILS_LIST_POISON1) &&
	       (node->prev == (struct list_node *)FUTILS_LIST_POISON2);
}

static inline int
list_node_is_ref(struct list_node *node)
{
	return !list_node_is_unref(node);
}

static inline void
list_init(struct list_node *list)
{
	list->next = list;
	list->prev = list;
}

static inline struct list_node*
list_prev(const struct list_node *list, const struct list_node *item)
{
	return (item->prev != list) ? item->prev : NULL;
}

static inline struct list_node*
list_next(const struct list_node *list, const struct list_node *item)
{
	return (item->next != list) ? item->next : NULL;
}

static inline struct list_node*
list_first(const struct list_node *list)
{
	return list->next;
}
static inline int
list_is_empty(const struct list_node *list)
{
	return list->next == list;
}

static inline struct list_node *
list_last(const struct list_node *list)
{
	return list->prev;
}

static inline void
list_add(struct list_node *novel, struct list_node *prev,
	      struct list_node *next)
{
	novel->next = next;
	novel->prev = prev;
	next->prev = novel;
	prev->next = novel;
}

static inline void
list_add_after(struct list_node *node, struct list_node *novel)
{
	list_add(novel, node, node->next);
}

static inline void
list_add_before(struct list_node *node, struct list_node *novel)
{
	list_add(novel, node->prev, node);
}

static inline void
list_detach(struct list_node *prev, struct list_node *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void
list_del(struct list_node *node)
{
	list_detach(node->prev, node->next);
	list_node_unref(node);
}

static inline void
list_del_init(struct list_node *node)
{
	list_detach(node->prev, node->next);
	list_init(node);
}

static inline void
list_replace(struct list_node *old, struct list_node *novel)
{
	novel->next = old->next;
	novel->next->prev = novel;
	novel->prev = old->prev;
	novel->prev->next = novel;
}

static inline void
list_replace_init(struct list_node *old, struct list_node *novel)
{
	list_replace(old, novel);
	list_init(old);
}

static inline void
list_move_after(struct list_node *to, struct list_node *from)
{
	list_detach(from->prev, from->next);
	list_add_after(to, from);
}

static inline void
list_move_before(struct list_node *to, struct list_node *from)
{
	list_detach(from->prev, from->next);
	list_add_before(to, from);
}

static inline int
list_is_last(const struct list_node *list, const struct list_node *node)
{
	return list->prev == node;
}

#define list_head_init(name) { &(name), &(name) }

#define list_entry(ptr, type, member)\
	FUTILS_CONTAINER_OF(ptr, type, member)

#define list_walk_forward(list, pos)\
	for (pos = (list)->next; pos != (list); pos = pos->next)

#define list_walk_backward(list, pos)\
	for (pos = (list)->prev; pos != (list);	pos = pos->prev)

#define list_walk_forward_safe(list, pos, tmp)	\
	for (pos = (list)->next,			\
			tmp = pos->next; pos != (list);	\
			pos = tmp, tmp = pos->next)

#define list_walk_backward_safe(list, pos, tmp)	\
	for (pos = (list)->prev,			\
			tmp = pos->prev; pos != (list);	\
			pos = tmp, tmp = pos->prev)

#define list_walk_entry_forward(list, pos, member)			\
	for (pos = list_entry((list)->next, __typeof__(*pos), member);	\
		&pos->member != (list);					\
		pos = list_entry(pos->member.next, __typeof__(*pos), member))

#define list_walk_entry_backward(list, pos, member)		\
	for (pos = list_entry((list)->prev, __typeof__(*pos), member);	\
		&pos->member != (list);					\
		pos = list_entry(pos->member.prev, __typeof__(*pos), member))

#define list_walk_entry_forward_safe(list, pos, tmp, member)	\
	for (pos = list_entry((list)->next, __typeof__(*pos), member),	\
			tmp = list_entry(pos->member.next,		\
					__typeof__(*pos), member);	\
		&pos->member != (list);					\
		pos = tmp, tmp = list_entry(tmp->member.next,	\
			__typeof__(*tmp), member))

#define list_walk_entry_backward_safe(list, pos, tmp, member)	\
	for (pos = list_entry((list)->prev, __typeof__(*pos), member),	\
			tmp = list_entry(pos->member.prev,		\
			__typeof__(*pos), member);			\
		&pos->member != (list);					\
		pos = tmp, tmp = list_entry(tmp->member.prev,	\
			__typeof__(*tmp), member))


static inline size_t
list_length(const struct list_node *list)
{
	size_t length = 0;
	const struct list_node *tmp;
	const struct list_node *current;

	list_walk_forward_safe(list, current, tmp) {
		length++;
	}
	return length;
}

/* list-as-fifo helpers */

/* Push one element into the list */
#define list_push(list, elem) list_add_before(list, elem)

/* Pop one element from the list. return NULL if the list is empty */
#define list_pop(list, type, member)                                           \
	({                                                                     \
		__typeof__(((type *)0)->member) * __pop_ptr = list_first(list);\
		__pop_ptr == list ? NULL : ({                                  \
			list_del(__pop_ptr);                                   \
			FUTILS_CONTAINER_OF(__pop_ptr, type, member);          \
		});                                                            \
	})

#endif /* (!defined list_head_init) && (!defined LIST_HEAD_INIT) */

#ifdef __cplusplus
}
#endif

#endif /* _LIST_H_ */
