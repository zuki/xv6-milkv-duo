#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PGSIZE    4096

void mmap_test();
void fork_test();
char buf[4096];

int
main(int argc, char *argv[])
{
        mmap_test();
        fork_test();
        printf("mmaptest: all tests succeeded\n");
        return 0;
}

char *testname = "???";

void
err(char *why)
{
        printf("mmaptest: %s failed: %s, pid=%d\n", testname, why, getpid());
        //exit(1);
}

//
// check the content of the two mapped pages.
// 1.5ページ分は'A'で残りの0.5ページは'\0'
//
int _v1(char *p)
{
    int i;
    char c;
    int ret = 0;
    for (i = 0; i < 8192; i++) {
        c = p[i];
        if (i < 6144) {
            if (c != 'A') {
                printf("mismatch at %d, wanted 'A', got 0x%x, addr=%p\n", i, p[i], p + i);
                ret--;
                break;
            }
        } else {
            if (c != 0) {
                printf("mismatch at %d, wanted zero, got 0x%x, addr=%p\n", i, p[i], p + i);
                ret--;
                break;
            }
        }
    }
    return ret;
}

//
// マップされるファイルを作成する
// 1.5ページは'A'、残りの半ページはゼロ
//
int
makefile(const char *f)
{
    int i;
    //int n = PGSIZE / BSIZE;

    unlink(f);
    int fd = open(f, O_WRONLY | O_CREAT, 0666);
    if (fd == -1) {
        err("makefile: open");
        return -1;
    }
    memset(buf, 'A', 4096);
    // write 1.5 page
    if (write(fd, buf, 4096) != 4096) {
        err("makefile: write 0 makefile 1");
        close(fd);
        return -1;
    }
    memset(buf+2048, 0, 2048);
    if (write(fd, buf, 4096) != 4096) {
        err("makefile: write 0 makefile 2");
        close(fd);
        return -1;
    }

    if (close(fd) == -1) {
        err("makefile: close");
        return -1;
    }

    return 0;
}

void
mmap_test(void)
{
    int fd;
    int i;
    int ret, error = 0;
    int ok, ng, total;
    char *p;
    ok = ng = total = 0;
    const char * const f = "mmap.dur";
    printf("mmap_testスタート\n");
    testname = "mmap_test";

    //
    // 1: 既知のコンテンツを持つファイルを作成
    // 2: それをメモリにマッピング
    // 3: マップトメモリの内容がファイルと一致するかテスト
    //
    if (makefile(f) < 0) {
        printf("%s: ng\n", testname);
        return;
    }

    if ((fd = open(f, O_RDONLY)) == -1) {
        err("open");
        return;
    }

    printf("- open mmap.dur (fd=%d) READ ONLY\n", fd);
    printf("[1] test mmap f\n");
    //
    // このmmap()の呼び出しは、ファイルfdの内容をアドレス空間に
    // マップするようカーネルに依頼する。
    //
    // 第1引数の0は、仮想アドレスをカーネルに選択させることを示す。
    // 第2引数は、マップするバイト数を示す。
    // 第3引数は、マップドメモリはRead-onlyであることを示す。
    // 第4引数は、プロセスがこのマップドメモリを変更しても
    //    変更をファイルに書き戻さないこと、および、変更を
    //    同じファイルをマッピングしている他のプロセスと共有しない
    //    ことを示す（もちろん、PROT_READのために変更は許されていない）。
    // 第5引数は、マップされるファイルのファイルディスクリプタ。
    // 第6引数は、ファイルないのマッピング開始オフセット。
    //
    p = mmap((void *)0, PGSIZE * 2, PROT_READ, MAP_PRIVATE, fd, 0);
    printf("- mmap fd=%d -> 2ページ分PROT_READ MAP_PRIVATEでマップ: addr=%p\n", fd, p);
    if (p == MAP_FAILED) {
        err("mmap (1)");
        printf("test mmap f: NG\n");
        error = 1;
        goto test1;
    }

    if ((ret = _v1(p)) < 0) {
        printf("- ret: %d\n", ret);
        munmap(p, PGSIZE * 2);
        error = 1;
        goto test1;
    }

    if (munmap(p, PGSIZE*2) < 0) {
        err("munmap (1)");
        error = 1;
        goto test1;
    }

    printf("- munmmap 2PAGE: addr=%p\n", p);

test1:
    if (error) {
        ng++;
        printf("[1] failed\n");
    } else {
        ok++;
        printf("[1] OK\n");
    }
    total++;
    error = 0;

    printf("[2] test mmap private\n");
    // Read-onlyでオープン済みのファイルをプライベートWritableな
    // マッピングでマップできるはず
    p = mmap((void *)0, PGSIZE*2, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    printf("- mmap fd=%d -> 2ページ分をPAGE PROT_READ/WRITE, MAP_PRIVATEでマップ: addr=%p\n", fd, p);
    if (p == MAP_FAILED) {
        err("mmap (2)");
        close(fd);
        error = 1;
        goto test2;
    }

    if (close(fd) == -1) {
        err("close");
        munmap(p, PGSIZE*2);
        error = 1;
        goto test2;
    }
    printf("- closed %d\n", fd);
    if ((ret = _v1(p)) < 0) {
        printf("- ret: %d\n", ret);
        munmap(p, PGSIZE*2);
        error = 1;
        goto test2;
    }
    printf("- write 2PAGE = Z\n");
    for (i = 0; i < PGSIZE*2; i++)
        p[i] = 'Z';
    printf("- p[1]=%c\n", p[1]);
    if (munmap(p, PGSIZE*2) < 0) {
         err("munmap (2)");
         error = 1;
        goto test2;
    }

    printf("- munmmap 2PAGE: addr=%p\n", p);

test2:
    if (error) {
        ng++;
        printf("[2] failed\n");
    } else {
        ok++;
        printf("[2] OK\n");
    }
    total++;
    error = 0;

    printf("[3] test mmap read-only\n");
    // check that mmap doesn't allow read/write mapping of a
    // file opened read-only with MAP_SHARED.
    if ((fd = open(f, O_RDONLY)) == -1) {
        err("open");
        error = 1;
        goto test3;
    }
    printf("- open mmap.dur (%d) RDONALY\n", fd);
    p = mmap((void *)0, PGSIZE*3, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("- mmap %d -> 3PAGE PROT_READ/WRITE MAP_SHARED: addr=%p\n", fd, p);
    if (p != MAP_FAILED) {
        err("mmap call should have failed");
        close(fd);
        munmap(p, PGSIZE*3);
        error = 1;
        goto test3;
    }
    if (close(fd) == -1) {
        err("close");
        error = 1;
        goto test3;
    }
    printf("- close %d\n", fd);
test3:
    if (error) {
        ng++;
        printf("[3] failed\n");
    } else {
        ok++;
        printf("[3] OK\n");
    }
    total++;
    error = 0;

    printf("[4] test mmap read/write\n");
    // check that mmap does allow read/write mapping of a
    // file opened read/write.
    if ((fd = open(f, O_RDWR)) == -1) {
        err("open");
        error = 1;
        goto test4;
    }
    printf("- open mmap.dur (%d) RDWR\n", fd);
    p = mmap((void *)0, PGSIZE*3, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("- mmap %d -> 3PAGE PROT_READ|WRITE MAP_SHARED: addr=%p\n", fd, p);
    if (p == MAP_FAILED) {
        err("mmap (4)");
        close(fd);
        error = 1;
        goto test4;
    }
    if (close(fd) == -1) {
        err("close");
        munmap(p, PGSIZE*2);
        error = 1;
        goto test4;
    }
    printf("- close %d\n", fd);

    // check that the mapping still works after close(fd).
    if ((ret = _v1(p)) < 0) {
        printf("- ret: %d\n", ret);
        munmap(p, PGSIZE*2);
        error = 1;
        goto test4;
    }
    // write the mapped memory.
    printf("- print 2PAGE = Z\n");
    for (i = 0; i < PGSIZE*2; i++)
        p[i] = 'Z';

    // unmap just the first two of three pages of mapped memory.
    if (munmap(p, PGSIZE*2) < 0) {
        err("munmap (4)");
        error = 1;
        goto test4;
    }
    printf("- munmmap 2PAGE: addr=%p\n", p);

test4:
    if (error) {
        ng++;
        printf("[4] failed\n");
    } else {
        ok++;
        printf("[4] OK\n");
    }
    total++;
    error = 0;

    printf("[5] test mmap dirty\n");

    // check that the writes to the mapped memory were
    // written to the file.
    if ((fd = open(f, O_RDWR)) == -1) {
        err("open");
        error = 1;
        goto test5;
    }
    printf("- open mmap.dur (%d) RDWR\n", fd);
    for (i = 0; i < PGSIZE + (PGSIZE/2); i++){
        char b;
        if (read(fd, &b, 1) != 1) {
            err("read (5)");
            error = 1;
            close(fd);
            goto test5;
        }

        if (b != 'Z') {
            printf("expect Z, but got %c\n", b);
            error = 1;
            close(fd);
            goto test5;
        }
    }
    if (close(fd) == -1) {
        err("close");
        error = 1;
        close(fd);
        goto test5;
    }
    printf("- close %d\n", fd);

test5:
    if (error) {
        ng++;
        printf("[5] failed\n");
    } else {
        ok++;
        printf("[5] OK\n");
    }
    total++;
    error = 0;

    printf("[6] test not-mapped unmap\n");
    // unmap the rest of the mapped memory.
    if (munmap(p+PGSIZE*2, PGSIZE) == -1) {
        err("munmap (6)");
        error = 1;
        close(fd);
        goto test6;
    }
    printf("- munmmap PAGE: addr=%p\n", p+PGSIZE*2);

test6:
    if (error) {
        ng++;
        printf("[6] failed\n");
    } else {
        ok++;
        printf("[6] OK\n");
    }
    total++;
    error = 0;

    printf("[7] test mmap two files\n");
    //
    // mmap two files at the same time.
    //
    int fd1;
    if((fd1 = open("mmap1", O_RDWR|O_CREAT, 0666)) < 0) {
        err("open mmap1");
        error = 1;
        goto test7;
    }

    printf("- open mmap1 (%d) RDWR/CREAT\n", fd1);
    if (write(fd1, "12345", 5) != 5) {
        err("write mmap1");
        error = 1;
        close(fd1);
        goto test7;
    }
    printf("- write %d: 12345\n", fd1);
    char *p1 = mmap((void *)0, PGSIZE, PROT_READ, MAP_PRIVATE, fd1, 0);
    printf("- mmap %d -> PAGE PROT_READ MAP_PRIVATE: addr=%p\n", fd1, p1);
    if(p1 == MAP_FAILED) {
        err("mmap mmap1");
        error = 1;
        close(fd1);
        goto test7;
    }

    close(fd1);
    printf("- close %d\n", fd1);
    unlink("mmap1");
    printf("- unlink mmap1\n");

    int fd2;
    if((fd2 = open("mmap2", O_RDWR|O_CREAT, 0666)) < 0) {
        err("open mmap2");
        error = 1;
        munmap(p1, PGSIZE);
        close(fd1);
        goto test7;
    }
    printf("- open mmap2 (%d) RDWR/CREAT\n", fd2);
    if(write(fd2, "67890", 5) != 5) {
        err("write mmap2");
        error = 1;
        munmap(p1, PGSIZE);
        close(fd2);
        goto test7;
    }
    printf("- write %d: 67890\n", fd2);
    char *p2 = mmap((void *)0, PGSIZE, PROT_READ, MAP_PRIVATE, fd2, 0);
    printf("- mmap %d -> PAGE PROT_READ MAP_PRIVATE: addr=%p\n", fd2, p2);
    if(p2 == MAP_FAILED) {
        err("mmap mmap2");
        error = 1;
        munmap(p1, PGSIZE);
        close(fd2);
        goto test7;
    }

    close(fd2);
    printf("- close %d\n", fd1);
    unlink("mmap2");
    printf("- unlink mmap2\n");

    int result;
    if((result = memcmp(p1, "12345", 5)) != 0) {
        printf("mmap1 mismatch: p1=%s, result=%d\n", p1, result);
        //err("mmap1 mismatch");
        error = 1;
        munmap(p1, PGSIZE);
        munmap(p2, PGSIZE);
        goto test7;
    }

    if((result = memcmp(p2, "67890", 5)) != 0) {
        printf("mmap2 mismatch: p2=%s, result=%d\n", p2, result);
        //err("mmap2 mismatch");
        error = 1;
        munmap(p1, PGSIZE);
        munmap(p2, PGSIZE);
        goto test7;
    }

    munmap(p1, PGSIZE);
    printf("- munmap PAGE: addr=%p\n", p1);
    if((result = memcmp(p2, "67890", 5)) != 0) {
        printf("mmap2 mismatch (2): p2=%s, result=%d\n", p2, result);
        //err("mmap2 mismatch (2)");
        error = 1;
        goto test7;
    }
    munmap(p2, PGSIZE);
    printf("- munmap PAGE: addr=%p\n", p2);
test7:
    if (error) {
        ng++;
        printf("[7] failed\n");
    } else {
        ok++;
        printf("[7] OK\n");
    }
    total++;

    printf("mmap_test: Total: %d, OK: %d, NG: %d\n", total, ok, ng);
    if (ng > 0)
        exit(1);
}

//
// mmap a file, then fork.
// check that the child sees the mapped file.
//
void
fork_test(void)
{
    int fd;
    int pid;
    int ret;
    const char * const f = "mmap.dur";

    printf("\nfork_test starting\n");
    testname = "fork_test";

    // mmap the file twice.
    makefile(f);
    if ((fd = open(f, O_RDONLY)) == -1)
        err("open");
    ret = unlink(f);
    if (ret < 0) {
        err("unlink");
    }
    char *p1 = mmap(0, PGSIZE*2, PROT_READ, MAP_SHARED, fd, 0);
    if (p1 == MAP_FAILED)
        err("mmap p1");
    char *p2 = mmap(0, PGSIZE*2, PROT_READ, MAP_SHARED, fd, 0);
    if (p2 == MAP_FAILED)
        err("mmap p2");

    // read just 2nd page.
    printf("p1: %p [%04d]=%c\n", p1, p1, *(p1));
    printf("p1:    [%04d]=%c\n", p1+PGSIZE, *(p1+PGSIZE));
    printf("p2: %p [%04d]=%c\n", p2, p2, *(p2));
    printf("p2:    [%04d]=%c\n", p2+PGSIZE, *(p2+PGSIZE));

    if (*(p1+PGSIZE) != 'A')
        err("fork mismatch (1)");

    if ((pid = fork()) < 0) {
        err("fork");
        return;
    }
    if (pid == 0) {
        if ((ret = _v1(p1)) < 0) {
            printf("- fork child v1(p1): ret: %d\n", ret);
        } else {
            printf("- fork child v1(p1) ok\n");
        }
        munmap((void *)p1, PGSIZE); // just the first page
        exit(0); // tell the parent that the mapping looks OK.
    }

    int status;
    waitpid(pid, &status, 0);

    if (status != 0){
        printf("fork_test failed, status=%d\n", status);
        munmap((void *)p1, 2*PGSIZE);
        munmap((void *)p2, 2*PGSIZE);
        exit(1);
    }

    // check that the parent's mappings are still there.
    if ((ret = _v1(p1)) < 0) {
        printf("- fork parent v1(p1): ret: %d\n", ret);
    } else {
        printf("- fork parent v1(p1) ok\n");
    }
    if ((ret = _v1(p2)) < 0) {
        printf("- fork parent v1(p2): ret: %d\n", ret);
    } else {
        printf("- fork parent v1(p2) ok\n");
    }
    munmap((void *)p1, 2*PGSIZE);
    munmap((void *)p2, 2*PGSIZE);
    printf("fork_test OK\n");
}
