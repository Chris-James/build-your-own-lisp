#ifndef TYPES_H
#define TYPES_H

/**
 * lval_types
 * Possible lval types
 */
enum lval_types {
  LVAL_NUM,
  LVAL_ERR,
  LVAL_SYM,
  LVAL_SEXPR,
  LVAL_QEXPR
};

/**
 * lval_errors
 * Possible lval error types
 */
enum lval_errors {
  L_ERR_DIV_ZERO,
  L_ERR_BAD_OP,
  L_ERR_BAD_NUM,
  L_ERR_BAD_TYPE,
  L_ERR_EMPTY_Q,
  L_ERR_ARG_COUNT
};

/**
 * value
 * Potential content of an lval
 */
typedef union {
  long num;
  int err;
  char* sym;
  struct lval** cell;
} value;

/**
 * lval
 */
typedef struct lval {
  int type;
  value val;
  int count;
} lval;

#endif
