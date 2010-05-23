#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <udis86.h>

#include <bfd.h>

#define OP_RET   0xc3
#define OP_INT3  0xcc

#define assert(x) do { if(!(x)) { die("%s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #x); }} while(0)

void die(const char *err, ...) {
    va_list ap;

    va_start(ap, err);
    vfprintf(stderr, err, ap);
    va_end(ap);

    exit(1);
}

struct ret_entry {
    struct ret_entry *next;
    file_ptr off;
};

struct ret_entry *rets = NULL;

void add_ret(file_ptr off) {
    struct ret_entry *r = malloc(sizeof *r);
    r->next = rets;
    r->off = off;
    rets = r;
}

int no_prefixes(ud_t *ud) {
    return (ud->pfx_rex == UD_NONE  &&
            ud->pfx_seg == UD_NONE  &&
            ud->pfx_opr == UD_NONE  &&
            ud->pfx_adr == UD_NONE  &&
            ud->pfx_lock == UD_NONE &&
            ud->pfx_rep == UD_NONE  &&
            ud->pfx_repe == UD_NONE &&
            ud->pfx_repne == UD_NONE);
}

void do_mark_rets(bfd *abfd, asection *sect, void *data) {
    ud_t ud;
    bfd_byte *contents;

    if (!(sect->flags & SEC_CODE))
        return;
    if (strcmp(sect->symbol->name, ".init") == 0)
        return;

    printf("Scanning %s (at %lx)...\n",
           sect->symbol->name, (uint64_t)sect->vma);

    assert(bfd_malloc_and_get_section(abfd, sect, &contents));

    ud_init(&ud);
    ud_set_mode(&ud, bfd_get_arch_size(abfd));
    ud_set_pc(&ud, (uint64_t)sect->vma);
    ud_set_input_buffer(&ud, contents, sect->size);

    while (ud_disassemble(&ud)) {
        assert(ud.mnemonic != UD_Iinvalid);
        if (ud.mnemonic == UD_Iret
            && ud.operand[0].type == UD_NONE
            && no_prefixes(&ud)) {
            file_ptr fileoff = (sect->filepos + ud.pc
                                - (uint64_t)sect->vma
                                - ud_insn_len(&ud));
            add_ret(fileoff);
        }
    }
}

void do_replacement(const char *path) {
    struct ret_entry *r = rets;
    FILE *f = fopen(path, "r+");
    unsigned char c;
    assert(f);

    while(r) {
        assert(!fseek(f, r->off, SEEK_SET));
        fread(&c, 1, 1, f);
        assert(c == OP_RET);
        assert(!fseek(f, -1, SEEK_CUR));
        c = OP_INT3;
        fwrite(&c, 1, 1, f);
        r = r->next;
    }
    fclose(f);
}

int do_replace_nops(const char *path) {
    bfd *b;
    bfd_byte *contents, *patch;
    asection *rodata, *text;
    bfd_vma err_string;

    bfd_init();

    b = bfd_openr(path, NULL);
    assert(b);
    assert(bfd_check_format(b, bfd_object));

    bfd_map_over_sections(b, do_mark_rets, NULL);
    bfd_close(b);
    do_replacement(path);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s PROG\n", argv[0]);
        return 1;
    }

    do_replace_nops(argv[1]);
}
