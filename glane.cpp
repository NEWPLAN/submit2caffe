#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include "glane_library.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define PFN_MASK_SIZE 8


int set_cpu_affinity(int core_id)
{
    int result;
    cpu_set_t  mask;
    CPU_ZERO(&mask);
    CPU_SET(core_id, &mask);
    result = sched_setaffinity(0, sizeof(mask), &mask);
    return result;
}

inline uint64_t current_time();
uint64_t current_time()
{
    struct timespec tstart = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    return tstart.tv_nsec;
}

uint64_t mem_virt2phy(const void *virtaddr)
{
    int fd, retval;
    uint64_t page, physaddr;
    unsigned long long virt_pfn;    // virtual page frame number
    int page_size;
    off_t offset;

    /* standard page size */
    page_size = getpagesize();

    fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "%s(): cannot open /proc/self/pagemap: %s\n", __func__, strerror(errno));
        return 0;
    }

    virt_pfn = (unsigned long)virtaddr / page_size;
    //printf("Virtual page frame number is %llu\n", virt_pfn);

    offset = sizeof(uint64_t) * virt_pfn;
    if (lseek(fd, offset, SEEK_SET) == (off_t) - 1)
    {
        fprintf(stderr, "%s(): seek error in /proc/self/pagemap: %s\n", __func__, strerror(errno));
        close(fd);
        return 0;
    }

    retval = read(fd, &page, PFN_MASK_SIZE);
    close(fd);
    if (retval < 0)
    {
        fprintf(stderr, "%s(): cannot read /proc/self/pagemap: %s\n", __func__, strerror(errno));
        return 0;
    }
    else if (retval != PFN_MASK_SIZE)
    {
        fprintf(stderr, "%s(): read %d bytes from /proc/self/pagemap but expected %d\n",
                __func__, retval, PFN_MASK_SIZE);
        return 0;
    }

    /*
     * the pfn (page frame number) are bits 0-54 (see
     * pagemap.txt in linux Documentation)make cl
     */
    if ((page & 0x7fffffffffffffULL) == 0)
    {
        fprintf(stderr, "Zero page frame number\n");
        return 0;
    }

    physaddr = ((page & 0x7fffffffffffffULL) * page_size) + ((unsigned long)virtaddr % page_size);

    return physaddr;
}


int main(int argc, char** argv)
{
    int fd, core_num, core_id = 19, i;
    int cmd_length = 95000, sub_length = 95000, cpl_length = 95000, sub_total_num = 0, cpl_total_num = 0;
    struct glane_entry cmds[cmd_length];
    struct glane_entry cpls[cmd_length];
    uint64_t useless = 0x123;

    int launch = 0;
    int defaults = 0;
    printf("your seted launch time: ", launch);
    scanf("%d", &launch);
    defaults = launch;


    core_num = init_driver("/dev/glaneoncpu0");
    if (core_num < 0)
    {
        fprintf(stderr, "init_driver(): error\n");
        goto failed;
    }

    if (set_cpu_affinity(core_id))
    {
        fprintf(stderr, "set_cpu_affinity(): error\n");
        goto free_driver;
    }

    for (i = 0; i < cmd_length; i++)
    {
        cmds[i].req_type = 0x40; // REQ_AIPRE_READ
        cmds[i].dst_ipv4 = 123;
        cmds[i].dst_devid = 234;
        cmds[i].req_flag = 34;
    }

    while (launch-- > 0)
    {
        cmd_length = sub_length;
        do
        {
            sub_length = submit_cmd(core_id + 1, cmds, cmd_length);
            if (sub_length < 0)
            {
                fprintf(stderr, "submit_cmd(): error\n");
                goto free_driver;
            }
            // printf("submit: %d\n", sub_length);
            cmd_length -= sub_length;
        }
        while (cmd_length != 0);

        if (cmd_length != 0)
        {
            fprintf(stderr, "submit_cmd(): error\n");
            goto free_driver;
        }

        if (launch % 100 == 0)
            printf("submit success in %d\n", defaults - launch);

        cmd_length = cpl_length;
        do
        {
            cpl_length = read_completion(core_id + 1, cpls, cmd_length);
            if (cpl_length < 0)
            {
                fprintf(stderr, "read_completion(): error\n");
                goto free_driver;
            }
            // printf("complete: %d\n", cpl_length);
            cmd_length -= cpl_length;
        }
        while (cmd_length != 0);


        if (cmd_length != 0)
        {
            fprintf(stderr, "read_completion(): error\n");
            goto free_driver;
        }
        if (launch % 100 == 0)
            printf("complete success in %d\n", defaults - launch);

        for (i = 0; i < sub_length; i++)
            if (memcmp(&cmds[i], &cpls[i], sizeof(struct glane_entry)) != 0)
            {
                fprintf(stderr, "memcmp(): error\n");
                goto free_driver;
            }
        if (launch % 100 == 0)
            printf("memcmp success in %d\n", defaults - launch);
    }
    // if (mem_virt2phy(&useless) == 0)
    // {
    //     printf("user bar ctrl failed\n");
    //     goto free_driver;
    // }
    // if (write_target_address(mem_virt2phy(&useless)) || write_target_value(0x888) || trigger_fpga())
    // {
    //     printf("user bar ctrl failed\n");
    //     goto free_driver;
    // }

    // while(useless == 0x123)
    // {
    //     printf("value: 0x%lX\n",useless);
    //     sleep(0.2);
    // }

    // printf("user bar ctrl success\n");

free_driver:
    free_driver();
failed:
    return 0;
}
