#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"
#include "utils.h"
#include "lvals.h"

#ifdef _WIN32

#include <string.h>
#define BUFF_SIZE 2048

static char buffer[BUFF_SIZE];

char * readline(char * prompt) {
  fputs(prompt, stdout);
  fgets(buffer, BUFF_SIZE, stdin);
  char * cpy = malloc(strlen(buffer) + 1)
  strcpy(cpy, buffer);
  cpy[(strlen(cpy) - 1)] = '\0';
  return cpy;
}

void add_history(char * unused) {}
#else
#include <editline/readline.h>
#endif

lval* builtin_head(lval*);
lval* builtin_tail(lval*);
lval* builtin_list(lval*);
lval* builtin_eval(lval*);
lval* builtin_init(lval*);
lval* builtin_cons(lval*);
lval* builtin_len(lval*);

char *builtin_names[] = { "head", "tail", "list", "eval", "init", "cons", "len", NULL };
lval* (*builtinFn[])(lval*) = { builtin_head, builtin_tail, builtin_list, builtin_eval, builtin_init, builtin_cons, builtin_len, NULL };

int main(int argc, char ** argv) {

  mpc_parser_t * Number = mpc_new("number");
  mpc_parser_t * Symbol = mpc_new("symbol");
  mpc_parser_t * Expr = mpc_new("expr");
  mpc_parser_t * Sexpr = mpc_new("sexpr");
  mpc_parser_t * Qexpr = mpc_new("qexpr");
  mpc_parser_t * Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    " number : /-?[0-9]+(\\.[0-9]+)?/;                               \
      symbol : '+' | '-' | '*' | '/' | '%' | '^' | /m((in)|(ax))/    \
             | \"head\" | \"tail\" | \"list\" | \"eval\" | \"init\"  \
             | \"cons\" | \"len\";                                   \
      expr   : <number> | <symbol> | <sexpr> | <qexpr>;              \
      sexpr  : '(' <expr>* ')';                                      \
      qexpr  : '{' <expr>* '}';                                      \
      lispy  : /^/ <expr>* /$/;                                      \
    ",
    Number, Symbol, Expr, Sexpr, Qexpr, Lispy
  );

  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+C to exit");

  while (1) {
    char * input = readline("lispy> ");
    add_history(input);

    // Parse user input
    mpc_result_t r;

    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      // Evaluate input and print result
      lval* result = lval_eval(lval_read(r.output));
      lval_println(result);
      lval_del(result);
      mpc_ast_delete(r.output);
    } else {
      // Error encountered
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(6, Number, Symbol, Expr, Sexpr, Qexpr, Lispy);
  return 0;
}

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

  // Check errors
  // ...too many args passed
  if (a->count != 1) {
    lval_del(a);
    value e;
    e.err = L_ERR_ARG_COUNT;
    return make_lval(LVAL_ERR, e);
  }

  // ...invalid expression passed
  if (a->val.cell[0]->type != LVAL_QEXPR) {
    lval_del(a);
    value e;
    e.err = L_ERR_BAD_TYPE;
    return make_lval(LVAL_ERR, e);
  }

  // ...empty expression passed
  if (a->val.cell[0]->count == 0) {
    lval_del(a);
    value e;
    e.err = L_ERR_EMPTY_Q;
    return make_lval(LVAL_ERR, e);
  }

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

  // Check errors
  // ...too many args passed
  if (a->count != 1) {
    lval_del(a);
    value e;
    e.err = L_ERR_ARG_COUNT;
    return make_lval(LVAL_ERR, e);
  }

  // ...invalid expression passed
  if (a->val.cell[0]->type != LVAL_QEXPR) {
    lval_del(a);
    value e;
    e.err = L_ERR_BAD_TYPE;
    return make_lval(LVAL_ERR, e);
  }

  // ...empty expression passed
  if (a->val.cell[0]->count == 0) {
    lval_del(a);
    value e;
    e.err = L_ERR_EMPTY_Q;
    return make_lval(LVAL_ERR, e);
  }

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
  // Check errors
  // ...too many args passed
  if (a->count != 1) {
    lval_del(a);
    value e;
    e.err = L_ERR_ARG_COUNT;
    return make_lval(LVAL_ERR, e);
  }

  // ...invalid expression passed
  if (a->val.cell[0]->type != LVAL_QEXPR) {
    lval_del(a);
    value e;
    e.err = L_ERR_BAD_TYPE;
    return make_lval(LVAL_ERR, e);
  }

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

  // Grab first element passed to `len`
  lval* q_expr = lval_pop(args, 0);

  // Convert # of elements in given Q-Expression to a valid lispy value
  value v;
  v.num = q_expr->count;

  // Return new lval
  return make_lval(LVAL_NUM, v);
}
