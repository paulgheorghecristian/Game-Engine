#include "ErrorUtils.h"

#include <cstdio>
#include <stdarg.h>

static const char *error_msgs[NUM_OF_ERROR_CODES] =
{
    "%s %s: executed successfully\n",
    "File %s not found\n",
    "OpenGL error: %s\n"
};

void print_error(ErrorCode ec, ...)
{
    va_list args;

    va_start(args, ec);
    vfprintf(stdout, error_msgs[(int) ec], args);
    va_end(args);
}
