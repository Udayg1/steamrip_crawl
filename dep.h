#ifndef DEP_H
#define DEP_H

#include <stddef.h>

char** split(char* arr, size_t *size, char* spl);
size_t find(char* arr, char* str);
char* slice(char* arr, int start, int end);
char* strip(char* arr);
char* join(char** arr, size_t len, const char* delimiter);
int hashCode(char* key);
size_t count(char* arr, char* str);


#endif