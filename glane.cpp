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
    cpu_set_t mask;
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


int glane_main(void)
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
    printf("core number is: %d\n",core_num);

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
        } while (cmd_length != 0);

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
        } while (cmd_length != 0);

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

free_driver:
    free_driver();
failed:
    return 0;
}
