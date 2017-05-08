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

/**
 * Possible lval types
 */
enum { LVAL_NUM, LVAL_ERR };

/**
 * Possible lval error types
 */
enum { L_ERR_DIV_ZERO, L_ERR_BAD_OP, L_ERR_BAD_NUM };

/**
 * value
 * Potential content of an lval
 */
typedef union {
  long num;
  int err;
} value;

/**
 * lval
 */
typedef struct {
  int type;
  value val;
} lval;

lval make_lval(int type, value x);

void lval_print(lval v);
void lval_println(lval v);

lval eval_op(lval x, char* op, lval y);
lval eval(mpc_ast_t* t);

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
      lval result = eval(r.output);
      lval_println(result);
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
 * make_lval
 * Returns a valid lval for given type.
 *
 * @param {int} type [`LVAL_NUM`|`LVAL_ERR`] - Type of lval to construct.
 * @param {value} x - Number to assign lval if type is `LVAL_NUM`, else the
 *        appropriate error code for lval.
 * @return {lval} v - Valid number or error lval.
 */
lval make_lval(int type, value x) {
  lval v;
  v.type = type;
  v.val = x;
  return v;
}

/*******************************************************************************
 * lval_print
 * Prints appropriate output message for given lval.
 *
 * @param {lval} v - The lval output.
 *  field {int}  v.type [`LVAL_NUM`|`LVAL_ERR`] - The type of given lval.
 *  field {value} v.val - The value of given lval.
 *  field {long} [v.val.num] - Value of lval if it is of type `LVAL_NUM`.
 *  field {int}  [v.val.err] - Type of error if lval is of type `LVAL_ERR`.
 */
void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM:
      printf("%li", v.val.num);
    break;
    case LVAL_ERR:
      switch (v.val.err) {
        case L_ERR_DIV_ZERO:
          printf("Error: Division by zero");
        break;
        case L_ERR_BAD_OP:
          printf("Error: Invalid Operator");
        break;
        case L_ERR_BAD_NUM:
          printf("Error: Invalid Number");
        break;
      }
    break;
  }
}

/*******************************************************************************
 * lval_println
 * Prints an lval followed by newline.
 *
 * @param {lval} v - The lval to print.
 */
void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

/*******************************************************************************
 * eval_op
 * Returns the result of performing given operator on given operands.
 *
 * @param {lval}  x  - First operand.
 * @param {char*} op - Desired operator.
 * @param {lval}  y  - Second operand.
 * @return {lval} result - The result of operation.
 */
lval eval_op(lval x, char* op, lval y) {

  // If either operand is an error, return operand
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  value result;
  result.num = x.val.num;
  int type = LVAL_NUM;

  switch (*op) {
    case '+':
      result.num += y.val.num;
      break;
    case '-':
      result.num -= y.val.num;
      break;
    case '*':
      result.num *= y.val.num;
      break;
    case '/':
      if (y.val.num == 0) {
        type = LVAL_ERR;
        result.err = L_ERR_DIV_ZERO;
      } else {
        result.num /= y.val.num;
      }
      break;
    case '%':
      result.num %= y.val.num;
      break;
    case '^':
      while (y.val.num > 1) {
        result.num *= x.val.num;
        y.val.num--;
      }
      break;
    case 'm':
      switch (*(op + 1)) {
        // Operator is "min"
        case 'i':
          result.num = (y.val.num < result.num) ? y.val.num : x.val.num;
          break;
        case 'a':
          // Operator is "max"
          result.num = (y.val.num > result.num) ? y.val.num : x.val.num;
      }
      break;
    default:
      type = LVAL_ERR;
      result.err = L_ERR_BAD_OP;
    break;
  }

  return make_lval(type, result);
}

/*******************************************************************************
 * eval
 *
 * @param {struct*}  t     - The node to evaluate.
 *  field {char*}    t.tag - The rules used to parse node.
 *  field {char*}    t.contents - The actual contents of the node.
 *  field {struct**} t.children - Node's child nodes.
 * @return {lval} x - Result of evaluation.
 */
lval eval(mpc_ast_t* t) {

  // If tagged as a number, return.
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? make_lval(LVAL_NUM, x) : make_lval(LVAL_ERR, L_ERR_BAD_NUM);
  }

  // Operator is always the second child.
  char* op = t->children[1]->contents;

  // Store third child
  lval x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }
  return x;
}
