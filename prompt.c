#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"

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

enum { LVAL_NUM, LVAL_ERR };
enum { L_ERR_DIV_ZERO, L_ERR_BAD_OP, L_ERR_BAD_NUM };

typedef struct {
  int type;
  long num;
  int err;
} lval;

lval lval_num(long x);
lval lval_err(int x);

long eval_op(long x, char* op, long y);
long eval(mpc_ast_t* t);

int main(int argc, char ** argv) {

  mpc_parser_t * Number = mpc_new("number");
  mpc_parser_t * Operator = mpc_new("operator");
  mpc_parser_t * Expr = mpc_new("expr");
  mpc_parser_t * Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    " number  : /-?[0-9]+(\\.[0-9]+)?/;                             \
      operator: '+' | '-' | '*' | '/' | '%' | '^' | /m((in)|(ax))/; \
      expr    : <number> | '(' <operator> <expr>+ ')';              \
      lispy   : /^/ <operator> <expr>+ /$/;                         \
    ",
    Number, Operator, Expr, Lispy
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
      long result = eval(r.output);
      printf("%li \n", result);
      mpc_ast_delete(r.output);
    } else {
      // Error encountered
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  return 0;
}

/*******************************************************************************
 * lval_num
 * Constructs a _number_ type lval.
 *
 * @param {long} x -
 * @return {lval} val -
 */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/*******************************************************************************
 * lval_err
 * Constructs an _error_ type lval.
 *
 * @param {int} x -
 * @return {lval} val -
 */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/*******************************************************************************
 * eval_op
 * Returns the result of performing given operator on given operands.
 *
 * @param {long}  x  - First operand.
 * @param {char*} op - Desired operator.
 * @param {long}  y  - Second operand.
 * @return {long} result - The result of operation.
 */
long eval_op(long x, char* op, long y) {

  long result = x;

  switch (*op) {
    case '+':
      result += y;
      break;
    case '-':
      result -= y;
      break;
    case '*':
      result *= y;
      break;
    case '/':
      result /= y;
      break;
    case '%':
      result %= y;
      break;
    case '^':
      while (y > 1) {
        result *= x;
        y--;
      }
      break;
    case 'm':
      switch (*(op + 1)) {
        // Operator is "min"
        case 'i':
          result = (y < result) ? y : x;
          break;
        case 'a':
          // Operator is "max"
          result = (y > result) ? y : x;
      }
      break;
  }

  return result;
}

/*******************************************************************************
 * eval
 *
 * @param {struct*}  t     - The node to evaluate.
 *  field {char*}    t.tag - The rules used to parse node.
 *  field {char*}    t.contents - The actual contents of the node.
 *  field {struct**} t.children - Node's child nodes.
 * @return {long} x - Result of evaluation.
 */
long eval(mpc_ast_t* t) {

  // If tagged as a number, return.
  if (strstr(t->tag, "number")) {
    return atoi(t->contents);
  }

  // Operator is always the second child.
  char* op = t->children[1]->contents;

  // Store third child
  long x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }
  return x;
}
