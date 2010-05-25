#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>

struct trace_ent {
    unsigned long ip;
    struct timeval tv;
} __attribute__((packed));

#define TRACE_BUF_SIZE 4096

static struct trace_ent *trace_buf;
static struct trace_ent *next;
static int trace_fd;

void flush_trace_buf() {
    write(trace_fd, trace_buf, TRACE_BUF_SIZE * sizeof *trace_buf);
    next = trace_buf;
}

void add_trace(int entry, unsigned long ip) __attribute__((regparm(3)));
void add_trace(int entry, unsigned long ip) {
    next->ip = (ip << 1) | !!entry;
    gettimeofday(&next->tv, NULL);

    if(++next == trace_buf + TRACE_BUF_SIZE)
        flush_trace_buf();
}

void __cyg_profile_func_enter (void *this_fn, void *call_site) {
    add_trace(1, (unsigned long)this_fn);
}

void __cyg_profile_func_exit  (void *this_fn, void *call_site) {
    add_trace(0, (unsigned long)this_fn);
}

void _init(void) {
    trace_buf = malloc(TRACE_BUF_SIZE * sizeof *trace_buf);
    next = trace_buf;
    trace_fd  = open("trace.out", O_WRONLY|O_TRUNC|O_CREAT, 0600);
    if (trace_fd < 0) {
        perror("opening trace.out");
        exit(1);
    }
}

void _fini(void) {
    close(trace_fd);
}
