#include <common/types.h>
#include <common/riscv.h>
#include "defs.h"
#include "sbi.h"
#include <common/param.h>

void _entry();

struct sbiret sbi_ecall(int ext, int fid, uint64_t arg0, uint64_t arg1,
        uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	struct sbiret ret;

	register uint64_t a0 asm ("a0") = arg0;
	register uint64_t a1 asm ("a1") = arg1;
	register uint64_t a2 asm ("a2") = arg2;
	register uint64_t a3 asm ("a3") = arg3;
	register uint64_t a4 asm ("a4") = arg4;
	register uint64_t a5 asm ("a5") = arg5;
	register uint64_t a6 asm ("a6") = fid;
	register uint64_t a7 asm ("a7") = ext;
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");

	ret.error = a0;
	ret.value = a1;

	return ret;
}

static inline long sbi_get_spec_version(void)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_GET_SPEC_VERSION, 0, 0, 0, 0, 0, 0);
	if (ret.error)
		panic("sbi_get_spec_version failed");

	return ret.value;
}

static inline long sbi_probe_extension(int extid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_ID_BASE, SBI_BASE_PROBE_EXTENSION, extid, 0, 0, 0, 0, 0);
	if (ret.error)
		return ret.error;

	return ret.value;
}

static inline long sbi_hart_start(uint64_t hartid, uint64_t saddr, uint64_t priv)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_ID_HSM, SBI_HSM_HART_START, hartid, saddr, priv, 0, 0, 0);
	if (ret.error)
		return ret.error;

	return ret.value;
}

void sbiinit(void)
{
	unsigned long next;
	uint64_t version = sbi_get_spec_version();
	uint64_t major = (version >> SBI_SPEC_VERSION_MAJOR_SHIFT) &
			SBI_SPEC_VERSION_MAJOR_MASK;
	uint64_t minor = version & SBI_SPEC_VERSION_MINOR_MASK;
	printf("SBI specification v%d.%d detected\n", major, minor);

	if (sbi_probe_extension(SBI_EXT_ID_TIME) > 0)
		printf("SBI TIME extension detected\n");

	if (sbi_probe_extension(SBI_EXT_ID_IPI) > 0)
		printf("SBI IPI extension detected\n");

	if (sbi_probe_extension(SBI_EXT_ID_RFNC) > 0)
		printf("SBI RFNC extension detected\n");

	if (sbi_probe_extension(SBI_EXT_ID_HSM) > 0) {
		printf("SBI HSM extension detected\n");

		uint64_t ret = SBI_SUCCESS;
		uint64_t hartid = 0;
		while (hartid < NCPU && ret == SBI_SUCCESS) {
			if (hartid == cpuid()) {
				hartid++;
				continue;
			}
			ret = sbi_hart_start(hartid, (uint64_t)_entry, 0);
			hartid++;
		}
	}

	if (sbi_probe_extension(SBI_EXT_ID_SRST) > 0)
		printf("SBI SRST extension detected\n");

	if (sbi_probe_extension(SBI_EXT_ID_PMU) > 0)
		printf("SBI PMU extension detected\n");

	csr_clear(CSR_IE, 1 << 5);
	next = csr_read(CSR_TIME) + INTERVAL;
	sbi_set_timer(next);
	csr_set(CSR_IE, 1 << 5);

	printf("\n");
}

void sbi_set_timer(unsigned long stime_value)
{
	sbi_ecall(SBI_EXT_ID_TIME, SBI_TIME_SET_TIMER, stime_value, 0,
                  0, 0, 0, 0);
}
