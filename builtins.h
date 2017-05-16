#ifndef BUILTINS_H
#define BUILTINS_H

#include "lvals.h"

lval* builtin(lval* a, char* func);

lval* builtin_op(lval*, char*);

lval* builtin_head(lval*);
lval* builtin_tail(lval*);
lval* builtin_list(lval*);
lval* builtin_eval(lval*);
lval* builtin_init(lval*);
lval* builtin_cons(lval*);
lval* builtin_len(lval*);

#endif
