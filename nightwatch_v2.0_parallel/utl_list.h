#ifndef LIST_H_
#define LIST_H_

#include <unistd.h>
#include <stddef.h>

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

static inline void
list_init(struct list_head *list_head) 
{
	list_head->next = list_head;
	list_head->prev = list_head;
}

static inline int
list_need_init(struct list_head *list_head)
{
	return (list_head->next == NULL || list_head->prev == NULL);
}

static inline int
list_empty(struct list_head *list_head) 
{
	return (list_head->next == list_head);
}

static inline void
list_add(struct list_head *entry, struct list_head *head)
{
	entry->next = head->next;
	entry->prev = head;
	entry->prev->next = entry;
	entry->next->prev = entry;
}

static inline void
list_del(struct list_head *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->prev = entry;
	entry->next = entry;
}

#define list_entry(ptr, type, member) \
((type*)((char*)(ptr) - offsetof(type, member)))

#define prev_entry(ptr, type, member) \
((type*)((char*)((ptr)->prev) - offsetof(type, member)))

#define next_entry(ptr, type, member) \
((type*)((char*)((ptr)->next) - offsetof(type, member)))

#define list_for_each_entry(pos, head, member) \
for (pos = list_entry((head)->next, typeof(*pos), member); \
&pos->member != head; \
pos = list_entry(pos->member.next, typeof(*pos), member))


#endif	/* LIST_H_ */
