#ifndef	INC_LINUX_UTSNAME_H
#define	INC_LINUX_UTSNAME_H

#define UNAME_SYSNAME "xv6"
#define UNAME_NODENAME "milkv-duo-256"
#define UNAME_RELEASE "0.3.1"
#define UNAME_VERSION "2025-03-24"
#define UNAME_MACHINE "riskv64"

struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char __domainname[65];
};

#endif
