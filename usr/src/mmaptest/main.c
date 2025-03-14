#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define MMAPBASE    0x2000000000UL
#define MMAPTOP     0x3FFFFFD000UL

char *filename = "mmaptest.txt";

char temp[8192];

// File backend mappings test (21 tests)
void file_private_test();
void file_shared_test();
void file_invalid_fd_test();
void file_invalid_flags_test();
void file_writeable_shared_mapping_on_ro_file_test();
void file_ro_shared_mapping_on_ro_file_test();
void file_exceed_size_test();
void file_exceed_count_test();
void file_empty_file_size_test();
void file_private_mapping_perm_test();
void file_pagecache_coherency_test();
void file_private_with_fork_test();
void file_shared_with_fork_test();
void file_mapping_with_offset_test();
void file_given_addr_test();
void file_invalid_addr_test();
void file_overlap_given_addr_test();
void file_intermediate_given_addr_test();
void file_intermediate_given_addr_not_possible_test();
void file_exceeds_file_size_test();
void file_mapping_on_wo_file_test();

// Anonymous tests (15 tests)
void anon_private_test();
void anon_shared_test();
void anon_private_fork_test();
void anon_shared_multi_fork_test();
void anon_exceed_size_test();
void anon_exceed_count_test();
void anon_zero_size_test();
void anon_missing_flags_test();
void anon_private_shared_fork_test();
void anon_given_addr_test();
void anon_invalid_addr_test();
void anon_overlap_given_addr_test();
void anon_intermediate_given_addr_test();
void anon_intermediate_given_addr_not_possible_test();
void anon_write_on_ro_mapping_test();

// Other Mmap tests (5 Tests)
void munmap_partial_size_test();
void mmap_write_on_ro_mapping_test();
void mmap_none_permission_test();
void mmap_valid_map_fixed_test();
void mmap_invalid_map_fixed_test();

void write_test(size_t n);

void file_tests() {
    file_invalid_fd_test();                             // F-01
    file_invalid_flags_test();                          // F-02
    file_writeable_shared_mapping_on_ro_file_test();    // F-03
    file_ro_shared_mapping_on_ro_file_test();           // F-04
    file_private_mapping_perm_test();                   // F-05
    file_exceed_size_test();                            // F-06
    file_exceed_count_test();                           // F-07
    file_empty_file_size_test();                        // F-08
    file_private_test();                                // F-09
    file_shared_test();                                 // F-10
    file_pagecache_coherency_test();                    // F-11
    file_private_with_fork_test();                      // F-12
    file_shared_with_fork_test();                       // F-13
    file_mapping_with_offset_test();                    // F-14
    file_given_addr_test();                             // F-15
    file_invalid_addr_test();                           // F-16
    file_overlap_given_addr_test();                     // F-17
    file_intermediate_given_addr_test();                // F-18
    file_intermediate_given_addr_not_possible_test();   // F-19
    file_exceeds_file_size_test();                      // F-20
    file_mapping_on_wo_file_test();                     // F-21
}

void anonymous_tests() {
    anon_private_test();                                // A-01
    anon_shared_test();                                 // A-02
    anon_private_fork_test();                           // A-03
    anon_shared_multi_fork_test();                      // A-04
    anon_private_shared_fork_test();                    // A-05
    anon_missing_flags_test();                          // A-06
    anon_exceed_count_test();                           // A-07
    anon_exceed_size_test();                            // A-08
    anon_zero_size_test();                              // A-09
    anon_given_addr_test();                             // A-10
    anon_invalid_addr_test();                           // A-11
    anon_overlap_given_addr_test();                     // A-12
    anon_intermediate_given_addr_test();                // A-13
    anon_intermediate_given_addr_not_possible_test();   // A-15
}

void other_tests() {
    munmap_partial_size_test();                         // O-01
    mmap_write_on_ro_mapping_test();                    // O-02
    mmap_none_permission_test();                        // O-03
    mmap_valid_map_fixed_test();                        // O-04
    mmap_invalid_map_fixed_test();                      // O-05
}

void write_test(size_t n) {
    memset(temp, 'a', n);

    int fd = open(filename, O_CREAT | O_WRONLY, 0666);
    if (fd == -1) {
        printf("write_test failed: at open\n");
        exit(1);
    }
    if (write(fd, temp, n) != n) {
        printf("write_test failed: at write\n");
        exit(1);
    }
    close(fd);
}

// Utility strcmp function
int my_strcmp(char *a, char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return i + 1;
        }
    }
    return 0;
}

int file_ok = 0, file_ng = 0, anon_ok = 0, anon_ng = 0, other_ok = 0,
    other_ng = 0;

int main(int args, char *argv[]) {
    file_tests();
    anonymous_tests();
    other_tests();
    //anon_private_fork_test();
    //anon_shared_multi_fork_test();
    //anon_private_shared_fork_test();
    printf("\nfile_test:  ok: %d, ng: %d\n", file_ok, file_ng);
    printf("anon_test:  ok: %d, ng: %d\n", anon_ok, anon_ng);
    printf("other_test: ok: %d, ng: %d\n", other_ok, other_ng);
    return 0;
}

// <!!-------- ファイルが背後にあるマッピング ---------- !!>
// [F-01] 不正なfdを指定した場合のテスト
// test 1: 負のfd (-1), オープンされていないfd (10), 範囲外のfd (128)を指定した場合はエラーになる
void file_invalid_fd_test() {
    printf("[F-01] 不正なfdを指定\n");
    write_test(100);
    int size = 100;
    int fds[3] = {-1, 10, 128};
    for (int i = 0; i < 3; i++) {
        void *addr = mmap((void *)0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fds[i], 0);
        if (addr == MAP_FAILED) {
            continue;
        }
        munmap(addr, size);
        unlink(filename);
        file_ng++;
        printf("[F-01] ng %d\n", i);
        return;
    }

    unlink(filename);
    file_ok++;
    printf("[F-01] ok\n");
}

// [F-02] 不正なフラグを指定した場合のテスト
// test 1: 不正なフラグ (0) を指定した場合はエラーになる
void file_invalid_flags_test() {
    printf("\n[F-02] 不正なフラグを指定\n");
    int size = 100;
    write_test(size);

    int fd = open(filename, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        printf("open failed\n");
        goto f2_bad_0;
    }

    char *addr = mmap((void *)0, size, PROT_READ | PROT_WRITE, 0, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        unlink(filename);
        file_ok++;
        printf("[F-02] ok\n");
        return;
    }

    int ret = munmap((void *)addr, size);
    if (ret == -1) {
        printf("munmap error\n");
    }
    close(fd);
    unlink(filename);

f2_bad_0:
    file_ng++;
    printf("[F-02] ng\n");
}

// [F-03] readonlyファイルにPROT_WRITEな共有マッピングをした場合
// test 1: この指定をした場合はエラーになる
void file_writeable_shared_mapping_on_ro_file_test() {
    printf("\n[F-03] readonlyファイルにPROT_WRITEな共有マッピング\n");
    write_test(200);

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("open failed: \n");
        goto f3_bad_0;
    }
    char *addr = mmap((void *)0, 200, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        unlink(filename);
        file_ok++;
        printf("[F-03] ok\n");
        return;
    }

    int ret = munmap((void *)addr, 200);
    if (ret == -1) {
        printf("munmap error\n");
    }

f3_bad_1:
    close(fd);
f3_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-03] ng\n");
}

// [F-04] readonlyファイルにPROT_READのみの共有マッピングをした場合のテスト
// test 1: マッピングは成功する
void file_ro_shared_mapping_on_ro_file_test() {
    printf("\n[F-04] readonlyファイルにPROT_READのみの共有マッピング\n");
    write_test(200);

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("open failed\n");
        goto f4_bad_0;
    }
    void *addr = mmap((void *)0, 200, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap error\n");
        goto f4_bad_1;
    }
    //printf("- mapped addr=%p\n", addr);
    int res = munmap(addr, 200);
    if (res == -1) {
        printf("munmap error\n");
        goto f4_bad_1;
    }

    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-04] ok\n");
    return;

f4_bad_1:
    close(fd);
f4_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-04] ng\n");
}

// [F-05] read onlyなファイルに対するPROT指定の異なるプライベートマッピンのテスト
// test 1: PROT_READのみ指定した場合、成功する
// test 2: PROT_WRITEも指定した場合も成功し、書き込みも可能
void file_private_mapping_perm_test() {
    printf("\n[F-05] PROT指定の異なるプライベートマッピング\n");
    write_test(200);

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("open failed\n");
        goto f5_bad_0;
    }

    // read only private mapping on read only opened file
    char *addr = mmap((void *)0, 200, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap1 failed\n");
        goto f5_bad_1;
    }
    //printf("- addr=%p\n", addr);

    // write & read private mapping on read only opened file
    char *addr2 = mmap((void *)0, 200, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE | MAP_POPULATE, fd, 0);
    if (addr2 == MAP_FAILED) {
        printf("mmap2 failed\n");
        goto f5_bad_2;
    }

    //printf("- addr2=%p\n", addr2);fflush(stdout);
    for (int i = 0; i < 20; i++) {
        addr2[i] = 'a';
    }
    // printf("- set addrs[0-20]='a'\n");

    int res = munmap(addr, 200);
    if (res < 0) {
        printf("munmap failed\n");
    }
    res = munmap(addr2, 200);
    if (res < 0) {
        printf("munmap2 failed\n");
        goto f5_bad_1;
    }

    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-05] ok\n");
    return;

f5_bad_2:
    munmap((void *)addr, 200);
f5_bad_1:
    close(fd);
f5_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-05] ng\n");
}

// [F-06] MMAPTOPを超えるサイズをマッピングした場合のテスト
// test 1: エラーになる
void file_exceed_size_test() {
    printf("\n[F-06] MMAPTOPを超えるサイズをマッピング\n");
    write_test(200);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f6_bad_0;
    }

    size_t size = MMAPTOP;
    void *addr = mmap((void *)0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr != MAP_FAILED) {
        printf("mmap failed\n");
        goto f6_bad_1;
    }

    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-06] ok\n");
    return;

f6_bad_1:
    close(fd);
f6_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-06] ng\n");
}

// [F-07] 連続したマッピングを行うテスト
// test 1: 50回の連続マッピングが成功する
void file_exceed_count_test() {
    printf("\n[F-07] 連続マッピング\n");
    int count = 50, i = 0, size = 4096;
    void *addr[count];

    write_test(size);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f7_bad_0;
    }

    for (; i < count; i++) {
        addr[i] = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd, 0);
        if (addr[i] == MAP_FAILED) {
            printf("mmap %d failed \n", i);
            break;
        }
    }
    // mmap成功
    if (i == count) {
        int ecnt = 0;
        for (int j = 0; j < count; j++) {
            int ret = munmap(addr[j], size);
            if (ret == -1) {
                printf("munmap %d failed\n", j);
                ecnt++;
            }
        }
        if (ecnt > 0)
            goto f7_bad_1;

        close(fd);
        unlink(filename);
        file_ok++;
        printf("[F-07] ok\n");
        return;
    }

    // mmap失敗
    for (int j = 0; j < i; j++) {
        munmap(addr[j], size);
    }

f7_bad_1:
    close(fd);
f7_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-07] ng\n");
}

// [F-08] 空のファイルを共有マッピングした場合のテスト
// test 1: マッピングは成功する
// test 2: マッピングに書き込んだ結果はファイルに反映する
void file_empty_file_size_test() {
    printf("\n[F-08] 空のファイルを共有マッピング\n");

    write_test(100);
    int fd = open(filename, O_TRUNC | O_RDWR, 0666);
    if (fd == -1) {
        printf("open failed\n");
        goto f8_bad_0;
    }

    char *addr = mmap((void *)0, 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        goto f8_bad_1;
    }

    for (int i = 0; i < 24; i++) {
        addr[i] = 'a';
    }

    int res = munmap(addr, 32);
    if (res == -1) {
        printf("munmap failed\n");
        goto f8_bad_1;
    }

    close(fd);

    // Check if data is correctly written into the file
    fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open faild 2\n");
        goto f8_bad_0;
        return;
    }

    char buf[32];
    int n = read(fd, buf, 32);
    if (n != 32) {
        printf("read faild\n");
        goto f8_bad_1;
    }

    for (int i = 0; i < 24; i++) {
        if (buf[i] != 'a') {
            printf("buf[%d] isn't a\n", i);
            goto f8_bad_1;
        }
    }
    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-08] ok\n");
    return;

f8_bad_1:
    close(fd);
f8_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-08] ng\n");
}

// [F09] ファイルが背後にあるプライベートマッピングのテスト
// test 1: ファイルから読み込んだバッファ1とmmapしたアドレスから読み込んだデータが一致する
// test 2: バッファ1とアドレスデータの一部を同じように書き換えると双方は一致する
// test 3: 再度ファイルから読み込んだバッファ2とバッファ1は異なる（プライベートマッピングは
//         書き戻さないため）
void file_private_test() {
    printf("\n[F-09] プライベートマッピング\n");
    int size = 128;

    write_test(size);     // 'a'を128バイト書き込み

    char *buf = (char *)malloc(size);
    if (buf == NULL) {
        printf("no memory for buf\n");
        goto f9_bad_0;
    }
    char *buf2 = (char *)malloc(size);
    if (buf2 == NULL) {
        printf("no memory for buf2\n");
        free(buf);
        goto f9_bad_0;
    }

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f9_bad_0;
    }

    int n = read(fd, buf, size);    // buf = 'a' x 128
    if (n != size) {
        printf("read faild: n=%d, size=128\n", n);
        goto f9_bad_1;
    }
    char *addr = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed 1\n");
        goto f9_bad_1;
    }

    if (my_strcmp(buf, addr, size) != 0) {   // buf = addr = "a...a"
        printf("strcmp1 failed\n");
        goto f9_bad_2;
    }

    for (int i = 0; i < 64; i++) {
        addr[i] = 'p';
        buf[i] = 'p';
    }

/*
    printf("write(u) (%p): ", addr);
    int pos;
    for (pos = 0; pos < 70; pos++) {
        printf("%c", addr[pos]);
        if (((pos + 1) % 10) == 0) printf(" ");
    }
    printf("\n");
    printf("write(u) (%p): ", buf);
    for (pos = 0; pos < 70; pos++) {
        printf("%c", buf[pos]);
        if (((pos + 1) % 10) == 0) printf(" ");
    }
    printf("\n");
*/
    int pos;
    if ((pos = my_strcmp(buf, addr, size)) != 0) {   // buf = addr = 'p' x 40 + 'a' x 960
        printf("strcmp2 failed: pos: %d, buf: %c, addr: %c\n", pos - 1, buf[pos - 1], addr[pos - 1]);
        goto f9_bad_2;
    }

    int ret = munmap((void *)addr, size);
    if (ret == -1) {
        printf("munmap failed\n");
        goto f9_bad_1;
    }

    int fd2 = open(filename, O_RDONLY);

    // Read from the file again and check if it is not equal to mapping data as
    // mapping is private

    n = read(fd2, buf2, size);
    if (n != size) {
        printf("read2 failed\n");
        goto f9_bad_2;
    }
    if (my_strcmp(buf, buf2, size) == 0) {
        printf("strcmp3 failed\n");
        goto f9_bad_2;
    }

    close(fd2);
    close(fd);
    unlink(filename);
    free(buf);
    free(buf2);
    file_ok++;
    printf("[F-09] ok\n");
    return;

f9_bad_2:
    munmap((void *)addr, size);
f9_bad_1:
    close(fd);
    free(buf);
    free(buf2);
f9_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-09] ng\n");
}

// [F-10] ファイルが背後にある共有マッピングのテスト
// test 1: ファイルから読み込んだバッファ1とmmapしたアドレスから読み込んだデータが一致する
// test 2: バッファ1とアドレスデータの一部を同じように書き換えると双方は一致する
// test 3: 再度ファイルから読み込んだバッファ2とバッファ1も一致する（共有マッピングは
//         書き戻すため）
void file_shared_test() {
    printf("\n[F-10] 共有マッピング\n");
    int size = 128;

    write_test(size);

    char *buf = (char *)malloc(size);
    if (buf == NULL) {
        printf("no memory for buf\n");
        goto f10_bad_0;
    }
    char *buf2 = (char *)malloc(size);
    if (buf2 == NULL) {
        printf("no memory for buf2\n");
        free(buf);
        goto f10_bad_0;
    }

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f10_bad_0;
    }

    int n = read(fd, buf, size);
    if (n != size) {
        printf("read1 faild (%d != %d)\n", n, size);
        goto f10_bad_2;
    }

    char *addr = mmap((void *)0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap1 failed\n");
        goto f10_bad_2;
    }

    // Check if both entries have same data
    if (my_strcmp(buf, addr, size) != 0) {
        printf("strcmp1 failed\n");
        goto f10_bad_3;
    }
    for (int i = 0; i < 64; i++) {
        addr[i] = 'a';
        buf[i] = 'a';
    }
    // Check if both mappings are same after edit
    if (my_strcmp(buf, addr, size) != 0) {
        printf("strcmp2 failed\n");
        goto f10_bad_3;
    }
    int res = munmap(addr, size);
    if (res == -1) {
        printf("munmap failed\n");
        goto f10_bad_2;
    }

    int fd2 = open(filename, O_RDONLY);
    if (fd2 < 0) {
        printf("open2 failed\n");
        goto f10_bad_2;
    }

    // Read from the file again and check if it is equal to mapping data
    n = read(fd2, buf2, size);
    if (n != size) {
        printf("read2 failed\n");
        close(fd2);
        goto f10_bad_2;
    }

    if (my_strcmp(buf, buf2, size) != 0) {
        printf("strcmp3 failed 5\n");
        close(fd2);
        goto f10_bad_2;
    }

    free(buf);
    free(buf2);
    close(fd2);
    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-10] ok\n");
    return;

f10_bad_3:
    munmap(addr, size);
f10_bad_2:
f10_bad_1:
    close(fd);
    free(buf);
    free(buf2);
f10_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-10] ng\n");
    return;
}

// [F-11] ファイルに書き込み後にページキャッシュが変更されるかのテスト
// test 1: 先頭からマッピングしたデータはファイルに一致する
// test 2: オフセット位置に書き込んだファイルとオフセット指定でマッピングしたデータは一致する
void file_pagecache_coherency_test() {
    printf("\n[F-11] ファイル書き込み後にページキャッシュを変更\n");
    int size = 100;
    int size2 = 4196;

    //printf("[F-11] buf: %p, buf2: %p\n", buf, buf2);
    //fflush(stdout);

    write_test(4296);

    char *buf = (char *)malloc(4096);
    if (buf == NULL) {
        printf("no memory for buf\n");
        goto f11_bad_0;
    }
    char *buf2 = (char *)malloc(size);
    if (buf2 == NULL) {
        printf("no memory for buf2\n");
        free(buf);
        goto f11_bad_0;
    }

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open1 failed\n");
        goto f11_bad_0;
    }

    int n = read(fd, buf, size);
    if (n != size) {
        printf("read1 failed\n");
        goto f11_bad_1;
    }
    char *addr = mmap((void *)0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        goto f11_bad_1;
    }
    //printf("[F-11] addr: %p\n", addr);
    //fflush(stdout);

    int j = my_strcmp(buf, addr, size);
    if (j != 0) {
        printf("strcmp1 failed: pos: %d, buf: %c, addr: %c\n", j, buf[j], addr[j]);
        goto f11_bad_2;
    }

    for (int i = 0; i < size; i++)
        buf2[i] = 'a';

    //printf("[F-11] buf2[0]=%x, buf2[99]=%x\n", buf2[0], buf2[99]);
    //fflush(stdout);

    // オフセット100の位置から100バイトだけ'a'を書き込む:  'a' * 2 * size
    n = write(fd, buf2, size);
    if (n != size) {
        printf("write failed\n");
        close(fd);
        goto f11_bad_2;
    }
    close(fd);

    int fd2 = open(filename, O_RDWR);  // Again open file just to seek to start
    if (fd2 == -1) {
        printf("open2 failed\n");
        goto f11_bad_2;
    }

    n = read(fd2, buf, size2 - 100);  // offset = 4096にseek
    if (n != (size2 - 100)) {
        printf("read 2 failed: n=%d\n", n);
        goto f11_bad_3;
    }

    n = read(fd2, buf, (size2 - 4096));  // offset = 4096から100バイト読み込む
    if (n != (size2 - 4096)) {
        printf("read 3 failed: n=%d\n", n);
        close(fd2);
        goto f11_bad_3;
    }

    //printf("[F-11] buf[0]=%x, buf[99]=%x, buf[100]=%x\n", buf[0], buf[99], buf[100]);
    //fflush(stdout);

    // offset 4096 から 100バイトマッピング
    char *addr2 = mmap((void *)0, (size2 - 4096), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd2, 4096);
    if (addr2 == MAP_FAILED) {
        printf("mmap addr2 failed\n");
        goto f11_bad_3;
    }

    //printf("[F-11] buf[0], buf[99]=%x, buf[100]=%x\n", buf[0], buf[99], buf[100]);
    //fflush(stdout);

    if (my_strcmp(addr2, buf, size2 - 4096) != 0) {
        printf("strcmp 2 failed\n");
        goto f11_bad_4;
    }

    int ret = munmap(addr2, size2 - 4096);
    if (ret == -1) {
        printf("munmap addr2 failed");
        goto f11_bad_3;
    }

    ret = munmap(addr, size);
    if (ret == -1) {
        printf("munmap addr failed\n");
        close(fd2);
        goto f11_bad_1;
    }

    close(fd2);
    unlink(filename);
    free(buf);
    free(buf2);
    file_ok++;
    printf("[F-11] ok\n");
    return;

f11_bad_4:
    munmap(addr2, size2 - 4096);
f11_bad_3:
    close(fd2);
f11_bad_2:
    munmap(addr, size);
f11_bad_1:
    close(fd);
    free(buf);
    free(buf2);
f11_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-11] ng\n");
    return;
}

// private file backed mapping with fork test
void file_private_with_fork_test() {
    printf("\n[F-12] forkを行うプライベードマッピング\n");
    int size = 200;

    write_test(size);

    char *buf = (char *)malloc(size);
    if (buf == NULL) {
        printf("no memory for buf\n");
        goto f12_bad_0;
    }

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f12_bad_0;
    }

    if (read(fd, buf, size) != size) {
        printf("read failed\n");
        goto f12_bad_1;
    }

    char *addr = mmap((void *)0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        goto f12_bad_1;
    }
    //printf("addr: %p\n", addr);

    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        goto f12_bad_2;
    } else if (pid == 0) {
        int ret = 0;
        for (int i = 0; i < 50; i++) {
            addr[i] = 'n';
        }
        // The mapping should not be same as we have edited the data
        if (my_strcmp(addr, buf, size) == 0) {
            printf("strcmp child failed\n");
            ret = 1;
        }
        close(fd);
        //unlink(filename);
        exit(ret);
    } else {
        int status;
        //printf("child pid: %d\n", pid);
        wait(&status);
        // As it is private mapping therefore it should be same as read
        if (my_strcmp(addr, buf, size) != 0) {
            printf("strcmp parent failed\n");
            goto f12_bad_2;
        }
        int res = munmap((void *)addr, size);
        if (res == -1) {
            printf("munmap failed\n");
            goto f12_bad_1;
        }
        if (status) {
            goto f12_bad_1;
        }
        close(fd);
        unlink(filename);
        free(buf);
        file_ok++;
        printf("[F-12] ok\n");
        return;
    }

f12_bad_2:
    munmap(addr, size);
f12_bad_1:
    close(fd);
    free(buf);
f12_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-12] ng\n");
    return;
}

// shared file backed mapping with fork test
void file_shared_with_fork_test() {
    printf("\n[F-13] forkのある共有マッピング\n");
    int size = 200;
    int status;

    write_test(size);

    char *buf = (char *)malloc(size);
    if (buf == NULL) {
        printf("no memory for buf\n");
        goto f13_bad_0;
    }

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open1 failed\n");
        goto f13_bad_0;
    }
    if (read(fd, buf, size) != size) {
        printf("read1 failed\n");
        goto f13_bad_1;
    }

    char *addr2 = mmap((void *)0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr2 == MAP_FAILED) {
        printf("mmap failed\n");
        goto f13_bad_1;
    }

    // fork
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        goto f13_bad_1;
    } else if (pid == 0) {
        int ret = 0;
        for (int i = 0; i < 50; i++) {
            addr2[i] = 'o';
        }
        if (my_strcmp(addr2, buf, size) == 0) {
            printf("child strcmp failed\n");
            ret = 1;
        }
        printf("child : addr2[0, 1, 49, 50]=[%c, %c, %c, %c], buf=[%c, %c, %c, %c]\n",
                addr2[0], addr2[1], addr2[49], addr2[50], buf[0], buf[1], buf[49], buf[50]);
        close(fd);
        //unlink(filename);
        exit(ret);
    } else {
        //printf("child pid: %d\n", pid);
        wait(&status);
        printf("child [%d] exit with %d\n", pid, status);
        char buf2[size];

        printf("parent 1: addr2[0, 1, 49, 50]=[%c, %c, %c, %c], buf=[%c, %c, %c, %c]\n",
                addr2[0], addr2[1], addr2[49], addr2[50], buf[0], buf[1], buf[49], buf[50]);
        // The data written in child process should persist here
        if (my_strcmp(addr2, buf, size) == 0) {
            printf("parent strcmp1 failed\n");
            goto f13_bad_2;
        }
        int ret = munmap(addr2, size);
        if (ret == -1) {
            printf("parent munmap failed\n");
            goto f13_bad_1;
        }
        close(fd);

        // Check if data is written into file
        int fd2 = open(filename, O_RDWR);
        if (fd2 == -1) {
            printf("open2 failed\n");
            goto f13_bad_0;
        }

        for (int i = 0; i < 50; i++) {
            buf[i] = 'o';
        }
        ssize_t n;
        if ((n = read(fd2, buf2, size)) != size) {
            printf("parent read2 failed: size=%d, n=%ld\n", size, n);
            close(fd2);
            goto f13_bad_0;
        }

        printf("parent 2: buf2[0, 1, 49, 50]=[%c, %c, %c, %c], buf=[%c, %c, %c, %c]\n",
            buf2[0], buf2[1], buf2[49], buf2[50], buf[0], buf[1], buf[49], buf[50]);

        if (my_strcmp(buf2, buf, size) != 0) {
            printf("parent strcmp2 failed\n");
            close(fd2);
            goto f13_bad_0;
        }
        close(fd2);

        if (status) {
            goto f13_bad_0;
        }

        unlink(filename);
        free(buf);
        file_ok++;
        printf("[F-13] ok\n");
        return;
    }

f13_bad_2:
    munmap(addr2, size);
f13_bad_1:
    close(fd);
    free(buf);
f13_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-13] ng\n");
}

// [F-14] オフセットを指定したプライベートマッピングのテスト
// test 1: オフセットマッピングとファイル読み込みの結果は一致する
// test 2: マッピングアドレスへの書き込みとバッファへの書き込みの結果は一致する
void file_mapping_with_offset_test() {
    printf("\n[F-14] オフセットを指定したプライベートマッピング\n");
    int size = 4296;

    write_test(4296);

    char *buf = (char *)malloc(size);
    if (buf == NULL) {
        printf("no memory for buf\n");
        goto f14_bad_0;
    }

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f14_bad_0;
    }

    int n = read(fd, buf, 4096);
    if (n != 4096) {
        printf("read 1 failed, n=%d\n", n);
        goto f14_bad_1;
    }

    n = read(fd, buf, size);
    if (n != (size - 4096)) {
        printf("read 2 failed, n=%d\n", n);
        goto f14_bad_1;
    }

    // オフセット 4096 から 200バイトマッピング
    char *addr = mmap((void *)0, (size - 4096), PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE, fd, 4096);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        goto f14_bad_1;
    }

    if (my_strcmp(buf, addr, (size - 4096)) != 0) {
        printf("strcmp failed\n");
        goto f14_bad_2;
    }

    for (int i = 0; i < 40; i++) {
        addr[i] = 'p';
        buf[i] = 'p';
    }

    if (my_strcmp(buf, addr, (size - 4096)) != 0) {
        printf("strcmp 2 failed\n");
        goto f14_bad_2;
    }

    int ret = munmap(addr, (size - 4096));
    if (ret == -1) {
        printf("munmap failed\n");
        goto f14_bad_1;
    }

    close(fd);
    unlink(filename);
    free(buf);
    file_ok++;
    printf("[F-14] ok\n");
    return;

f14_bad_2:
    munmap(addr, (size - 4096));
f14_bad_1:
    close(fd);
    free(buf);
f14_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-14] ng\n");
}

// file backed mmap when the valid address is provided by user
void file_given_addr_test() {
    printf("\n[F-15] 正しいアドレスを指定\n");

    write_test(200);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f15_bad_0;
    }

    char *addr = mmap((void *)(MMAPBASE + 0x1000), 200, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        goto f15_bad_1;
    }
    int ret = munmap(addr, 200);
    if (ret == -1) {
        printf("munmap failed\n");
        goto f15_bad_1;
    }

    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-15] ok\n");
    return;

f15_bad_1:
    close(fd);
f15_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-15] ng\n");
}

// file backed mmap when provided address is less than MMAPBASE
void file_invalid_addr_test(void) {
    printf("\n[F-16] 不正なアドレスを指定\n");
    write_test(200);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f16_bad_0;
    }

    char *addr = mmap((void *)(MMAPBASE - 0x1000), 200, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        goto f16_bad_1;
    }

    int ret = munmap(addr, 200);
    if (ret == -1) {
        printf("munmap failed\n");
        goto f16_bad_1;
    }

    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-16] ok\n");
    return;

f16_bad_1:
    close(fd);
f16_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-16] ng\n");
}

// [F-17] 指定されたアドレスが既存のマッピングアドレスと重なる場合
// test 1: アドレス指定(MMAPBASE + 0x1000)で8000バイトマッピング
// test 2: 1と重なるアドレス指定(MMAPBASE + 0x1000)で200バイトマッピング
void file_overlap_given_addr_test() {
    printf("\n[F-17] 指定アドレスがマッピングアドレスと重なる\n");
    write_test(8000);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f17_bad_0;
    }

    char *addr = mmap((void *)(MMAPBASE + 0x1000), 8000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        goto f17_bad_1;
    }
    //printf("- addr=%p\n", addr);

    char *addr2 = mmap((void *)(MMAPBASE + 0x1000), 200, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE |  MAP_POPULATE, fd, 0);
    if (addr2 == MAP_FAILED) {
        printf("mmap 2 failed");
        goto f17_bad_2;
    }

    if (addr2 == (void *)(MMAPBASE + 0x1000)) {
        printf("mmap 2 is bad mapping: addr1 = %p", addr2);
        goto f17_bad_3;
    }

    //printf("- addr2=%p\n", addr2);

    int ret = munmap(addr, 8000);
    if (ret == -1) {
        printf("munmap 1 failed\n");
        munmap(addr2, 200);
        goto f17_bad_1;
    }

    ret = munmap(addr2, 200);
    if (ret == -1) {
        printf("munmap 2 failed\n");
        goto f17_bad_1;
    }

    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-17] ok\n");
    return;

f17_bad_3:
    munmap(addr2, 200);
f17_bad_2:
    munmap(addr, 8000);
f17_bad_1:
    close(fd);
f17_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-17] ng\n");
}

// [F-18] ２つのマッピングの間のアドレスを指定したマッピングのテスト
// test 1: addr指定せずに1000バイトのマッピング
// test 2: addr指定(MMAPBASE + 0x3000)して200バイトのマッピング
// test 3: addr指定(MMAPBASE + 0x100)して1000バイトのマッピング
void file_intermediate_given_addr_test() {
    printf("\n[F-18] 既存のマッピング間のアドレスを指定したマッピング\n");
    write_test(1000);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f18_bad_0;
    }

    char *addr = mmap((void *)0, 1000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap 1 failed\n");
        goto f18_bad_1;
    }
    //printf("addr=%p\n", addr);
    char *addr2 = mmap((void *)(MMAPBASE + 0x3000), 200, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr2 == MAP_FAILED) {
        printf("mmap 2 failed\n");
        goto f18_bad_2;
    }

    //printf("addr2=%p\n", addr2);
    char *addr3 = mmap((void *)(MMAPBASE + 0x100), 1000, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_POPULATE, fd, 0);
    if (addr3 == MAP_FAILED) {
        printf("mmap 3 failed\n");
        goto f18_bad_3;
    }

    if (addr3 != addr + 0x1000) {
        printf("mapping faild: expect: %p, mapped: %p\n", addr + 0x1000, addr3);
        goto f18_bad_4;
    }

    //printf("addr3=%p\n", addr3);
    int ret = munmap(addr3, 1000);
    if (ret == -1) {
        printf("munmap addr3 failed\n");
        goto f18_bad_3;
    }

    ret = munmap(addr2, 200);
    if (ret == -1) {
        printf("munmap addr2 failed\n");
        goto f18_bad_2;
    }

    ret = munmap(addr, 1000);
    if (ret == -1) {
        printf("munmap addr failed\n");
        goto f18_bad_1;
    }

    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-18] ok\n");
    return;

f18_bad_4:
    munmap(addr3, 1000);
f18_bad_3:
    munmap(addr2, 200);
f18_bad_2:
    munmap(addr, 1000);
f18_bad_1:
    close(fd);
f18_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-18] ng\n");
}

// [F-19] ２つのマッピングの間に不可能なアドレスを指定した場合
// test 1: アドレス指定なしで1000バイトマッピング
// test 2: アドレス指定(MMAPBASE + 0x3000)して200バイトマッピング
// test 3: アドレス指定(MMAPBASE + 0x100)して10000バイトマッピング
void file_intermediate_given_addr_not_possible_test() {
    printf("\n[F-19] 既存のマッピング間に不可能なアドレスを指定\n");
    write_test(8192);

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f19_bad_0;
    }

    char *addr = mmap((void *)0, 1000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE, fd, 0);
    //printf("addr =%p\n", addr);
    if (addr == MAP_FAILED) {
        printf("mmap addr failed\n");
        goto f19_bad_1;
    }

    char *addr2 = mmap((void *)(MMAPBASE + 0x3000), 200, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_POPULATE, fd, 0);
    //printf("addr2=%p\n", addr2);
    if (addr2 == MAP_FAILED) {
        printf("mmap addr2 failed\n");
        goto f19_bad_2;
    }

    char *addr3 = mmap((void *)(MMAPBASE + 0x100), 10000, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_POPULATE, fd, 0);
    //printf("addr3=%p\n", addr3);
    if (addr3 == MAP_FAILED) {
        printf("mmap addr3 failed\n");
        goto f19_bad_3;
    }

    if (addr3 == (void *)(MMAPBASE + 0x100)) {
        printf("bad mapping\n");
        goto f19_bad_4;
    }

    //printf("addr=%p, addr2=%p, addr3=%p\n", addr, addr2, addr3);
    int ret = munmap(addr3, 10000);
    if (ret == -1) {
        printf("munmap addr3 failed\n");
        goto f19_bad_3;
    }

    ret = munmap(addr2, 200);
    if (ret == -1) {
        printf("munmap addr2 failed\n");
        goto f19_bad_2;
    }

    ret = munmap(addr, 1000);
    if (ret == -1) {
        printf("munmap addr failed\n");
        goto f19_bad_1;
    }

    close(fd);
    unlink(filename);
    file_ok++;
    printf("[F-19] ok\n");
    return;

f19_bad_4:
    munmap(addr3, 10000);
f19_bad_3:
    munmap(addr2, 200);
f19_bad_2:
    munmap(addr, 1000);
f19_bad_1:
    close(fd);
f19_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-19] ng\n");
}

// [F-20] 共有マッピングでファイル容量より大きなサイズを指定した場合
// test 1: マッピングは成功する
// test 2: 容量以上の書き込みをしても問題ない（容量以降の書き込み結果の比較はしていない）
void file_exceeds_file_size_test() {
    printf("\n[F-20] ファイル容量より大きな共有マッピング\n");
    // 2286バイトのファイルに8000バイトのマッピングを行い、その後、8000バイトまで書き込む
    int size = 8000;

    write_test(2286);

    char *buf = (char *)malloc(3000);
    if (buf == NULL) {
        printf("no memory for buf\n");
        goto f20_bad_0;
    }

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        printf("open failed\n");
        goto f20_bad_0;
    }

    int n = read(fd, buf, 2286);
    if (n != 2286) {
        printf("read failed\n");
        goto f20_bad_1;
    }


    char *addr = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n");
        goto f20_bad_1;
    }

    for (int i = 0; i < size; i++) {
        addr[i] = 'a';
    }

    if (my_strcmp(addr, buf, 2286) != 0) {
        printf("strcmp failed\n");
        goto f20_bad_2;
    }

    int res = munmap(addr, size);
    if (res == -1) {
        printf("munmap failed\n");
        goto f20_bad_1;
    }

    close(fd);
    unlink(filename);
    free(buf);
    file_ok++;
    printf("[F-20] ok\n");
    return;

f20_bad_2:
    munmap(addr, 1000);
f20_bad_1:
    close(fd);
    free(buf);
f20_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-20] ng\n");
}

// [F-21] write onlyファイルへのREAD/WRITE共有マッピングのテスト
// test 1: エラーになる
void file_mapping_on_wo_file_test() {
    printf("\n[F-21] write onlyファイルへのREAD/WRITE共有マッピング\n");
    int size = 2000;
    write_test(size);

    int fd = open(filename, O_WRONLY);
    if (fd == -1) {
        printf("open failed\n");
        goto f21_bad_0;
    }

    char *addr = (char *)mmap((void *)0, size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        unlink(filename);
        file_ok++;
        printf("[F-21] ok\n");
        return;
    }
    int ret = munmap((void *)addr, size);
    if (ret == -1) {
        printf("munmap failed\n");
    }

f21_bad_1:
    close(fd);
f21_bad_0:
    unlink(filename);
    file_ng++;
    printf("[F-21] ng\n");
}

// <!! ----------- Anonymous mappings test ---------------------- !!>

// Read/Write権限を持ち、サイズが2ページを超えるシンプルな
// anonymousプライベートマッピング
void anon_private_test() {
    printf("\n[A-01] anonymousプライベートマッピング\n");
    int size = 10000;

    int *p1 = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("mmap failed\n");
        goto a1_bad_0;
    }

    for (int i = 0; i < size / 4; i++) {
        p1[i] = i;
    }
    //printf("p[0]=%d, p[2499]=%d\n", p1[0], p1[2499]);
    int ret = munmap(p1, size);
    if (ret < 0) {
        printf("munmap failed\n");
        goto a1_bad_0;
    }

    anon_ok++;
    printf("[A-01] ok\n");
    return;

a1_bad_1:
    munmap(p1, size);
a1_bad_0:
    file_ng++;
    printf("[A-01] ng\n");
}

// Shared mapping test
void anon_shared_test() {
    printf("\n[A-02] anonymous共有マッピング\n");
    size_t size = 10000;
    int ret;

    int *p1 = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("mmap failed\n");
        goto a2_bad_0;
    }
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        goto a2_bad_1;
    } else if (pid == 0) {
        for (int i = 0; i < size / 4; i++) {
            p1[i] = i;
        }
        exit(0);
    } else {
        int status;
        wait(&status);
        ret = msync(p1, size, MS_ASYNC);
        if (ret < 0) {
            printf("failed msync\n");
            goto a2_bad_1;
        }
        for (int i = 0; i < size / 4; i++) {
            if (p1[i] != i) {
                printf("p1[%d]: %d != %d\n", i, p1[i], i);
                goto a2_bad_1;
            }
        }
        ret = munmap((void *)p1, size);
        if (ret < 0) {
            printf("munmap failed\n");
            goto a2_bad_0;
        }
        anon_ok++;
        printf("[A-02] ok\n");
        return;
    }

a2_bad_1:
    munmap(p1, size);
a2_bad_0:
    file_ng++;
    printf("[A-02] ng\n");
}

// Private mapping with fork
void anon_private_fork_test() {
    printf("\n[A-03] fork付きのanonymousプライベートマッピング\n");
    char temp[200];

    for (int i = 0; i < 200; i++) {
        temp[i] = 'a';
    }
    int size = 200;
    char *p1 = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);  // Shared mapping
    if (p1 == MAP_FAILED) {
        printf("mmap failed\n");
        goto a3_bad_0;
    }
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
        goto a3_bad_1;
    } else if (pid == 0) {
        for (int i = 0; i < size; i++) {
            p1[i] = 'a';
        }
        exit(0);
    } else {
        int status;
        wait(&status);
        if (my_strcmp(temp, p1, size) == 0) {
            printf("strcmp parent\n");
            goto a3_bad_1;
        }
        int res = munmap(p1, size);
        if (res == -1)
            goto a3_bad_0;

        anon_ok++;
        printf("[A-03] ok\n");
        return;
    }

a3_bad_1:
    munmap(p1, size);
a3_bad_0:
    file_ng++;
    printf("[A-03] ng\n");
}

// Shared mapping test with multiple forks
void anon_shared_multi_fork_test() {
    printf("\n[A-04] 多段fork付きのanonymous共有マッピング\n");
    int size = 1000;
    int status, excd = 0;

    char *p1 = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("mmap failed\n");
        goto a4_bad_0;
    }

    char data[1000];
    for (int i = 0; i < size; i++) {
        data[i] = 'r';
    }

    int pid1 = fork();
    if (pid1 == -1) {
        printf("fork 1 failed\n");
        goto a4_bad_1;
    } else if (pid1 == 0) {  // 1st fork Child Process
        for (int i = 0; i < size; i++) {
            p1[i] = 'r';
        }
        int pid2 = fork();
        if (pid2 == -1) {
            printf("fork 2 failed\n");
            munmap(p1, size);
            excd = 1;
            //exit(1);
        } else if (pid2 == 0) {  // 2nd fork Child Process
            if (my_strcmp(data, p1, size) != 0) {
                printf("strcmp fork 2 failed\n");
                excd = 1;
                //exit(2);
            }
#if 0
            int pid3 = fork();
            if (pid3 == -1) {
                printf("fork 3 failed\n");
                munmap((void *)p1, size);
                excd = 1;
                //exit(3);
            } else if (pid3 == 0) {  // 3rd fork Child Process
                if (my_strcmp(data, p1, size) != 0) {
                    printf("strcmp fork 3 failed\n");
                    excd = 1;
                    //exit(4);
                } else {
                    excd = 6;
                }
                exit(excd);
            } else {  // 3rd fork Parent Process
                printf("pid3=%d\n", pid3);
                int status3;
                //waitpid(pid3, &status3, 0);
                wait(&status3);
                printf("[A-04] pid[6] status3=0x%x\n", status3);
                fflush(stdout);
                if ((status3 >> 8) != 6) {
                    printf("fork 3 parent: status=%d\n", status3);
                    excd = 1;
                    //exit(4);
                }
                if (my_strcmp(data, p1, size) != 0) {
                    printf("strcmp fork 3 parent failed\n");
                    excd = 1;
                    //exit(5);
                } else {
                    excd = 5;
                }
            }
            exit(excd);
#endif
        } else {  // 2nd fork Parent Process
            printf("pid2=%d\n", pid2);
            int status2;
            //waitpid(pid2, &status2, 0);
            wait(&status2);
            printf("[A-04] pid[5] status2=0x%x\n", status2);
            fflush(stdout);
            if ((status2 >> 8) != 5) {
                printf("fork 2 parent: status2=%d\n", status2);
                excd = 1;
                //exit(6);
            }
            if (my_strcmp(data, p1, size) != 0) {
                printf("strcmp fork 2 parent failed\n");
                excd = 1;
                //exit(7);
            } else {
                excd = 4;
            }
        }
        exit(excd);
    } else {  // 1st fork Parent process
        printf("pid1=%d\n", pid1);
        int status1;
        //waitpid(pid1, &status1, 0);
        wait(&status1);
        printf("[A-04] pid[4] status1=0x%x\n", status1);
        fflush(stdout);
        if ((status1 >> 8) != 4) {
            printf("fork 1 parent: status=%d\n", status1);
            goto a4_bad_1;
        }
        if (my_strcmp(data, p1, size) != 0) {
            printf("strcmp fork 1 parent failed\n");
            goto a4_bad_1;
        }
        int res = munmap(p1, size);
        if (res == -1) {
            printf("munmap failed\n");
            goto a4_bad_0;
        }

        anon_ok++;
        printf("[A-04] ok\n");
        return;
    }

a4_bad_1:
    munmap(p1, size);
a4_bad_0:
    file_ng++;
    printf("[A-04] ng\n");
}

// fork syscall with anonymous mapping test
void anon_private_shared_fork_test() {
    printf("\n[A-05] fork付きのanonymousプライベートとanonymous共用マッピングの共存\n");
    int size = 200;
    char data1[200], data2[200];

    for (int i = 0; i < size; i++) {
        data1[i] = 'a';
        data2[i] = 'r';
    }
    char *p1 = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);  // Private mapping
    if (p1 == MAP_FAILED) {
        printf("anonymous mmap failed\n");
        goto a5_bad_0;
    }
    char *p2 = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);  // Shared mapping
    if (p2 == MAP_FAILED) {
        printf("shared mmap failed\n");
        goto a5_bad_1;
    }

    int pid = fork();
    if (pid == -1) {
        printf("fork failed\n");
        goto a5_bad_2;
    } else if (pid == 0) {
        for (int i = 0; i < size; i++) {
            p1[i] = 'a';
        }
        for (int i = 0; i < size; i++) {
            p2[i] = 'r';
        }
        exit(0);
    } else {
        int status;
        wait(&status);
        // Private mapping
        if (my_strcmp(p1, data1, size) == 0) {
            printf("strcmp private failed\n");
            goto a5_bad_2;
        }
        // Shared mapping
        if (my_strcmp(p2, data2, size) != 0) {
            printf("strcmp share failed\n");
            goto a5_bad_2;
        }
        int ret = munmap(p2, size);
        if (ret < 0) {
            printf("munmap p2 failed\n");
            goto a5_bad_1;
        }
        ret = munmap(p1, size);
        if (ret < 0) {
            printf("munmpa p1 failed\n");
            goto a5_bad_0;
        }
        anon_ok++;
        printf("[A-05] ok\n");
        return;
    }

a5_bad_2:
    munmap(p2, size);
a5_bad_1:
    munmap(p1, size);
a5_bad_0:
    file_ng++;
    printf("[A-05] ng\n");
}

// Missing flags Test: Missing MAP_PRIVATE or MAP_SHARED in flags
void anon_missing_flags_test(void) {
    printf("\n[A-06] 不正なflagsのanonymousマッピング\n");
    int size = 10000;
    // Missing MAP_PRIVATE or MAP_SHARED flag
    int *p1 = (int *)mmap((void *)0, size, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS, -1, 0);
    if (p1 != MAP_FAILED) {
        printf("mmap worked out\n");
        munmap(p1, size);
        anon_ng++;
        printf("[A-06] ng\n");
        return;
    }
    anon_ok++;
    printf("[A-06] ok\n");
}

// anonymous mapping count test when it exceeds 32
void anon_exceed_count_test(void) {
    printf("\n[A-07] anonymousマッピングの回数超過\n");
    int size = 5096;
    int count = 50;
    char *arr[50];
    int i = 0;
    int ret;

    for (; i < count; i++) {
        char *p1 = mmap((void *)0, size, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
        arr[i] = p1;
        //printf("arr[%d]=%p\n", i, arr[i]);
        if (p1 == MAP_FAILED) {
            break;
        }
    }
    // 50回のmmapに成功した場合
    if (i == 50) {
        int failed = 0;
        for (int j = 0; j < i; j++) {
            ret = munmap(arr[j], size);
            if (ret < 0) {
                printf("munmap arr[%d]=%p failed\n", j, arr[j]);
                failed++;
            }
        }
        if (failed > 0)
            goto a7_bad_0;
        printf("[A-07] ok\n");
        return;
    }

    // mmapに失敗
    printf("mmap failed at %d count\n", i);
    for (int j = 0; j < i; j++) {
        ret = munmap(arr[j], size);
        if (ret < 0) {
            printf("munmap arr[%d]=%p failed\n", j, arr[j]);
        }
    }

a7_bad_0:
    anon_ng++;
    printf("[A-07] ng\n");
}

// anonymous mapping when size exceeds KERNBASE
void anon_exceed_size_test(void) {
    printf("\n[A-08] anonymous exceed mapping size test\n");
    unsigned long size = (1UL << 48);  // USERTOP
    char *p1 = (char *)mmap((void *)0, size, PROT_READ | PROT_WRITE,
                             MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    if (p1 != MAP_FAILED) {
        printf("[A-08] failed at mmap: p1=%p\n", p1);
        munmap((void *)p1, size);
        anon_ng++;
        return;
    }
    printf("[A-08] ok\n");
    anon_ok++;
}

// mmap when provided size is 0
void anon_zero_size_test(void) {
    printf("\n[A-09] anonymous zero size mapping test\n");
    char *p1 = (char *)mmap((void *)0, 0, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p1 != MAP_FAILED) {
        printf("[A-09] failed\n");
        munmap((void *)p1, 0);
        anon_ng++;
        return;
    }
    printf("[A-09] ok\n");
    anon_ok++;
}

// mmap when the valid address is provided by user
void anon_given_addr_test() {
    printf("\n[A-10] anonymous valid provided address test\n");
    char *p1 = (char *)mmap((void *)(MMAPBASE + 0x1000), 200, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("[A-10] failed: at mmap\n");
        anon_ng++;
        return;
    }
    int res = munmap((void *)p1, 200);
    if (res == -1) {
        printf("[A-10] failed at munmap\n");
        anon_ng++;
        return;
    }
    printf("[A-10] ok\n");
    anon_ok++;
}

// mmap when provided address is less than MMAPBASE
// このテストは無効: MMAPBASE以下のアドレスもmmap可能にしたため
void anon_invalid_addr_test(void) {
    printf("\n[A-11] anonymous invalid provided address test\n");
/*
    char *p1 = (char *)mmap((void *)0x50001000, 200, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p1 != MAP_FAILED) {
        printf("[A-11] failed at mmap\n");
        munmap((void *)p1, 200);
        anon_ng++;
        return;
    }
*/
    printf("[A-11] ok: not running because of an invalid test\n");
    anon_ok++;
}

// test when the address is provided by user and it overlaps with existing
// address
void anon_overlap_given_addr_test() {
    long error;

    printf("\n[A-12] anonymous overlapping provided address test\n");
    char *p1 = (char *)mmap((void *)(MMAPBASE + 0x1000), 10000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("[A-12] failed: at first mmap\n");
        anon_ng++;
        return;
    }
    char *p2 = (char *)mmap((void *)(MMAPBASE + 0x1000), 200, PROT_READ | PROT_WRITE ,
                              MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p2 == MAP_FAILED || p2 == (void *)(MMAPBASE + 0x1000)) {
        printf("[A-12] failed at second mmap\n");
        munmap((void *)p1, 10000);
        anon_ng++;
        return;
    }
    error = munmap((void *)p1, 10000);
    if (error < 0) {
        printf("[A-12] failed at first munmap\n");
        anon_ng++;
        munmap((void *)p2, 200);
        return;
    }
    error = munmap((void *)p2, 200);
    if (error < 0) {
        printf("[A-12] failed: at first munmap\n");
        anon_ng++;
        return;
    }
    printf("[A-12] ok\n");
    anon_ok++;
}

// test when the mapping is possible between two mappings
void anon_intermediate_given_addr_test() {
    int ret;

    printf("\n[A-13] 中間のアドレスを指定したanonymousマッピング\n");
    char *p1 = (char *)mmap((void *)0, 1000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("mmap p1 failed\n");
        goto a13_bad_0;
    }
    printf("p1=%p\n", p1);

    char *p2 = (char *)mmap((void *)(MMAPBASE + 0x3000), 200, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);

    if (p2 == MAP_FAILED) {
        printf("mmap p2 failed\n");
        goto a13_bad_1;
    }
    printf("p2=%p\n", p2);

    char *p3 = (char *)mmap((void *)(MMAPBASE + 0x100), 1000, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p3 == MAP_FAILED) {
        printf("mmap p3 failed\n");
        goto a13_bad_2;
    }
    printf("p3=%p\n", p3);

    if (p3 != (p1 + 0x1000)) {
        printf("invalid p3: expect = %p, mapped = %p\n", (void *)(MMAPBASE + 0x1000), p3);
        goto a13_bad_3;
    }

    ret = munmap((void *)p3, 1000);
    if (ret < 0) {
        printf("munmap p3 failed\n");
        goto a13_bad_2;
    }

    ret = munmap((void *)p2, 200);
    if (ret < 0) {
        printf("munmap p2 failed\n");
        goto a13_bad_1;
    }

    ret = munmap((void *)p1, 1000);
    if (ret < 0) {
        printf("munmap p1 failed\n");
        goto a13_bad_0;
    }

    anon_ok++;
    printf("[A-13] ok\n");
    return;

a13_bad_3:
    munmap(p3, 1000);
a13_bad_2:
    munmap(p2, 200);
a13_bad_1:
    munmap(p1, 1000);
a13_bad_0:
    anon_ng++;
    printf("[A-05] ng\n");
}

// mmap when the mapping is not possible between two mappings
void anon_intermediate_given_addr_not_possible_test() {
    printf(
        "\n[A-14] anonymous intermediate provided address not possible test\n");
    char *p1 = (char *)mmap((void *)0, 1000, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("[A-14] failed at first mmap\n");
        anon_ng++;
        return;
    }
    char *p2 = (char *)mmap((void *)(MMAPBASE + 0x3000), 200, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p2 == MAP_FAILED) {
        printf("[A-14] failed at second mmap\n");
        anon_ng++;
        munmap((void *)p1, 1000);
        return;
    }
    char *p3 = (char *)mmap((void *)(MMAPBASE + 0x100), 10000, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p3 == MAP_FAILED) {
        printf("[A-14] failed at second mmap\n");
        anon_ng++;
        munmap((void *)p1, 1000);
        munmap((void *)p2, 200);
        return;
    }
    if (p3 == (void *)(MMAPBASE + 0x100)) {
        printf("[A-14] failed at third mmap\n");
        anon_ng++;
        munmap((void *)p1, 1000);
        munmap((void *)p2, 200);
        return;
    }
    munmap((void *)p1, 1000);
    munmap((void *)p2, 200);
    munmap((void *)p3, 10000);
    printf("[A-14] ok\n");
    anon_ok++;
}

// <!! ------------------Other MMAP Tests ------------- !!>
// Munmap only partial size of the mapping test
void munmap_partial_size_test() {
    printf("\n[O-01] munmap only partial size test\n");
    int size = 10000;
    long error;

    int *p1 = (int *)mmap((void *)0, size, PROT_READ | PROT_WRITE,
                           MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("[O-01] failed at mmap\n");
        other_ng++;
        return;
    }
    for (int i = 0; i < size / 4; i++) {
        p1[i] = i;
    }
    // Munmap only first page
    error = munmap((void *)p1, 5);
    if (error < 0) {
        printf("[O-01] failed at munmap\n");
        other_ng++;
        munmap((void *)p1, size);
        return;
    }
    // Check if next page is still accessible or not
    for (int i = 1024; i < size / 4; i++) {
        p1[i] = i * 2;
    }
    error = munmap((void *)p1 + 4096, size);
    if (error < 0) {
        printf("[O-01] failed at munmap 2\n");
        other_ng++;
        return;
    }
    printf("[O-01] ok\n");
    other_ok++;
}

// When there is only read permission on mapping but user tries to write
void mmap_write_on_ro_mapping_test() {
    printf("\n[O-02] write on read only mapping test\n");
    int status;

    int pid = fork();
    if (pid == -1) {
        printf("[O-02] failed at fork\n");
        other_ng++;
        return;
    }
    if (pid == 0) {
        int size = 10000;
        int *addr = (int *)mmap((void *)0, size, PROT_READ,
                               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (addr == MAP_FAILED) {
            printf("[O-02] failed at mmap child\n");
            other_ng++;
            exit(1);
        }
        /*  ここで例外発生
            for (int i = 0; i < size / 4; i++) {
              addr[i] = i;
            }
            // If the memory access allowed then test should failed
            printf("write on read only mapping test failed 3\n");
            other_ng++;
        */
        munmap(addr, size);
        exit(0);
    } else {
        wait(&status);
        printf("[O-02] ok\n");
        other_ok++;
    }
}

// None permission on mapping test
void mmap_none_permission_test() {
    printf("\n[O-03] none permission on mapping test\n");
    int status;

    int pid = fork();
    if (pid == -1) {
        printf("[O-03] failed at fork\n");
        other_ok++;
        return;
    }
    if (pid == 0) {
        int size = 10000;
        char *p1 = (char *)mmap((void *)(MMAPBASE + 0x3000), size, PROT_NONE,
                                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (p1 == MAP_FAILED) {
            printf("[O-03] failed mmap\n");
            other_ng++;
            exit(1);
        }
        /*
            printf("p1=%p\n", p1);

            for (int i = 0; i < size / 4; i++) {
              p1[i] = i;
            }
            // If the memory access allowed then test should failed
            printf("none permission on mapping test failed 3\n");
            other_ng++;
        */
        munmap((void *)p1, size);
        exit(0);
    } else {
        wait(&status);
        printf("[O-03] ok\n");
        other_ok++;
    }
}

// To test MAP_FIXED flag with valid address
void mmap_valid_map_fixed_test() {
    printf("\n[O-04] mmap valid address map fixed flag test\n");
    long error;

    char *p1 = mmap((void *)(MMAPBASE + 0x1000), 200, PROT_WRITE | PROT_READ,
                     MAP_FIXED | MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    if (p1 == MAP_FAILED) {
        printf("[O-04] failed at mmap\n");
        other_ng++;
        return;
    }
    error = munmap((void *)p1, 200);
    if (error < 0) {
        printf("[O-04] failed at munmap\n");
        other_ng++;
        return;
    }
    printf("[O-04] ok\n");
    other_ok++;
}

// To test MAP_FIXED flag with invalid addresses
void mmap_invalid_map_fixed_test() {
    printf("\n[O-05] mmap invalid address map fixed flag test\n");
    // When the address is less than MMAPBASE（これは現在は無効）
/*
    char *p1 = mmap((void *)(MMAPBASE - 0x2000), 200, PROT_WRITE | PROT_READ,
                     MAP_FIXED | MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    printf("p1=%p\n", p1);
    if (p1 != MAP_FAILED) {
        printf("[O-05] failed at mmap 1\n");
        munmap((void *)p1, 200);
        other_ng++;
        return;
    }
*/
    // When the address is not page aligned
    char *p2 = mmap((void *)(MMAPBASE + 0x100), 200, PROT_WRITE | PROT_READ,
                      MAP_FIXED | MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    //printf("p2=%p\n", p2);
    if (p2 != MAP_FAILED) {
        printf("[O-05] failed at mmap 2\n");
        other_ng++;
        return;
    }
    // Mapping is not possible because other mapping already exists at provided
    // address (MMAPBASEはdash)
    char *p3 = mmap((void *)MMAPBASE, 200, PROT_WRITE | PROT_READ,
                      MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    //printf("p3=%p\n", p3);
    if (p3 == MAP_FAILED) {
        printf("[O-05] failed mmap 3\n");
        other_ng++;
        return;
    }
    char *p4 = mmap(p3, 200, PROT_WRITE | PROT_READ,
                      MAP_FIXED | MAP_PRIVATE | MAP_POPULATE | MAP_ANONYMOUS, -1, 0);
    //printf("p4=%p\n", p4);
    if (p4 != MAP_FAILED) {
        printf("[O-05] failed mmap 4\n");
        munmap((void *)p3, 200);
        other_ng++;
        return;
    }
    munmap((void *)p3, 200);
    printf("[O-05] test ok\n");
    other_ok++;
}
