#ifndef INC_LIST_H
#define INC_LIST_H

#include <common/types.h>
#include <printf.h>

#define offsetof(st, m) __builtin_offsetof(st, m)

#define container_of(ptr, type, member)                 \
({                                                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);  \
    (type *)((char *)__mptr - offsetof(type,member));   \
})

// このエントリの構造体を返す
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

struct list_head {
    struct list_head *next, *prev;
};

// リストを初期化
static inline void list_init(struct list_head *list)
{
    list->next = list->prev = list;
}

// リストは空か?
static inline int list_empty(struct list_head *list)
{
    return list->next == list;
}

// リストの先頭のエントリを返す
static inline struct list_head * list_front(struct list_head *list)
{
    return list->next;
}

// リストの最後のエントリを返す
static inline struct list_head * list_back(struct list_head *list)
{
    return list->prev;
}

// prev -> item -> next : itemをprevとnextの間に挿入
static inline void list_insert(struct list_head *item, struct list_head *prev, struct list_head *next)
{
    next->prev = item;
    item->next = next;
    item->prev = prev;
    prev->next = item;
}

// item -> list : itemをlistの先頭に挿入
static inline void list_push_front(struct list_head *list, struct list_head *item)
{
    list_insert(item, list, list->next);
}

// head <- item : itemをheadの後ろに挿入
static inline void list_push_back(struct list_head *list, struct list_head *item)
{
    list_insert(item, list->prev, list);
    trace("[list: %p, next: %p, prev: %p], [item: %p, next: %p, prev->%p]", list, list->next, list->prev, item, item->next, item->prev);
}

static inline void list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

// prev -> (item) -> next : item自身をリストから外す
static inline void list_drop(struct list_head *item)
{
    trace("[item: %p, next: %p, prev: %p]", item, item->next, item->prev);
    list_del(item->prev, item->next);
}

// リストの先頭のエントリをリストから外す
static inline void list_pop_front(struct list_head *list)
{
    //struct list_head *item = list_front(list);
    trace("[list: %p, next: %p, prev: %p], [item: %p, next: %p, prev: %p]", list, list->next, list->prev, item, item->next, item->prev);
    list_drop(list_front(list));
}

// リストの最後のエントリをリストから外す
static inline void list_pop_back(struct list_head *list)
{
    list_drop(list_back(list));
}

// リストを結合する
static inline void list_concat(struct list_head *list, struct list_head *other)
{
    if (list_empty(other)) {
        return;
    }

    other->next->prev = list;
    other->prev->next = list->next;
    list->next->prev = other->prev;
    list->next = other->next;

    list_init(other);
}

// リストからitemを探す
static inline struct list_head *
list_find(struct list_head *list, struct list_head *item)
{
    for (struct list_head *p = list->next; p != list; p = p->next) {
        if (p == item)
            return item;
    }
    return 0;
}

// リストの長さを返す
static inline int list_length(const struct list_head *list) {
    struct list_head *item;
    int count = 0;

    item = list->next;
    while (item != list) {
        item = item->next;
        count++;
    }
    return count;
}

// memberを要素に持つheadリストの要素をposに入れてループ
#define list_foreach(item, head, member)                           \
    for (item = container_of(list_front(head), typeof(*item), member);    \
        &item->member != (head);                                         \
        item = container_of(item->member.next, typeof(*item), member))

#define list_foreach_reverse(item, head, member)                   \
    for (item = container_of(list_back(head), typeof(*item), member);     \
        &item->member != (head);                                         \
        item = container_of(item->member.prev, typeof(*item), member))

/* Iterate over a list safe against removal of list entry. */
#define list_foreach_safe(item, temp, head, member)                   \
    for(item = container_of(list_front(head), typeof(*item), member),     \
        temp = container_of(item->member.next, typeof(*temp), member);         \
        &item->member != (head);                                         \
        item = temp, temp = container_of(item->member.next, typeof(*temp), member))

#endif
