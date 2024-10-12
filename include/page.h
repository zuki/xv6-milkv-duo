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
 * @file page.h
 */
#ifndef INC_PAGE_H
#define INC_PAGE_H

#include "list.h"

/**
 * @ingroup page
 * @def PAGE_START
 * @brief ページ領域の先頭アドレス
 *        pages[0:PAGE_NUM] | page0, page1, ...
 */
#define PAGE_START ((char*)(0x80600000))

/**
 * @ingroup page
 * @def PAGE_SIZE
 * @brief ページサイズ (4KB)
 */
#define PAGE_SIZE 0x1000
/**
 * @ingroup page
 * @def PAGE_NUM
 * @brief 総ページ数.
 *        ページに使用できるのは 0x3800 (14336) * 0x1000 (5096) = 56MB
 */
#define PAGE_NUM  0x3800

/**
 * @ingroup page
 * @def PAGE_MAX_ORDER
 * @brief ページブロックの最大オーダ数.
 *        最大ページブロックは 2^10 (1024) * 4KB = 4MB.
 */
#define PAGE_MAX_ORDER 10
/**
 * @ingroup page
 * @def PAGE_MAX_DEPTH
 * @brief 最大のページ深さ.
 */
#define PAGE_MAX_DEPTH (PAGE_MAX_ORDER + 1)
/**
 * @ingroup page
 * @def PF_FREE_LIST
 * @brief ページフラグ: 空きリスト
 */
#define PF_FREE_LIST  (1 << 0)
/**
 * @ingroup page
 * @def PF_FIRST_PAGE
 * @brief ページフラグ: 2分割の前方ブロック
 */
#define PF_FIRST_PAGE (1 << 1)
/**
 * @ingroup page
 * @def _page_cleanup_
 * @brief ページクリーンアップ関数
 */
#define _page_cleanup_ _cleanup_(page_cleanup)

/**
 * @ingroup page
 * @var page_index
 * @brief pages配列のインデックス.
 */
typedef uint64_t page_index;

/**
 * @ingroup page
 * @struct page
 * @brief ページ構造体.
 */
struct page {
    page_index index;       /**< ページインデックス (0 : PAGE_NUM - 1) */
    unsigned int flags;     /**< フラグ */
    unsigned int order;     /**< ページブロックの大きさ (2^order) */
    struct page* next;      /**< 次のページ構造体へのポインタ */
};

extern struct page *pages;

#endif
