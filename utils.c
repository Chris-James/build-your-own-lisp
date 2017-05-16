#include "utils.h"

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
