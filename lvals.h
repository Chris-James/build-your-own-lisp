#ifndef PROMPT_H
#define PROMPT_H

#include "mpc/mpc.h"
#include "types.h"
#include "utils.h"
#include "builtins.h"

lval* make_lval(int type, value x);
void lval_del(lval* v);
lval* lval_add(lval* s_expr, lval* new_lval);
lval* lval_read(mpc_ast_t* t);
void lval_println(lval* v);
lval* lval_eval(lval* v);
void lval_print(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);

#endif
