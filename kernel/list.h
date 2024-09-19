#ifndef INC_LIST_H
#define INC_LIST_H

#include "types.h"

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
static inline void
list_init(struct list_head *head)
{
    head->next = head->prev = head;
}

// リストは空か?
static inline int
list_empty(struct list_head *head)
{
    return head->next == head;
}

// リストの先頭のエントリを返す
static inline struct list_head *
list_front(struct list_head *head)
{
    return head->next;
}

// リストの最後のエントリを返す
static inline struct list_head *
list_back(struct list_head *head)
{
    return head->prev;
}

// prev -> cur -> next : curをprevとnextの間に挿入
static inline void
list_insert(struct list_head *cur, struct list_head *prev, struct list_head *next)
{
    next->prev = cur;
    cur->next = next;
    cur->prev = prev;
    prev->next = cur;
}

// cur -> head : curをheadのheadの前に挿入
static inline void
list_push_front(struct list_head *head, struct list_head *cur)
{
    list_insert(cur, head, head->next);
}

// head <- cur : curをheadの後ろに挿入
static inline void
list_push_back(struct list_head *head, struct list_head *cur)
{
    list_insert(cur, head->prev, head);
}

// prev -> next : prevとnextの間のエントリを削除
static inline void
list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

// prev -> (item) -> next : item自身をリストから削除
static inline void
list_drop(struct list_head *item)
{
    list_del(item->prev, item->next);
}

// リストの先頭のエントリを削除
static inline void
list_pop_front(struct list_head *head)
{
    list_drop(list_front(head));
}

// リストの最後のエントリを削除
static inline void
list_pop_back(struct list_head *head)
{
    list_drop(list_back(head));
}

// リストからitemを探す
static inline struct list_head *
list_find(struct list_head *head, struct list_head *item)
{
    for (struct list_head *p = head->next; p != head; p = p->next) {
        if (p == item)
            return item;
    }
    return 0;
}

// memberを要素に持つheadリストの要素をposに入れてループ
#define LIST_FOREACH_ENTRY(pos, head, member)                           \
    for (pos = container_of(list_front(head), typeof(*pos), member);    \
        &pos->member != (head);                                         \
        pos = container_of(pos->member.next, typeof(*pos), member))

#define LIST_FOREACH_ENTRY_REVERSE(pos, head, member)                   \
    for (pos = container_of(list_back(head), typeof(*pos), member);     \
        &pos->member != (head);                                         \
        pos = container_of(pos->member.prev, typeof(*pos), member))

/* Iterate over a list safe against removal of list entry. */
#define LIST_FOREACH_ENTRY_SAFE(pos, n, head, member)                   \
    for(pos = container_of(list_front(head), typeof(*pos), member),     \
        n = container_of(pos->member.next, typeof(*n), member);         \
        &pos->member != (head);                                         \
        pos = n, n = container_of(n->member.next, typeof(*n), member))

#endif
