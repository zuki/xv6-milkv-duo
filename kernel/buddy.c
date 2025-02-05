/*
Copyright 2014 Akira Midorikawa

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
/**
 * @file buddy.c
*/

#include "defs.h"
#include "common/types.h"
#include "page.h"
#include "printf.h"
#include "spinlock.h"

extern char _end[]; // kernel.ldで定義sれているカーネル後の最初のアドレス

struct spinlock buddy_lock;

/** @ingroup buddy
 * @struct free_list
 * @brief 空きリスト構造体
*/
struct free_list {
    struct page *head;
};

/** @ingroup buddy
 * @var free_lists
 * @brief 空きリストの構造体: ページオーダ毎に1要素
*/
static struct free_list free_lists[PAGE_MAX_DEPTH];

/**
 * @ingroup buddy_static
 * @brief 相棒のページインデックスを取得する.
 * @param page ページへの構造体
 * @return pageの相棒のページのページインデックス
 */
static page_index buddy_find_buddy_index(struct page *page) {
    /* ページがペアの後半 */
    if ((page->index >> page->order) & 0x1) {
        return (page->index - (1 << page->order));
    /* ページがペアの前半 */
    } else {
        return (page->index + (1 << page->order));
    }
}

/**
 * @ingroup buddy_static
 * @brief フリーリストの先頭ブロックを返す.
 *
 * @param list フリーリストへのポインタ
 * @return ブロックの先頭ページの構造体へのポインタ。
 *         listがからの場合は NULL
 */
static struct page *buddy_list_pop(struct free_list *list) {
    // debug("list: %08p, head: %08p", list, list->head);
    struct page *page = list->head;

    if (page) {
        list->head = page->next;
        page->next = NULL;
        page->flags &= ~PF_FREE_LIST;
    }

    return page;
}

/**
 * @ingroup buddy_static
 * @brief フリーリストの先頭にブロックを追加する.
 *
 * @param list フリーリストへのポインタ
 * @param page 追加するブロックの先頭ページへのポインタ
 */
static void buddy_list_push(struct free_list *list, struct page *page) {
    if (list->head) {
        page->next = list->head;
    } else {
        page->next = NULL;
    }
    /* フリーフラグを立てて、リストの先頭に追加する */
    list->head = page;
    page->flags |= PF_FREE_LIST;
}

/**
 * @ingroup buddy_static
 * @brief フリーリストからページブロックを削除する.
 *
 * @param list ページブロックが所属するフリーリストへのポインタ
 * @param index 削除するページブロックの先頭ページのインデックス
 * @return 削除したページブロックの先頭ページの構造体へのポインタ。該当するページブロックがなかった場合は NULL
 */
static struct page *buddy_list_remove(struct free_list *list, page_index index) {
    struct page **pp = &list->head, *page = list->head;

    while (page) {
        /* 該当のページを発見 */
        if (page->index == index) {
            /* listからpageを削除 */
            *pp = page->next;
            /* ページの後処理 */
            page->next = NULL;
            page->flags &= ~PF_FREE_LIST;
            /* ページを返す */
            return page;
        }
        /* 次の要素をチェック */
        pp = &page->next;
        page = page->next;
    }

    return NULL;
}

/**
 * @ingroup buddy_static
 * @brief 相棒がフリーかをチェックする.
 *
 * @param page 相棒をチェックするページへのポインタ
 * @param buddy pageの相棒へのポインタ
 * @return 相棒がフリーなら 1; フリーでなければ 0
 */
static int buddy_is_free_buddy(const struct page *page, const struct page *buddy) {
    return ((buddy->flags & PF_FREE_LIST) && buddy->order == page->order);
}

/**
 * @ingroup buddy
 * @brief バディシステムを初期化する.
 *          最大ブロック (4MB = 2^10*4KB) で初期化する
 */
void buddy_init(void) {
    struct page *page;
    int i, len, max_page_block = (1 << PAGE_MAX_ORDER); // 1024 = 2^10

    /* FIXME: PAGE_STARTを固定値で与えているので、カーネル機能が増えるにつれて
     * _endがPAGE_STARTを超える可能性がある */
    if (_end > PAGE_START) {
        error("_end: 0x%x, start: 0x%x", _end, PAGE_START);
        panic("fix PAGE_START");
    }

    /* 1. buddyシステムのロックを初期化 */
    initlock(&buddy_lock, "buddy");

    /* 2. 配列 free_lists[PAGE_MAX_DEPTH] を0クリアする
     *        free_listはブロックごとに1つずつある */
    memset(free_lists, 0, sizeof(struct free_list) * PAGE_MAX_DEPTH);

    /* 3. pagesをPAGE_STARTの直前に作成して、0クリアする
     *     pagesはPAGE_NUM個ある4KBのページのページ構造体のリスト
     *     pagesはpage.cで(struct page *)として定義されている */
    pages = (struct page*)(PAGE_START - (sizeof(struct page) * PAGE_NUM));
    memset(pages, 0, sizeof(struct page) * PAGE_NUM);
    trace("pages: 0x%08x, size: 0x%08x, PAGE_START: 0x%08x", pages, sizeof(struct page) * PAGE_NUM, PAGE_START);
    trace("sizeof(struct page): 0x%04x\n", sizeof(struct page));

    // 3. page->indexの設定
    for (i = 0; i < PAGE_NUM; ++i) {
        pages[i].index = i;
    }

    /* 4. pagesを max_page_blockの大きさ (2^10 * 4KB = 4MB) ブロックに分け、
     *    ブロックの最初のページの構造体の next, order, flagsを設定する。
     *    len = 14 = 0x3800 / 0x400
     */
    for (i = 0, len = PAGE_NUM / max_page_block; i < len; ++i) {
        /* pageはブロック(4MB)の先頭page */
        page = &pages[i * max_page_block];

        /* page->nextの設定 : 次のブロックの先頭; 最後のブロックは NULL */
        if (i < (len - 1)) {
            page->next = &pages[(i + 1) * max_page_block];
        }
        /* 最初はすべて最大order(10)のブロック */
        page->order = PAGE_MAX_ORDER;
        /* ブロックは未使用 */
        page->flags |= PF_FREE_LIST;
        trace("pages[%d]: address: %08p, order: %d, flags: 0x%08x", page->index, page, page->order, page->flags);
    }

    /* 5. 最大ブロックのフリーリストの先頭をセットする */
    free_lists[PAGE_MAX_ORDER].head = &pages[0];
    trace("\nfree_lists[10].head: %08p\n", free_lists[10].head);
}

/**
 * @ingroup buddy_static
 * @brief 指定のオーダーのページブロックを取得する.
 *        指定のオーダーより大きな空きブロックのあるオーダーを
 *        見つけ、fromオーダーまで再帰的に分割してfromオーダーの
 *        ブロックを返す。使用するのは常に分割した後半部分で
 *        前半部分は空きリストにつなぐ。
 *
 * @param from 必要なデータブロックのオーダー
 * @return データブロックの先頭ページのページ構造体へのポインタ。
 *         全て使用済みの場合はpanic
 */
static struct page* buddy_pull_block(int from) {
    int i, to = 0;
    struct free_list *list;
    struct page *page;
    /* 1. 指定のオーダより大きな空きのあるブロックを探す */
    for (i = from + 1; i < PAGE_MAX_DEPTH; ++i) {
        list = &free_lists[i];
        page = buddy_list_pop(list);

        if (page) {
            /* 空きブロックを2つに分割するので返すブロックは-1*/
            to = i - 1;
            break;
        }
    }

    /* 2. すべてのオーダーに空きブロックがなければエラー */
    if (i == PAGE_MAX_DEPTH) {
        panic("out of memory");
    }
    // debug("from: %d, to: %d", from, to);
    /* 3. 得られた空きブロックから必要なブロックに分割する */
    for (i = to; i >= from; --i) {
        list = &free_lists[i];
        /* 3.1 ブロックを分割して前半はフリーリストにつなぐ */
        page->order = i;
        list->head = page;
        page->flags |= PF_FREE_LIST;
        // debug("list[%d]: %08p, head: %016p", i, list, list->head);
        // debug("free_lists[%d]: %08p, head: %08p", i, &free_lists[i], free_lists[i].head);
        // debug("page: %08p, p->order: %d", page, page->order);
        /* 3.2 ブロックの後半を使って、さらに分割できるかチェックする  */
        page += (1 << i);
    }
    /* 4. 必要なオーダのブロックまで分割できたのでオーダをセットする */
    page->order = from;
    /* 5. fromオーダーのブロックを返す */
    trace("\nreturn page: %08p, index: %d, order: %d, flags: 0x%08x, next: %08p\n",
        page, page->index, page->flags, page->order, page->next);
    return page;
}

/**
 * @ingroup buddy
 * @brief 指定されたサイズのページブロックを割り当てる.
 * ページが含まれるブロックの先頭ページを返す.
 *
 * @param size 必要なサイズ
 * @return 必要なサイズを満たすページブロックの先頭ページ構造体へのポインタ
 */
struct page *buddy_alloc(size_t size) {
    int i;
    struct free_list *list;
    struct page *page = NULL;

    /* 1. lockを取得 */
    acquire(&buddy_lock);

    /* 2. オーダー0からsizeを満たすブロックを探す*/
    for (i = 0; i < PAGE_MAX_DEPTH; ++i) {
        list = &free_lists[i];
        // debug("free_lists[%d]: %016p, head: %08p", i, &free_lists[i], free_lists[i].head);
        /* 2.1 サイズ以上のブロックが見つかったら空きブロックがあるかチェックする*/
        if (((1UL << i) * PAGE_SIZE) >= size) {
            // debug("size %d match order %i list: %16p", size, i, list);
            page = buddy_list_pop(list);
            /* 2.2 このオーダーに空きがなかったら大きなオーダー
             *  から分割してこのオーダーの空きブロックを作って返す */
            if (!page) {
                page = buddy_pull_block(i);
            }

            break;
        }
    }
    /* 3. lockを解放 */
    release(&buddy_lock);

    /* 4. すべてのオーダーで空きがなかったらpanic */
    if (i == PAGE_MAX_DEPTH) {
        error("requested page is too large: size=0x%x", size);
        panic("No memory");
    }
    /* 5. 前方ブロックフラグをたてる。相棒にはこのフラグがない */
    page->flags |= PF_FIRST_PAGE;   // 使用済みフラグ

    trace("size: %d, page: %08p, index: %d, flags: 0x%08x, order: %d, addr: %08p",
       size, page, page->index, page->flags, page->order, page_address(page));
    return page;
}

/**
 * @ingroup buddy
 * @brief ページブロックを解放し、相棒があればブロックをまとめる.
 *          pageはページブロックの先頭
 *
 * @param page ページ構造体へのポインタ
 */
void buddy_free(struct page *page) {
    struct free_list *list;
    struct page *p, *bp;
    page_index bi;

    /* 1. lockを取得 */
    acquire(&buddy_lock);
    /* 2. ページが所属するフリーリストを取得する */
    list = &free_lists[page->order];
    /* 3. ページの相棒のインデックスを取得する */
    bi = buddy_find_buddy_index(page);
    /* 4. 相棒のページを取得する */
    bp = &pages[bi];

    /* 5. 前方ブロックフラグを外す */
    page->flags &= ~PF_FIRST_PAGE;

    /* 6. 解放するページブロックの処置 */
    /* 6.1 解放するページはまとめられない : フリーリストに追加して終了 */
    if (page->order == PAGE_MAX_ORDER || !buddy_is_free_buddy(page, bp)) {
        buddy_list_push(list, page);
    /* 6.2 まとめられる : 再帰的にブロックをまとめる */
    } else {
        /* 6.2.1 このブロックのフィールドを編集する */
        page->flags &= ~PF_FREE_LIST;
        page->next = NULL;
        /* 6.2.2 相棒をリストから削除する */
        p = buddy_list_remove(list, bi);
        if (!p) {
            panic("broken page");
        }
        /* 6.2.3 相棒が2分割の後半 : pageに相棒を併合*/
        if ((bi >> page->order) & 0x1) {
            p->order = 0;   // 相棒のorderフィールドをクリア
        /* 6.2.4 相棒が2分割の前方 : 相棒にpageを併合 */
        } else {
            page->order = 0;    // pageのorderフィールドをクリア
            page = p;           // 対象ブロックの先頭を相棒とする
        }
        /* 6.2.5 合併したブロックのオーダーを修正する */
        page->order++;
        /* 6.2.6 まとめられなくなるまで再帰実行 */
        release(&buddy_lock);
        return buddy_free(page);
    }
    /* 7. lockを解放 */
    release(&buddy_lock);
}
