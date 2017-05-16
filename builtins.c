#include "builtins.h"

#define L_ASSERT(arg, condition, error) if (!(condition)) { lval_del(arg); value e; e.err = error; return make_lval(LVAL_ERR, e); }

char *builtin_names[] = { "head", "tail", "list", "eval", "init", "cons", "len", NULL };
lval* (*builtinFn[])(lval*) = { builtin_head, builtin_tail, builtin_list, builtin_eval, builtin_init, builtin_cons, builtin_len, NULL };

/*******************************************************************************
 * builtin_op
 * Evaluates given lval according to given operator.
 *
 * @param a - The lval to evaluate.
 * @param op - The operation to perform.
 * @return {loval*} - The resulting expression.
 */
lval* builtin_op(lval* a, char* op) {

  // Ensure all arguments are numbers
  for (int i = 0; i < a->count; i++) {
    if (a->val.cell[i]->type != LVAL_NUM) {
      lval_del(a);
      value v;
      v.err = L_ERR_BAD_NUM;
      return make_lval(LVAL_ERR, v);
    }
  }

  // Pop first element
  lval* x = lval_pop(a, 0);
  value result = x->val;

  // If subtraction operator & no arguments - perform unary negation
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->val.num = -x->val.num;
  }

  /* While there are still elements remaining */
  while (a->count > 0) {

    /* Pop the next element */
    lval* y = lval_pop(a, 0);

    switch (*op) {
      case '+':
        x->val.num += y->val.num;
      break;
      case '-':
        x->val.num -= y->val.num;
      break;
      case '*':
        x->val.num *= y->val.num;
      break;
      case '/':
        if (y->val.num == 0) {
          lval_del(x);
          value v;
          v.err = L_ERR_DIV_ZERO;
          x = make_lval(LVAL_ERR, v);
          break;
        } else {
          x->val.num /= y->val.num;
        }
      break;
      case '%':
        x->val.num %= y->val.num;
      break;
      case '^':
        if (y->val.num == 0) {
          result.num = 1;
        } else if (y->val.num == 1) {
          continue;
        } else {
          while (y->val.num > 1) {
            result.num *= x->val.num;
            y->val.num--;
          }
        }
        x->val.num = result.num;
      break;
      case 'm':
        switch (*(op + 1)) {
          case 'i':
            // Operator is "min"
            result.num = (y->val.num < result.num) ? y->val.num : x->val.num;
          break;
          case 'a':
            // Operator is "max"
            result.num = (y->val.num > result.num) ? y->val.num : x->val.num;
          break;
        }
        x->val = result;
      break;
      case 'h':
      break;
      case 't':
        x->val = y->val;
      break;
    }
    lval_del(y);
  }
  lval_del(a);
  return x;
}



/*******************************************************************************
 * builtin
 * Determines if given function is a builtin command, executes it if so.
 *
 * @param a - Pointer to the lval to necessary to execute command.
 * @param func - Pointer to the name of the command to check for/execute.
 *
 * @return - Pointer to resulting lval or error lval.
 */
lval* builtin(lval* a, char* func) {

  // Search builtin command list for the given function
  for (int i = 0; builtin_names[i] != NULL; i++) {

    // If found, execute the builtin command
    if (strcmp(func, builtin_names[i]) == 0) {
      return builtinFn[i](a);
    }

  }

  // If given function matches a builtin operation, perform operation
  if (strstr("+-/*", func)) {
    return builtin_op(a, func);
  }

  lval_del(a);
  value e;
  e.err = L_ERR_BAD_OP;
  return make_lval(LVAL_ERR, e);
}



/*******************************************************************************
 * builtin_head
 * Returns the first element of a given Q-Expression.
 *
 * @param a - Pointer to the Q-Expression to operate on.
 *
 * @return -  Pointer to the first element of Q-Expression.
 */
lval* builtin_head(lval* a) {

  // Ensure only one argument passed
  L_ASSERT(a, a->count == 1, L_ERR_ARG_COUNT);

  // Ensure argument passed was a Q-Expression
  L_ASSERT(a, a->val.cell[0]->type == LVAL_QEXPR, L_ERR_BAD_TYPE);

  // Ensure Q-Expression passed was not empty
  L_ASSERT(a, a->val.cell[0]->count != 0, L_ERR_EMPTY_Q);

  // Otherwise take first argument
  lval* v = lval_take(a, 0);

  //Delete all elements that are not head element
  while (v->count > 1) { lval_del(lval_pop(v, 1)); }

  return v;
}



/*******************************************************************************
 * builtin_tail
 * Returns all elements in given Q-Expression except the first.
 *
 * @desc Pops and deletes the item at index 0, returns remaining elements.
 *
 * @param a - Pointer to the Q-Expression to operate on.
 *
 * @return v - Pointer to the list of remaining elements.
 */
lval* builtin_tail(lval* a) {

  // Ensure only one argument passed
  L_ASSERT(a, a->count == 1, L_ERR_ARG_COUNT);

  // Ensure argument passed was a Q-Expression
  L_ASSERT(a, a->val.cell[0]->type == LVAL_QEXPR, L_ERR_BAD_TYPE);

  // Ensure Q-Expression passed was not empty
  L_ASSERT(a, a->val.cell[0]->count != 0, L_ERR_EMPTY_Q);

  // Take first argument
  lval* v = lval_take(a, 0);

  // Delete first element and return
  lval_del(lval_pop(v, 0));
  return v;
}



/*******************************************************************************
 * builtin_list
 * Returns given S-Expression as a Q-Expression.
 *
 * @param a - Pointer to the S-Expression to convert.
 *
 * @return a - Pointer to the Q-Expression.
 */
lval* builtin_list(lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}



/*******************************************************************************
 * builtin_eval
 * Evaluates a given Q-Expression.
 *
 * @desc Takes as input some single Q-Expression, which it converts to an
 * S-Expression, and evaluates using lval_eval.
 *
 * @param a - Pointer to the Q-Expression to convert & evaluate.
 *
 * @return a - Pointer to the lval resulting from evaluation.
 */
lval* builtin_eval(lval* a) {

  // Ensure only one argument passed
  L_ASSERT(a, a->count == 1, L_ERR_ARG_COUNT);

  // Ensure argument passed was a Q-Expression
  L_ASSERT(a, a->val.cell[0]->type == LVAL_QEXPR, L_ERR_BAD_TYPE);

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;

  return lval_eval(x);
}



/*******************************************************************************
 * builtin_init
 * Returns all elements in given Q-Expression except the last.
 *
 * @param a - Pointer to the Q-Expression to operate on.
 *
 * @return - Pointer to the list of remaining elements.
 */
lval* builtin_init(lval* a) {

  // Ensure only one argument passed
  L_ASSERT(a, a->count == 1, L_ERR_ARG_COUNT);

  // Ensure argument passed was a Q-Expression
  L_ASSERT(a, a->val.cell[0]->type == LVAL_QEXPR, L_ERR_BAD_TYPE);

  // Ensure Q-Expression passed was not empty
  L_ASSERT(a, a->val.cell[0]->count != 0, L_ERR_EMPTY_Q);

  // Take first argument
  lval* v = lval_take(a, 0);
  lval_del(lval_pop(v, (v->count - 1)));
  return v;
}



/*******************************************************************************
 * builtin_cons
 * Appends given value to the front of given Q-Expression.
 *
 * @param args - The value (at index 0) & Q-Expression (at index 1) to concatenate.
 *
 * @return new_q - Pointer to newly constructed Q-Expression.
 *
 * @example
 *
 * cons 1 {2 3 4}
 * // => {1 2 3 4}
 */
lval* builtin_cons(lval* args) {

  // Ensure exactly two arguments passed
  L_ASSERT(args, args->count == 2, L_ERR_ARG_COUNT);

  // Ensure arguments are valid types
  L_ASSERT(args, args->val.cell[0]->type == LVAL_NUM, L_ERR_BAD_TYPE);
  L_ASSERT(args, args->val.cell[1]->type == LVAL_QEXPR, L_ERR_BAD_TYPE);

  // Create new Q-Expression with a dummy value
  value _;
  _.num = 0;
  lval * new_q = make_lval(LVAL_QEXPR, _);

  // Append given value to new Q-Expression
  new_q = lval_add(new_q, lval_pop(args, 0));

  // Append elements in given Q-Expression to new Q-Expression
  lval* r = lval_eval(lval_take(args, 0));
  while (r->count) {
    new_q = lval_add(new_q, lval_pop(r, 0));
  }

  return new_q;
}



/*******************************************************************************
 * builtin_len
 * Returns the number of elements in a given Q-Expression.
 *
 * @param args - Arguments passed to `len` command.
 *
 * @return - Pointer to the number of elements in Q-Expression.
 *
 * @example
 *
 * len {2 4 6 8}
 * // => 4
 */
lval* builtin_len(lval* args) {

  // Ensure `len` was passed only one argument
  L_ASSERT(args, args->count == 1, L_ERR_ARG_COUNT);

  // Ensure `len` was passed a Q-Expression
  L_ASSERT(args, args->val.cell[0]->type == LVAL_QEXPR, L_ERR_BAD_TYPE);

  // Grab first element passed to `len`
  lval* q_expr = lval_take(args, 0);

  // Convert # of elements in given Q-Expression to a valid lispy value
  value v;
  v.num = q_expr->count;

  // Return new lval
  return make_lval(LVAL_NUM, v);
}
