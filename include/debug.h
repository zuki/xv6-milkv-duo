#ifndef INC_DEBUG_H
#define INC_DEBUG_H

#include <common/types.h>
#include <elf.h>

struct tls_module {
    struct tls_module *next;
    void *image;
    size_t len, size, align, offset;
};

struct __libc {
    char can_do_threads;
    char threaded;
    char secure;
    signed char need_locks;
    int threads_minus_1;    // 4
    size_t *auxv;           // 8
    struct tls_module *tls_head;    // 16
    size_t tls_size, tls_align, tls_cnt;    // 24, 32, 40
    size_t page_size;
    void *global_locale;
};

struct td_index {
    size_t args[2];
    struct td_index *next;
};

struct dso {
    unsigned char *base;
    char *name;
    size_t *dynv;
    struct dso *next, *prev;
    struct proghdr *phdr;
    int phnum;
    size_t phentsize;
    Sym *syms;
    uint64_t *hashtab;
    uint32_t *ghashtab;
    int16_t *versym;
    char *strings;
    struct dso *syms_next, *lazy_next;
    size_t *lazy, lazy_cnt;
    unsigned char *map;
    size_t map_len;
    dev_t dev;
    ino_t ino;
    char relocated;
    char constructed;
    char kernel_mapped;
    char mark;
    char bfs_built;
    char runtime_loaded;
    struct dso **deps, *needed_by;
    size_t ndeps_direct;
    size_t next_dep;
    void *ctor_visitor;
    char *rpath_orig, *rpath;
    struct tls_module tls;
    size_t tls_id;
    size_t relro_start, relro_end;
    uintptr_t *new_dtv;
    unsigned char *new_tls;
    struct td_index *td_index;
    struct dso *fini_next;
    char *shortname;
    //struct fdpic_loadmap *loadmap;
    void *loadmap;
    struct funcdesc {
        void *addr;
        size_t *got;
    } *funcdescs;
    size_t *got;
    char buf[];
};

struct musl_file {
    unsigned flags;
    unsigned char *rpos, *rend;
    int (*close)(struct musl_file *);
    unsigned char *wend, *wpos;
    unsigned char *mustbezero_1;
    unsigned char *wbase;
    size_t (*read)(struct musl_file *, unsigned char *, size_t);
    size_t (*write)(struct musl_file *, const unsigned char *, size_t);
    off_t (*seek)(struct musl_file *, off_t, int);
    unsigned char *buf;
    size_t buf_size;
    struct musl_file *prev, *next;
    int fd;
    int pipe_pid;
    long lockcount;
    int mode;
    int lock;
    int lbf;
    void *cookie;
    off_t off;
    char *getln_buf;
    void *mustbezero_2;
    unsigned char *shend;
    off_t shlim, shcnt;
    struct musl_file *prev_locked, *next_locked;
    void *locale;
};

#endif
