#ifndef ERRORUTILS_H
#define ERRORUTILS_H

enum ErrorCode {
    NO_ERROR = 0,
    FILE_NOT_FOUND,
    OPENGL_ERROR,

    NUM_OF_ERROR_CODES
};

void print_error(ErrorCode ec, ...);

#endif // ERRORUTILS_H
