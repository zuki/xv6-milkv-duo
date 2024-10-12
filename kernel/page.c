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
 * @file page.c
 */
#include "page.h"
#include "defs.h"

/**
 * @ingroup page
 * @var pages
 * @brief ページ配列. 全物理ページの配列.
 */
struct page *pages;

/**
 * @ingroup page
 * @brief ページシステムを初期化する.
 */
void page_init(void) {
    buddy_init();
    slab_cache_init();
}

/**
 * @ingroup page
 * @brief 指定されたページの仮想アドレスを返す.
 *
 * @param page ページヘのポインタ
 * @return ページの仮想アドレス
 */
void *page_address(const struct page *page) {
    return PAGE_START + (PAGE_SIZE * page->index);
}

/**
 * @ingroup page
 * @brief 指定された仮想アドレスのページを返す.
 *
 * @param address ページの仮想アドレス
 * @return ページ構造体へのポインタ
 */
struct page *page_find_by_address(void *address) {
    page_index index = (page_index)(((char*)address - PAGE_START) / PAGE_SIZE);

    if (index < PAGE_NUM) {
        return &pages[index];
    } else {
        return NULL;
    }
}

/**
 * @ingroup page
 * @brief 指定されたページが含まれるブロックの先頭ページを返す.
 *
 * @param address ページ構造体へのポインタ
 * @return ページ構造体へのポインタ
 */
struct page *page_find_head(const struct page *page) {
    page_index index = page->index;

    while (index < PAGE_NUM) {
        if (pages[index].flags & PF_FIRST_PAGE) {
            return &pages[index];
        }

        index--;
    }

    return NULL;
}

/**
 * @ingroup page
 * @brief ページブロックを解放する.
 *
 * @param address ページブロックの先頭のページ構造体へのポインタ
 * @return ページ構造体へのポインタ
 */
void page_cleanup(struct page **page) {
    if (*page) {
        buddy_free(*page);
    }
}
