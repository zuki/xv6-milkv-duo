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

/** @file slab.c */

#include <common/types.h>
#include <defs.h>
#include <page.h>
#include <printf.h>
#include <spinlock.h>

/**
 * @ingroup slab
 * @def MAX_SLAB_NAME
 * @brief スラブ名の長さ
 */
#define MAX_SLAB_NAME   32
/**
 * @ingroup slab
 * @def SLAB_FREE_END
 * @brief フリーオブジェクトリストの終了マーク
 */
#define SLAB_FREE_END   (-1U)
/**
 * @ingroup slab
 * @def SLAB_HEADER_SIZE
 * @brief スラブヘッダー長.
 *  + sizeof(uint64_t) はfreeのSLAB_FREE_END用
 */
#define SLAB_HEADER_SIZE (sizeof(struct slab_header) + sizeof(uint32_t))

/* slab lock */
struct spinlock slab_lock;

/**
 * @ingroup slab
 *
 * @struct slab_header
 * @brief スラブヘッダー構造体.
 */
struct slab_header {
    struct slab_header *next;   /**< 次のスラブヘッダーへのポインタ */
    uint32_t *free;             /**< フリーオブジェクト番号リストへのポインタ */
    uint8_t *object;            /**< オブジェクトリストへのポインタ */
};

/**
 * @ingroup slab
 * @struct slab_cache
 * @brief スラブキャッシュ構造体.
 */
struct slab_cache {
    char name[MAX_SLAB_NAME];   /**< キャッシュ名 */

    struct slab_cache *next;    /**< 次のスラブキャッシュへのポインタ */

    uint32_t slab_size;         /**< スラブのサイズ（page * 2^order） */
    uint32_t object_size;       /**< オブジェクトのサイズ（4バイト切り上げ） */
    uint32_t alignment;          /**< アライメント. 不要な場合は0 */

    struct slab_header *slabs_full;     /**< 全使用済みリスト */
    struct slab_header *slabs_partial;  /**< 一部使用済みリスト */
    struct spinlock lock;
};

/**
 * @ingroup slab
 * @var slab_size_limits
 * @brief スラブに収めることができるオブジェクトの最大サイズ.
 *        メモリブロックオーダー毎の配列。
 */
static uint32_t slab_size_limits[PAGE_MAX_DEPTH] = {0};

/**
 * @ingroup slab
 * @var free_cache_head
 * @brief 空きスラブキャッシュのリスト.
 */
struct slab_cache *free_cache_head = NULL;

/**
 * @ingroup slab_static
 * @brief 1スラブに格納できる最大オブジェクト数を取得する.
 *   + sizeof(uint32_t) はobject番号
 *   sizeof(struct slab_header) + sizeof(uint32_t) * N + object_size * N + sizeof(uint32_t) + alignment <= slab_size
 *   N <= (slab_size - (sizeof(struct slab_header) + sizeof(uint32_t)) - alignment) / (sizeof(uint32_t) + object_size)
 *     <= (slab_size - SLAB_HEADER_SIZE - alignment) / (object_size  + sizeof(uint32_t) )
 *   (0x4000 - 0x1c - 0x20) / (0x400 + 4) = 0xf
 *   0x1c + 0x3c + 0x3c00 + 4 + 0x20 = 0x3c7c
 * @param スラブキャッシュへのポインタ
 * @return 最大オブジェクト数
 */
static uint32_t slab_max_object_num(const struct slab_cache *cache) {
    return (cache->slab_size - SLAB_HEADER_SIZE - cache->alignment) / (cache->object_size + sizeof(uint32_t));
}

/**
 * @ingroup slab_static
 * @brief スラブを新規作成する.
 *
 * @param スラブキャッシュへのポインタ
 * @return 作成されたスラブのヘッダーへのポインタ
 */
static struct slab_header *slab_new(const struct slab_cache *cache) {
    struct slab_header *header;
    uint32_t i, max_object_num = slab_max_object_num(cache);
    trace("cache '%s', max_obj_num: %d", cache->name, max_object_num);

    /* 1. 新規スラブを割り当て、ヘッダーを0クリアする */
    header = (struct slab_header*)page_address(buddy_alloc(cache->slab_size));
    memset(header, 0, sizeof(struct slab_header));

    /* 2. free, objectメンバーのアドレスをセットする */
    header->free = (void *)(header + 1);     /* ヘッダーの直後 */
    /* free objリストの直後, header->freeは4バイト、+ 1はSLAB_FREE_END用 */
    header->object = (void *)(header->free + max_object_num + 1);
    if (cache->alignment)
        header->object = (void *)(((uint64_t)header->object + (cache->alignment - 1)) & ~(cache->alignment - 1));
    trace("new header: %p, free: %p, object: %p", header, header->free, header->object);
    trace("header->free: ");
    /* 3. free objectリストに次のobject番号をセットする: 0->1->2->...->14->SLAB_FREE_END */
    for (i = 0; i < max_object_num; ++i) {
        header->free[i] = i + 1;
        trace("  [%d]: %p = 0x%x", i, &header->free[i], header->free[i]);
    }
    /* 3.1 最後に終了マークをセットする */
    header->free[max_object_num] = SLAB_FREE_END;
    trace("  [%d]: %p = 0x%x", max_object_num, &header->free[max_object_num], header->free[max_object_num]);

    //debug_bytes("header->free:", (char *)header->free, 64);

    return header;
}

/**
 * @ingroup slab_static
 * @brief 新規スラブキャッシュを取得する。
 *      リストfree_cache_headの先頭のスラブキャッシュを切り取って返す。
 *      free_cache_headが空の場合は新規作成してリストに追加し、先頭を返す
 *
 * @return 作成されたスラブキャッシュへのポインタ
 */
static struct slab_cache *slab_cache_new(void) {
    uint32_t i;
    struct slab_cache *cache;

    /* 1. free_cache_headリストが空の場合は割り当てる */
    if (!free_cache_head) {
        /* 1.1 新規リスト用のメモリ（1ページ）を割り当てる */
        cache = memset(page_address(buddy_alloc(PAGE_SIZE)), 0, PAGE_SIZE);
        trace("cache: %p", cache);
        /* 1.2. スラブキャッシュ構造体リストとして初期化する（nextフィールドのセット） */
        for (i = 0; i < (PAGE_SIZE / sizeof(struct slab_cache)) - 1; ++i) {
            cache[i].next = &cache[i+1];
            trace("cache[%d]: %p, next: %p", i, &cache[i], cache[i].next);
        }
        /* 1.3 作成したリストをfree_cache_headにつなげる */
        free_cache_head = cache;
        trace("free_cache_head: %p", free_cache_head);
    }
    /* 2. free_cache_headの先頭のスラブキャッシュ構造体を返す */
    cache = free_cache_head;
    /* 3. free_cache_headを更新する */
    free_cache_head = cache->next;
    /* 4. 返すスラブキャッシュをリストから切り離す */
    cache->next = NULL;

    /* 5. slab_cache lockを初期化する */
    initlock(&cache->lock, "slab_cache");

    trace("cache: %p, free_cache_head: %p", cache, free_cache_head);

    return cache;
}

/**
 * @ingroup slab_static
 * @brief スラブキャッシュを削除する.
 *
 * @param 削除するスラブキャッシュへのポインタ
 */
static void slab_cache_delete(struct slab_cache *cache) {
    /* キャッシュを0クリアする */
    memset(cache, 0, sizeof(struct slab_cache));
    /* free_cache_headの先頭につなげる */
    cache->next = free_cache_head;
    free_cache_head = cache;
}

/**
 * @ingroup slab
 * @brief スラブキャッシュシステムを初期化する.
 */
void slab_cache_init(void) {
    int i, slab_size;

    trace("sizeof(slab_header): 0x%x, SLAB_HEADER_SIZE: 0x%x", sizeof(struct slab_header), SLAB_HEADER_SIZE);
    trace("sizeof(slab_cache): 0x%x", sizeof(struct slab_cache));

    /* 1. メモリブロックの種別ごとに1ブロックに収まる最大のオブジェクト長をセットする */
    for (i = 0; i < PAGE_MAX_DEPTH; ++i) {
        /* 4KB * 1, 2, 4, 8, ..., 1024 */
        slab_size = PAGE_SIZE * (1UL << i);
        /* 各スラブに無駄なくオブジェクトを詰めるための最大オブジェクト長
         * （FIXEME: 8オブジェクトが最適?） */
        // slab_size >= sizeof(struct slab_header) + sizeof(uint32_t) * (N + 1) + obj_size + N
        //           >= SLAB_HEADER_SIZE + sizeof(uint32_t) * N + obj_size * N
        // obj_size <= (slab_size - SLAB_HEADER_SIZE - sizeof(uint32_t) * N) / N
        //          <= (slab_size / N) - (SLAB_HADER_SIZE / N) - sizeof(uint32_t)   (+1は切り上げ?, 9はアライメントを考慮)
        slab_size_limits[i] = (slab_size / 9) - ((SLAB_HEADER_SIZE / 8) + 1) - sizeof(uint32_t);
        trace("i: %d, page_size: 0x%x, limits: %d", i, slab_size, slab_size_limits[i]);
    }

    initlock(&slab_lock, "slab");

    free_cache_head = NULL;
}

/**
 * @ingroup slab
 * @brief スラブキャッシュを新規作成する.
 *
 * @param name 作成するスラブキャッシュの名前
 * @param size 作成するスラブキャッシュのオブジェクトサイズ
 * @return 作成されたスラブキャッシュへのポインタ。エラーの場合は NULL
 */
struct slab_cache *slab_cache_create(const char *name, uint32_t size, uint32_t alignment) {
    int i;
    struct slab_cache *cache;

    /* 1. 引数をチェックする */
    if (!size) {
        return NULL;
    }
    if (strlen(name) >= MAX_SLAB_NAME) {
        return NULL;
    }

    /* 2. slab lockをacquire */
    acquire(&slab_lock);
    /* 3. 新規スラブキャッシュを割り当てる */
    cache = slab_cache_new();
    /* 4. slab lock をrelease*/
    release(&slab_lock);

    /* 5. 名前とオブジェクトサイズを設定する */
    safestrcpy(cache->name, name, strlen(name));
    uint32_t align = alignment ? alignment : 4;         // 最低4バイトアライメント
    cache->object_size = (size + align - 1) & ~(align - 1);
    cache->alignment = alignment;
    /* 6. スラブサイズを決定する */
    for (i = 0; i < PAGE_MAX_DEPTH; ++i) {
        if (cache->object_size <= slab_size_limits[i]) {
            cache->slab_size = PAGE_SIZE * (1 << i);
            break;
        }
    }
    /* 7. オブジェクトサイズが大きすぎる場合はエラー */
    if (!cache->slab_size) {
        return NULL;
    }
    trace("created '%s': object: %d, slab: 0x%x, depth: %d, align: %d",
        cache->name, cache->object_size, cache->slab_size, i, cache->alignment);
    return cache;
}

/**
 * @ingroup slab
 * @brief スラブキャッシュを削除する.
 *
 * @param cache 削除するスラブキャッシュへのポインタ
 */
void slab_cache_destroy(struct slab_cache *cache) {
    struct slab_header *header, *next_header;

    /* 1. lockをかける */
    acquire(&slab_lock);

    /* 2. このキャッシュの完全使用済みスラブを解放する */
    header = cache->slabs_full;
    while (header) {
        next_header = header->next;
        buddy_free(page_find_by_address(header));
        header = next_header;
    }
    /* 2, このキャッシュの一部使用済みスラブを解放する */
    header = cache->slabs_partial;
    while (header) {
        next_header = header->next;
        buddy_free(page_find_by_address(header));
        header = next_header;
    }
    /* 3. スラブキャッシュ構造体を削除する */
    slab_cache_delete(cache);

    /* 4. lockを解除 */
    release(&slab_lock);
}

/**
 * @ingroup slab
 * @brief スラブキャッシュからオブジェクトを割り当てる.
 *
 * @param cache オブジェクトを割り当てるスラブキャッシュへのポインタ
 * @return 割り当てられたオブジェクトへのポインタ
 */
void *slab_cache_alloc(struct slab_cache *cache) {
    struct slab_header *header;
    uint32_t index = 0, next_index, *free_list = 0;

    /* 1. lockを取得する */
    acquire(&cache->lock);

    /* 2. 一部割り当て済みリストから割り当てる */
    header = cache->slabs_partial;
    /* 3. 一部割り当て済みリストがない場合は割り当てる */
    if (!header) {
        header = slab_new(cache);
        cache->slabs_partial = header;
    }

    /* 4. フリーリストを更新する */
    free_list = (uint32_t *)(header + 1);       /* free_listの先頭アドレス*/
#if 0
    if (index < 2)
        debug_bytes("free_list:", (char *)free_list, 128);
#endif
    index = (uint32_t)(header->free - free_list); /* 使用するフリーオブジェクトのインデックス */
    next_index = *header->free;                 /* 次のフリーオブジェクトのインデックス */
                                                /* 初期化時に i+1 が設定されている */
    trace("index: 0x%x, next_index: 0x%x", index, next_index);
    free_list[index] = SLAB_FREE_END;           /* 使用するオブジェクト位置に空き終了マークを付ける */
    header->free = &free_list[next_index];      /* header->freeを更新する */
    trace("free_list: [%d]: 0x%x, [%d]: 0x%x", index, free_list[index], next_index, free_list[next_index]);
    trace("header->free: %p = 0x%x", header->free, *header->free);

    /* 5. header->freeが終了マークを指した場合はslabs_fullにつなげる */
    if (*header->free == SLAB_FREE_END) {
        trace("header->free is full");
        /* 4.1 headerをslabs_partialリストから外す */
        cache->slabs_partial = header->next;
        /* 4.2 headerをslabs_fullの先頭につなげる */
        if (cache->slabs_full) {
            header->next = cache->slabs_full;
        } else {
            header->next = NULL;
        }
        cache->slabs_full = header;
    }

    /* 6. lockを開放する */
    release(&cache->lock);

    /* 7. 割り当てたオブジェクトを返す */
    void *addr = header->object + (cache->object_size * index);
    trace("addr: %p = %p + (0x%x * 0x%x)", addr, header->object, cache->object_size, index);
#if 0
    if (index < 2) {
        debug_bytes("free_list:", (char *)free_list, 128);
    }
    printf("\n");
#endif
    return addr;
    //return header->object + (cache->object_size * index);
}

/**
 * @ingroup slab
 * @brief スラブキャッシュのオブジェクトを解放する.
 *
 * @param cache オブジェクトを解放スラブキャッシュへのポインタ
 * @param obj 解放するオブジェクト
 */
void slab_cache_free(struct slab_cache *cache, void *obj) {
    uint32_t index, next_index, *free_list;
    trace("free obj: %p", obj);

    struct page *page = page_find_head(page_find_by_address(obj));
    struct slab_header *header = (void*)page_address(page), *h, **hp;

    /* 1. lockを取得する */
    acquire(&cache->lock);

    /* 2. free_listを求める */
    free_list = (void *)(header + 1);
    /* 3. オブジェクトのインデックスを求める */
    index = ((uint8_t *)obj - header->object) / cache->object_size;
    /* 4. 次のフリーオブジェクトのインデックスを求める */
    next_index = (uint32_t)(header->free - free_list);
    trace("index: 0x%x, next_index: 0x%x", index, next_index);
    /* 5. 解放するオブジェクトを次のフリーオブジェクトとする */
    free_list[index] = next_index;
    header->free = &free_list[index];
    trace("header->free: %p", header->free);
    /* 6. 次のオブジェクトが終了マークだった場合は */
    /*    slabs_fullからslabs_partialに付け替える  */
    if (free_list[next_index] == SLAB_FREE_END) {
        trace("next_index is FREE_END");
        hp = &cache->slabs_full;
        h = cache->slabs_full;
        while (h) {
            if (h == header) {
                *hp = h->next;

                h->next = cache->slabs_partial;
                cache->slabs_partial = h;
                /* lockを開放する */
                release(&slab_lock);
                /* 処理終了 */
                return;
            }

            hp = &h->next;
            h = h->next;
        }
        /* 何かがおかしいのでpanic */
        panic("broken slab");
    }

    /* 7. lockを開放する */
    release(&cache->lock);
}
