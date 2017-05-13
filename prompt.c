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
  LVAL_QEXPR
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
void lval_println(lval* v);
lval* lval_eval(lval* v);
void lval_print(lval* v);

int main(int argc, char ** argv) {

  mpc_parser_t * Number = mpc_new("number");
  mpc_parser_t * Symbol = mpc_new("symbol");
  mpc_parser_t * Expr = mpc_new("expr");
  mpc_parser_t * Sexpr = mpc_new("sexpr");
  mpc_parser_t * Qexpr = mpc_new("qexpr");
  mpc_parser_t * Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    " number : /-?[0-9]+(\\.[0-9]+)?/;                           \
      symbol : '+' | '-' | '*' | '/' | '%' | '^' | /m((in)|(ax))/ | \"head\" | \"tail\"; \
      expr   : <number> | <symbol> | <sexpr> | <qexpr>;                    \
      sexpr  : '(' <expr>* ')';                                  \
      qexpr  : '{' <expr>* '}';                                  \
      lispy  : /^/ <expr>* /$/;                                  \
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
    case LVAL_QEXPR:
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
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      // If S-Expression or Q-Expression delete all elements inside
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
 * print_expr
 * Prints an S or Q-Expression lval.
 *
 * @param v - Pointer to the lval to be printed.
 */
void print_expr(lval* v) {

  char open = (v->type == LVAL_SEXPR) ? '(' : '{';
  char close = (v->type == LVAL_SEXPR) ? ')' : '}';

  putchar(open);
  for (int i = 0; i < v->count; i++) {

    // Print value contained within
    lval_print(v->val.cell[i]);

    // Don't print trailing space if last element
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
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
    case LVAL_QEXPR:
      print_expr(v);
    break;
  }
}

/*******************************************************************************
 * lval_println
 * Prints an lval followed by newline.
 *
 * @param v - Pointer to the lval to print.
 */
void lval_println(lval* v) {
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
  if (strstr(t, "qexpr")) { tag = LVAL_QEXPR; }
  if (strcmp(t, ">") == 0 || strstr(t, "sexpr")) { tag = LVAL_SEXPR; }
  return tag;
}

/*******************************************************************************
 * is_delimiter
 * Returns 1 if given string is a delimiter.
 *
 * @param content - Pointer to string to be cheked.
 *
 * @return 1 if given string matches a delimiter
 */
int is_delimiter(char* content) {
  return !strcmp(content, "(") || !strcmp(content, ")") || !strcmp(content, "{") || !strcmp(content, "}");
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

    case LVAL_QEXPR:
      // ...set type & value of lval
      type = LVAL_QEXPR;
      // Set `v` to arbitrary value
      // V's value doesn't matter - it's ignored in `make_lval()` for s-expressions.
      v.num = 0;
    break;
  }

  // Package type and "raw" value as an lval
  lval* x = make_lval(type, v);

  // Convert child nodes to lvals & append to new lval
  for (int i = 0; i < t->children_num; i++) {
    if (is_delimiter(t->children[i]->contents)) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

/*******************************************************************************
 * lval_pop
 * Extracts a single element from given S-Expression.
 *
 * @desc lval_pop extracts a single element from an S-Expression at index `i`
 * and shifts the rest of the list backward so that it no longer contains that
 * lval*. It then returns the extracted value.
 *
 * @param v - The S-Expression containing the desired element.
 * @param i - The position of the element to extract.
 *
 * @return {lval*} x - The extracted lval.
 */
lval* lval_pop(lval* v, int i) {

  // Find the item at index `i`
  lval* x = v->val.cell[i];

  // Shift memory after the item at `i` over the top
  memmove(&v->val.cell[i], &v->val.cell[(i + 1)], sizeof(lval*) * (v->count-i-1));

  // Decrease the count of items in the list
  v->count--;

  // Reallocate the memory used
  v->val.cell = realloc(v->val.cell, sizeof(lval*) * v->count);

  return x;
}

/*******************************************************************************
 * lval_take
 * Extracts a single element from given S-Expression and deletes the source lval.
 *
 * @param v - The S-Expression containing the desired element.
 * @param i - The position of the desired element.
 * @return {lval*} x - Pointer to the extracted lval.
 */
lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
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
 * lval_eval_sexpr
 * Evaluates a valid S-Expression.
 *
 * @desc We first evaluate all the children of the S-Expression. If any of these
 * children are errors we return the first error we encounter. If the S-Expression
 * has no children we just return it directly. If the S-Expression has a single
 * child that child is returned. Otherwise, we ensure the first child is a valid
 * symbol and, if so, perform the desired operation and return the result.
 *
 * @param v - Pointer to the S-Expression to evaluate.
 *
 * @return {lval*} - Pointer to the result of evaluation.
 */
lval* lval_eval_sexpr(lval* v) {

  // Evaluate children
  for (int i = 0; i < v->count; i++) {

    v->val.cell[i] = lval_eval(v->val.cell[i]);

    // Perform error check
    if (v->val.cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  // Empty expression
  if (v->count == 0) { return v; }

  // Single expression
  if (v->count == 1) { return lval_take(v, 0); }

  // Ensure that first element is symbol, else return BAD_OP error
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    value v;
    v.err = L_ERR_BAD_OP;
    return make_lval(LVAL_ERR, v);
  }

  // Call builtin with operator
  lval* result = builtin(v, f->val.sym);

  lval_del(f);
  return result;
}

/*******************************************************************************
 * lval_eval
 * Returns the result of evaluating a valid lval.
 *
 * @param v - Pointer to the lval to evaluate.
 * @return {lval*} result - Pointer to the result of evaluation.
 */
lval* lval_eval(lval* v) {
  lval* result = v;
  switch (v->type) {
    case LVAL_SEXPR:
      result = lval_eval_sexpr(v);
    break;
    default:
      // All other lval types remain the same
    break;
  }
  return result;
}
