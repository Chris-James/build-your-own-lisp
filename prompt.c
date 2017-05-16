#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"
#include "utils.h"
#include "lvals.h"
#include "builtins.h"

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
