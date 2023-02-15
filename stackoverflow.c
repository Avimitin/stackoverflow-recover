#include "sigsegv.h"

#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>

#include "altstack-util.h"

static jmp_buf mainloop;
static sigset_t mainsigset;

static volatile char *stack_lower_bound;
static volatile char *stack_upper_bound;

static volatile int * endless_recurse(int n, volatile int * ptr) {
  if (n < INT_MAX) {
    *endless_recurse(n + 1, ptr) += n;
  }

  return ptr;
}

static int start_recurse(volatile int n) {
  return *endless_recurse(n, &n);
}

static void stackoverflow_handler_continuation(void *a1, void *a2, void *a3) {
  int arg = (int) (long) a1;
  longjmp(mainloop, arg);
}

// In Linux, `stackoverflow_context_t` is `ucontext_t *`
static void stackoverflow_handler(int emergency, stackoverflow_context_t ctx) {
  char dummy;
  volatile char *addr = &dummy;
  if (!(addr >= stack_lower_bound && addr <= stack_upper_bound))
    abort ();
  printf ("Stack overflow caught.\n");
  sigprocmask (SIG_SETMASK, &mainsigset, NULL);
  sigsegv_leave_handler (stackoverflow_handler_continuation,
                         (void *) (long) (emergency ? -1 : 1), NULL, NULL);
}

int main() {
  sigset_t emptyset;

  /* pre-allocate a new extra stack for the recover handler */
  prepare_alternate_stack();

  /* Install the stack overflow handler.  */
  if (stackoverflow_install_handler (&stackoverflow_handler,
                                     mystack, MYSTACK_SIZE)
      < 0)
    exit (2);
  stack_lower_bound = mystack;
  stack_upper_bound = mystack + MYSTACK_SIZE - 1;

  /* Save the current signal mask.  */
  sigemptyset (&emptyset);
  sigprocmask (SIG_BLOCK, &emptyset, &mainsigset);

  switch(setjmp( mainloop )) {
    case 0:
      printf("starting recursion\n");
      start_recurse(0);
      printf("Unreachable\n"); exit(1);
    case 1:
      break;
    default:
      printf("Something unexpected happened");
      abort();
  }

  check_alternate_stack_no_overflow();

  printf("All done\n");
  exit(0);
}
