#include <string.h>
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
