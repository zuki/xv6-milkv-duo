#include <linux/time.h>
#include <riscv.h>
#include <printf.h>
#include <proc.h>
#include <list.h>
#include <spinlock.h>


struct spinlock timerlock;
static struct timer_list timer_list;
static uint64_t timer_jiffies = 0;

void timer_init(void)
{
    initlock(&timerlock, "timer lock");
    list_init(&timer_list.list);
}

static inline void internal_add_timer(struct timer_list *timer)
{
    //割り込み禁止（trap.c）&& timerlockを保持

    struct list_head *head, *curr;
    struct timer_list *entry;
    bool unset = true;

    head = &timer_list.list;
    curr = head->next;

    while (curr != head) {
        entry = list_entry(curr, struct timer_list, list);
        if (timer->expires <= entry->expires) {
            if (curr->prev == head) {           // entryは先頭
                entry->expires -= timer->expires;
                list_push_front(&timer_list.list, &timer->list);
            } else if (curr->next == head) {    // entryは末尾
                timer->expires -= entry->expires;
                list_push_back(&timer_list.list, &timer->list);
            } else {                            // entryの前に挿入
                entry->expires -= timer->expires;
                list_insert(&timer->list, entry->list.prev, &entry->list);
            }
            unset = false;
            break;
        }
        timer->expires -= entry->expires;
        curr = curr->next;
    }
    if (unset)
        list_push_back(&timer_list.list, &timer->list);
}

#if 0
static void update_proc_time(int user_mode)
{
    struct proc *p = thisproc();
    if (user_mode)
        p->utime++;
    else
        p->stime++;
}
#endif

static inline int detach_timer(struct timer_list *timer)
{
    if (!timer_pending(timer))  // 未登録
        return 0;
    list_drop(&timer->list);    // リストから自分を外す
    return 1;
}

void add_timer(struct timer_list *timer)
{
    acquire(&timerlock);
    if (timer_pending(timer))   // 二重登録
        goto bug;
    internal_add_timer(timer);
    release(&timerlock);
    return;
bug:
    release(&timerlock);
    warn("bug: kernel timer added twice at %p",
            __builtin_return_address(0));
}

int del_timer(struct timer_list * timer)
{
    int ret;

    acquire(&timerlock);
    ret = detach_timer(timer);                      // 1. リストから削除
    timer->list.next = timer->list.prev = NULL;     // 2. 内部リストをクリア
    release(&timerlock);
    return ret;
}

void run_timer_list(void)
{
    struct list_head *head, *curr;
    uint64_t delta = jiffies - timer_jiffies;

    acquire(&timerlock);
    head = &timer_list.list;
    curr = head->next;

    while (curr != head) {
        struct timer_list *timer;
        void (*fn)(uint64_t);
        uint64_t data;              // データには(void *)が設定可能

        timer = list_entry(curr, struct timer_list, list);
        if (timer->expires <= delta) {
            fn = timer->fn;
            data = timer->data;
            detach_timer(timer);
            timer->list.next = timer->list.prev = NULL;
            release(&timerlock);
            fn(data);
            acquire(&timerlock);
        } else {
            timer->expires -= delta;    // リストの先頭のexpiresを経過時刻だけ減ずる
            break;
        }
        curr = curr->next;
    }
    timer_jiffies += delta;
    release(&timerlock);
}
