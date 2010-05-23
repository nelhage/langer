#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <ucontext.h>

struct sigaction old;

void _handle_sigtrap(int s, siginfo_t *sig, void *v) {
    ucontext_t *ctx = v;
    greg_t sp = ctx->uc_mcontext.gregs[REG_RSP];
    ctx->uc_mcontext.gregs[REG_RIP] = *((greg_t*)sp);
    ctx->uc_mcontext.gregs[REG_RSP] += sizeof(greg_t*);
}

void _init(void) {
    struct sigaction act = {
        .sa_sigaction = _handle_sigtrap
    };
    sigaction(SIGTRAP, &act, &old);
}

void _fini(void) {
    sigaction(SIGTRAP, &old, NULL);
}
