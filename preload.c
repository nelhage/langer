#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>

struct sigaction old;

struct trace_ent {
    unsigned long entry : 1;
    unsigned long ip    : 63;
    struct timeval tv;
};

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
    next->entry = entry;
    next->ip = ip;
    gettimeofday(&next->tv, NULL);

    if(++next == trace_buf + TRACE_BUF_SIZE)
        flush_trace_buf();
}

void _handle_sigtrap(int s, siginfo_t *sig, void *v) {
    ucontext_t *ctx = v;
    greg_t sp = ctx->uc_mcontext.gregs[REG_RSP];
    greg_t ip = ctx->uc_mcontext.gregs[REG_RIP];
    ctx->uc_mcontext.gregs[REG_RIP] = *((greg_t*)sp);
    ctx->uc_mcontext.gregs[REG_RSP] += sizeof(greg_t*);
    add_trace(0, ip);
}

void _init(void) {
    trace_buf = malloc(TRACE_BUF_SIZE * sizeof *trace_buf);
    next = trace_buf;
    trace_fd  = open("trace.out", O_WRONLY|O_TRUNC|O_CREAT, 0600);
    if (trace_fd < 0) {
        perror("opening trace.out");
        exit(1);
    }

    struct sigaction act = {
        .sa_sigaction = _handle_sigtrap
    };
    sigaction(SIGTRAP, &act, &old);
}

void _fini(void) {
    sigaction(SIGTRAP, &old, NULL);
    close(trace_fd);
}
