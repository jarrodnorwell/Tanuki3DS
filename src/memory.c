#include "memory.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "3ds.h"
#include "emulator_state.h"
#include "svc_defs.h"
#include "types.h"

void sigsegv_handler(int sig, siginfo_t* info, void* ucontext) {
    u8* addr = info->si_addr;
    if (ctremu.system.virtmem <= addr &&
        addr < ctremu.system.virtmem + BITL(32)) {
        lerror("(FATAL) invalid 3DS memory access at %08x (pc near %08x)",
               addr - ctremu.system.virtmem, ctremu.system.cpu.pc);
        exit(1);
    }
    sigaction(SIGSEGV, &(struct sigaction){.sa_handler = SIG_DFL}, NULL);
}

void x3ds_memory_init(X3DS* system) {
    system->virtmem = mmap(NULL, BITL(32), PROT_NONE,
                           MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (system->virtmem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    struct sigaction sa = {.sa_sigaction = sigsegv_handler,
                           .sa_flags = SA_SIGINFO};
    sigaction(SIGSEGV, &sa, NULL);

    VMBlock* initblk = malloc(sizeof(VMBlock));
    *initblk = (VMBlock){
        .startpg = 0, .endpg = BIT(20), .perm = 0, .state = MEMST_FREE};
    system->vmblocks.startpg = BIT(20);
    system->vmblocks.endpg = BIT(20);
    system->vmblocks.next = initblk;
    system->vmblocks.prev = initblk;
    initblk->prev = &system->vmblocks;
    initblk->next = &system->vmblocks;
}

void x3ds_memory_destroy(X3DS* system) {
    while (system->vmblocks.next != &system->vmblocks) {
        VMBlock* tmp = system->vmblocks.next;
        system->vmblocks.next = system->vmblocks.next->next;
        free(tmp);
    }

    sigaction(SIGSEGV, &(struct sigaction){.sa_handler = SIG_DFL}, NULL);
    munmap(system->virtmem, BITL(32));
}

void insert_vmblock(X3DS* system, VMBlock* n) {
    VMBlock* l = system->vmblocks.next;
    while (l != &system->vmblocks) {
        if (l->startpg <= n->startpg && n->startpg < l->endpg) break;
        l = l->next;
    }
    VMBlock* r = l->next;
    n->next = r;
    n->prev = l;
    l->next = n;
    r->prev = n;

    while (r->startpg < n->endpg) {
        if (r->endpg < n->endpg) {
            n->next = r->next;
            n->next->prev = n;
            free(r);
            r = n->next;
        } else {
            r->startpg = n->endpg;
            break;
        }
    }
    if (n->startpg < l->endpg) {
        if (n->endpg < l->endpg) {
            VMBlock* nr = malloc(sizeof(VMBlock));
            *nr = *l;
            nr->prev = n;
            nr->next = r;
            n->next = nr;
            r->prev = nr;
            nr->startpg = n->endpg;
            nr->endpg = l->endpg;
            r = nr;
        }
        if (l->startpg == n->startpg) {
            n->prev = l->prev;
            n->prev->next = n;
            free(l);
            l = n->prev;
        } else {
            l->endpg = n->startpg;
        }
    }
    if (r->startpg < BIT(20) && r->perm == n->perm && r->state == n->state) {
        n->endpg = r->endpg;
        n->next = r->next;
        n->next->prev = n;
        free(r);
    }
    if (l->startpg < BIT(20) && l->perm == n->perm && l->state == n->state) {
        l->endpg = n->endpg;
        l->next = n->next;
        l->next->prev = l;
        free(n);
    }
}

void print_vmblocks(VMBlock* vmblocks) {
    VMBlock* cur = vmblocks->next;
    while (cur != vmblocks) {
        printf("[%08x,%08x,%d,%d] ", cur->startpg << 12, cur->endpg << 12,
               cur->perm, cur->state);
        cur = cur->next;
    }
    printf("\n");
}

void x3ds_vmalloc(X3DS* system, u32 base, u32 size, u32 perm, u32 state) {
    base = base & ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (!size) return;

    VMBlock* n = malloc(sizeof(VMBlock));
    *n = (VMBlock){.startpg = base >> 12,
                   .endpg = (base + size) >> 12,
                   .perm = perm,
                   .state = state};
    insert_vmblock(system, n);

    void* ptr = mmap(&system->virtmem[base], size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    system->used_memory += size;
    linfo("mapped 3DS virtual memory at %08x with size 0x%x, perm %d, state %d",
          base, size, perm, state);
    print_vmblocks(&system->vmblocks);
}

VMBlock* x3ds_vmquery(X3DS* system, u32 addr) {
    addr >>= 12;
    VMBlock* b = system->vmblocks.next;
    while (b != &system->vmblocks) {
        if (b->startpg <= addr && addr < b->endpg) return b;
        b = b->next;
    }
    return NULL;
}