#ifndef MKFS_FILES_H
#define MKFS_FILES_H

char *usrbins[] = {
    "usr/bin/b2sum",
    "usr/bin/base32",
    "usr/bin/base64",
    "usr/bin/basename",
    "usr/bin/basenc",
    "usr/bin/cat",
    "usr/bin/chcon",
    "usr/bin/chgrp",
    "usr/bin/chmod",
    "usr/bin/chown",
    "usr/bin/chroot",
    "usr/bin/cksum",
    "usr/bin/comm",
    "usr/bin/cp",
    "usr/bin/csplit",
    "usr/bin/cut",
    "usr/bin/date",
    "usr/bin/dd",
    "usr/bin/df",
    "usr/bin/dir",
    "usr/bin/dircolors",
    "usr/bin/dirname",
    "usr/bin/du",
    "usr/bin/echo",
    "usr/bin/env",
    "usr/bin/expand",
    "usr/bin/expr",
    "usr/bin/factor",
    "usr/bin/false",
    "usr/bin/fmt",
    "usr/bin/fold",
    "usr/bin/groups",
    "usr/bin/head",
    "usr/bin/hostid",
    "usr/bin/id",
    "usr/bin/install",
    "usr/bin/join",
    "usr/bin/kill",
    "usr/bin/link",
    "usr/bin/ln",
    "usr/bin/logname",
    "usr/bin/ls",
    "usr/bin/md5sum",
    "usr/bin/mkdir",
    "usr/bin/mkfifo",
    "usr/bin/mknod",
    "usr/bin/mktemp",
    "usr/bin/mv",
    "usr/bin/nice",
    "usr/bin/nl",
    "usr/bin/nohup",
    "usr/bin/nproc",
    "usr/bin/numfmt",
    "usr/bin/od",
    "usr/bin/paste",
    "usr/bin/pathchk",
    "usr/bin/pinky",
    "usr/bin/pr",
    "usr/bin/printenv",
    "usr/bin/printf",
    "usr/bin/ptx",
    "usr/bin/pwd",
    "usr/bin/readlink",
    "usr/bin/realpath",
    "usr/bin/rm",
    "usr/bin/rmdir",
    "usr/bin/runcon",
    "usr/bin/seq",
    "usr/bin/sha1sum",
    "usr/bin/sha224sum",
    "usr/bin/sha256sum",
    "usr/bin/sha384sum",
    "usr/bin/sha512sum",
    "usr/bin/shred",
    "usr/bin/shuf",
    "usr/bin/sleep",
    "usr/bin/sort",
    "usr/bin/split",
    "usr/bin/stat",
    "usr/bin/stty",
    "usr/bin/sum",
    "usr/bin/sync",
    "usr/bin/tac",
    "usr/bin/tail",
    "usr/bin/tee",
    "usr/bin/test",
    "usr/bin/timeout",
    "usr/bin/touch",
    "usr/bin/tr",
    "usr/bin/true",
    "usr/bin/truncate",
    "usr/bin/tsort",
    "usr/bin/tty",
    "usr/bin/uname",
    "usr/bin/unexpand",
    "usr/bin/uniq",
    "usr/bin/unlink",
    "usr/bin/uptime",
    "usr/bin/users",
    "usr/bin/vdir",
    "usr/bin/wc",
    "usr/bin/who",
    "usr/bin/whoami",
    "usr/bin/yes",
    "usr/bin/[",
    "usr/bin/dash",
    "usr/bin/login",
    "usr/bin/passwd",
    "usr/bin/su",
    "usr/bin/readelf",
    "usr/bin/objdump",
    "usr/bin/nm",
    "usr/bin/file",
    NULL
};

char *etc_files[] = {
    "usr/etc/passwd",
    "usr/etc/group",
    "usr/etc/inittab",
    "usr/etc/login.access",
    "usr/etc/login.defs",
    "usr/etc/limits",
    "usr/etc/profile",
    "usr/etc/now",
    "usr/etc/ld-musl-riscv64.path",
    NULL
};

char *usrsbins[] = {
    "usr/sbin/pwconv",
    "usr/sbin/pwunconv",
    NULL
};

char *lib_files[] = {
    "usr/lib/libc.so",
    "usr/lib/libmyadd.so",
    "usr/lib/libmagic.so.1",
    "usr/lib/libbsd.so.0",
    "usr/lib/libmd.so.0",
    NULL
};

char *local_bin_files[] = {
//    "usr/local/bin/file",
    NULL
};

char *usr_share_misc_files[] = {
    "usr/share/misc/magic.mgc",
    NULL
};

char *usr_libexec_coreutils_files[] = {
    "usr/libexec/coreutils/libstdbuf.so",
    NULL
};

char *home_zuki_files[] = {
    "usr/home/zuki/.profile",
    NULL
};

char *home_root_files[] = {
    "usr/home/root/.profile",
    NULL
};

char *text_files[] = {
    "usr/test.txt",
    "usr/test2.txt",
    NULL
};

char *test_files[] = {
    "usr/test/abc.txt",
    NULL
};


int nelms(char **ary) {
    int num = 0;
    for (char **p = ary; *p; p++) {
        num++;
    }
    return num;
}

#endif
