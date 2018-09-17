#ifndef __MY_HUGE_PAGE__
#define __MY_HUGE_PAGE__

#if defined(__cplusplus)
extern "C"
{
#endif

#define HUGEPAGE_FILE "/dev/hugepages1G/random"
#define LENGTH (1024UL * 1024 * 1024)   // 1M
#define PROTECTION (PROT_READ | PROT_WRITE)
#define ADDR (void *)(0x0UL)
#define FLAGS (MAP_SHARED | MAP_POPULATE)
#define BAD_PHYS_ADDR 0
#define PFN_MASK_SIZE 8

int hugepage_main(void);

#if defined(__cplusplus)
}
#endif

#endif