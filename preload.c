#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>
#include <string.h>

struct trace_ent {
    unsigned long ip;
    struct timeval tv;
} __attribute__((packed));

#define TRACE_BUF_SIZE 4096

static struct trace_ent *trace_buf;
static struct trace_ent *next;
static int trace_fd = -1;

struct function_record {
    unsigned long ip;
    int calls;
    int depth;
    struct timeval entry;
    struct timeval elapsed;
    struct function_record *hash_next;
};

struct profile_entry {
    unsigned long ip;
    unsigned long calls;
    unsigned long elapsed_sec;
    unsigned long elapsed_usec;
} __attribute__((packed));

#define HASH_SIZE 4096
#define hash_ip(ip) ((ip) % HASH_SIZE)

static struct function_record **fn_hash;

static struct function_record *lookup_record(unsigned long ip) {
    struct function_record *f = fn_hash[hash_ip(ip)];
    while (f && f->ip != ip) f = f->hash_next;
    return f;
}

static struct function_record *new_record(unsigned long ip) {
    struct function_record *r = lookup_record(ip);

    if (r) return r;

    r = malloc(sizeof *r);
    memset(r, 0, sizeof *r);

    r->ip = ip;
    r->hash_next = fn_hash[hash_ip(ip)];
    fn_hash[hash_ip(ip)] = r;
    return r;
}

static void flush_trace_buf() {
    write(trace_fd, trace_buf, (next - trace_buf) * sizeof *trace_buf);
    next = trace_buf;
}

static void add_trace(int entry, unsigned long ip, struct timeval *tv) {
    if (!next)
        return;
    next->ip = (ip << 1) | !!entry;
    next->tv = *tv;

    if(++next == trace_buf + TRACE_BUF_SIZE)
        flush_trace_buf();
}

/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */
int
timeval_subtract (result, x, y)
     struct timeval *result, *x, *y;
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

int timeval_add(struct timeval *res, const struct timeval *x, const struct timeval *y)
{
    res->tv_sec = x->tv_sec+y->tv_sec;

    res->tv_usec = x->tv_usec + y->tv_usec;

    while( res->tv_usec > 1000000 ){
        res->tv_usec -= 1000000;
        res->tv_sec++;
    }

    return 0;
}

void accrue_time(struct function_record *r, struct timeval *tv) {
    struct timeval t1, t2;
    timeval_subtract(&t1, tv, &r->entry);
    timeval_add(&t2, &t1, &r->elapsed);
    r->elapsed = t2;
}

static void log_times(void) {
    struct function_record *r;
    struct profile_entry prof;
    int fd, i;

    fd = open("langer.prof", O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) {
        perror("open(langer.prof)");
        return;
    }

    for (i = 0; i < HASH_SIZE; i++) {
        r = fn_hash[i];
        while (r) {
            prof.ip           = r->ip;
            prof.calls        = r->calls;
            prof.elapsed_sec  = r->elapsed.tv_sec;
            prof.elapsed_usec = r->elapsed.tv_usec;
            write(fd, &prof, sizeof prof);
            r = r->hash_next;
        }
    }

    close(fd);
}

void __cyg_profile_func_enter (void *this_fn, void *call_site) {
    struct timeval tv;
    struct function_record *r;
    gettimeofday(&tv, NULL);
    add_trace(1, (unsigned long)this_fn, &tv);

    r = new_record((unsigned long)this_fn);
    if (!r->depth)
        r->entry = tv;
    r->depth++;
    r->calls++;
}

void __cyg_profile_func_exit  (void *this_fn, void *call_site) {
    struct timeval tv;
    struct function_record *r;

    gettimeofday(&tv, NULL);
    add_trace(0, (unsigned long)this_fn, &tv);

    r = new_record((unsigned long)this_fn);
    r->depth--;
    if (!r->depth) {
        accrue_time(r, &tv);
    }
}

void __attribute__((constructor)) start_profiling(void);
void start_profiling(void) {
    const char *trace_file = getenv("LANGER_TRACE_ALL");
    if (trace_file) {
        trace_buf = malloc(TRACE_BUF_SIZE * sizeof *trace_buf);
        next = trace_buf;
        trace_fd  = open(trace_file, O_WRONLY|O_TRUNC|O_CREAT, 0600);
        if (trace_fd < 0) {
            perror("opening trace file");
            exit(1);
        }
    }

    fn_hash = malloc(HASH_SIZE * sizeof *fn_hash);
    memset(fn_hash, 0, HASH_SIZE * sizeof *fn_hash);
}

void __attribute__((destructor)) stop_profiling(void);
void stop_profiling(void) {
    if (trace_fd > 0) {
        flush_trace_buf();
        close(trace_fd);
    }
    log_times();
}
