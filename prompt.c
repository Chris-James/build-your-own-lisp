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
 * lval_types
 * Possible lval types
 */
enum lval_types {
  LVAL_NUM,
  LVAL_ERR,
  LVAL_SYM,
  LVAL_SEXPR,
};

/**
 * lval_errors
 * Possible lval error types
 */
enum lval_errors {
  L_ERR_DIV_ZERO,
  L_ERR_BAD_OP,
  L_ERR_BAD_NUM
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

lval* make_lval(int type, value x);
void lval_del(lval* v);
lval* lval_add(lval* s_expr, lval* new_lval);
lval* lval_read(mpc_ast_t* t);

void lval_print(lval* v);
void lval_println(lval v);

lval eval(mpc_ast_t* t);

int main(int argc, char ** argv) {

  mpc_parser_t * Number = mpc_new("number");
  mpc_parser_t * Symbol = mpc_new("symbol");
  mpc_parser_t * Expr = mpc_new("expr");
  mpc_parser_t * Sexpr = mpc_new("sexpr");
  mpc_parser_t * Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    " number  : /-?[0-9]+(\\.[0-9]+)?/;                           \
      symbol: '+' | '-' | '*' | '/' | '%' | '^' | /m((in)|(ax))/; \
      expr    : <number> | <symbol> | <sexpr>;                    \
      sexpr   : '(' <expr>* ')';                                  \
      lispy   : /^/ <expr>* /$/;                                  \
    ",
    Number, Symbol, Expr, Sexpr, Lispy
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

  mpc_cleanup(5, Number, Symbol, Expr, Sexpr, Lispy);
  return 0;
}

/*******************************************************************************
 * make_lval
 * Packages a given type and "raw" value as a valid lval.
 *
 * @param {int} type [`LVAL_{NUM|ERR|SYM|SEXPR}`] - Type of lval to construct.
 * @param {value} x - The "raw" value of lval.
 * @return {lval*} v - Pointer to valid lval.
 */
lval* make_lval(int type, value x) {
  lval* v = malloc(sizeof(lval));
  v->type = type;
  switch (type) {
    case LVAL_SYM:
      v->val.sym = malloc(strlen(x.sym) + 1);
      strcpy(v->val.sym, x.sym);
    break;
    case LVAL_SEXPR:
      v->count = 0;
      v->val.cell = NULL;
    break;
    default:
      v->val = x;
    break;
  }
  return v;
}

/*******************************************************************************
 * lval_del
 * Delete an lval by freeing all allocated memory associated with lval & fields.
 *
 * @param v - Pointer to the lval to delete.
 */
void lval_del(lval* v) {

  switch (v->type) {
    case LVAL_SYM:
      // If symbol free the string data
      free(v->val.sym);
    break;
    case LVAL_SEXPR:
      // If S-Expression delete all elements inside
      for (int i = 0; i < v->count; i++) {
        lval_del(v->val.cell[i]);
      }
      // Free memory allocated to contain the pointers
      free(v->val.cell);
    break;
    case LVAL_NUM:
    case LVAL_ERR:
    break;
  }

  // Free memory allocated for lval itself
  free(v);
}

/*******************************************************************************
 * lval_add
 * Appends an lval to the given S-Expression's list of lvals.
 *
 * @param s_expr - Pointer to the S-Expression being updated.
 * @param new_lval - Pointer to the lval to append.
 *
 * @return s_expr - Pointer to the updated S-Expression lval.
 */
lval* lval_add(lval* s_expr, lval* new_lval) {
  s_expr->count++;
  s_expr->val.cell = realloc(s_expr->val.cell, (sizeof(lval*) * s_expr->count));
  s_expr->val.cell[(s_expr->count - 1)] = new_lval;
  return s_expr;
}

/*******************************************************************************
 * lval_print
 * Prints appropriate output for a given lval.
 *
 * @param v - The lval.
 *  field {int} v.type - The type of given lval.
 *  field {value} v.val - The "raw" value of given lval.
 */
void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:
      printf("%li", v->val.num);
    break;
    case LVAL_ERR:
      switch (v->val.err) {
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
    case LVAL_SYM:
      printf("%s", v->val.sym);
    break;
    case LVAL_SEXPR:
      putchar('(');
      for (int i = 0; i < v->count; i++) {

        /* Print Value contained within */
        lval_print(v->val.cell[i]);

        /* Don't print trailing space if last element */
        if (i != (v->count-1)) {
          putchar(' ');
        }
      }
      putchar(')');
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
 * get_lval_tag
 * Returns a node's tag's corresponding lval type.
 *
 * @desc Encapsulating this functionality allows us to write a switch statement
 * in lval_read() which makes it simpler to read & understand.
 *
 * @param t - The tag.
 * @return tag - The corresponding lval type for tag.
 */
int get_lval_tag(char* t) {
  int tag;
  if (strstr(t, "number")) { tag = LVAL_NUM; }
  if (strstr(t, "symbol")) { tag = LVAL_SYM; }
  if (strcmp(t, ">") == 0 || strstr(t, "sexpr")) { tag = LVAL_SEXPR; }
  return tag;
}

/*******************************************************************************
 * lval_read
 * Converts an ast node to a valid Lispy lval.
 *
 * @param t - The node to evaluate.
 *  field {char*} t.tag - The rule used to parse node.
 *  field {char*} t.contents - The actual contents of the node.
 *  field {struct**} t.children - Node's child nodes.
 * @return {lval*} x - Pointer to lval constructed for given node.
 */
lval* lval_read(mpc_ast_t* t) {

  value v;
  int type = LVAL_ERR;

  int tag = get_lval_tag(t->tag);
  switch (tag) {

    // Node is a number
    case LVAL_NUM:
      // ...attempt conversion from string to long integer
      errno = 0;
      v.num = strtol(t->contents, NULL, 10);
      if (errno != ERANGE) {
        // ...conversion successful, set type of value to NUM
        type = LVAL_NUM;
      } else {
        // ...something went wrong, set type of value to ERR
        type = LVAL_ERR;
        v.err = L_ERR_BAD_NUM;
      }
    break;

    // Node is a symbol
    case LVAL_SYM:
      // ...set type & value of lval
      type = LVAL_SYM;
      v.sym = t->contents;
    break;

    // Node is root or s-expression
    case LVAL_SEXPR:
      // ...set type & value of lval
      type = LVAL_SEXPR;
      // Set `v` to arbitrary value
      // V's value doesn't matter - it's ignored in `make_lval()` for s-expressions.
      v.num = 0;
    break;
  }

  // Package type and "raw" value as an lval
  lval* x = make_lval(type, v);

  // Convert child nodes to lvals & append to new lval
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
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
    value x;
    int type;

    x.num = strtol(t->contents, NULL, 10);
    if (errno != ERANGE) {
      type = LVAL_NUM;
    } else {
      type = LVAL_ERR;
      x.err = L_ERR_BAD_NUM;
     }
     return make_lval(type, x);
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
