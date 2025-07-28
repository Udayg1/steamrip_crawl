#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DEP
char* slice(char* arr, int start, int end);
void strip(char* arr);

char** split(const char* arr, size_t* size, const char* spl) {
    int cap = 16;
    char** out = malloc(cap * sizeof(char*));
    // size_t len = strlen(arr);
    size_t delim_len = strlen(spl);
    size_t count_out = 0;

    const char* start = arr;
    const char* pos = strstr(start, spl);

    while (pos != NULL) {
        size_t part_len = pos - start;
        char* part = malloc(part_len + 1);
        memcpy(part, start, part_len);
        part[part_len] = '\0';
        if (count_out + 1 >= cap) {
            cap *= 2;
            out = realloc(out, cap * sizeof(char*));
        }
        strip(part);
        out[count_out++] = part;
        start = pos + delim_len;
        pos = strstr(start, spl);
    }

    // Add the last part
    char* part = strdup(start);
    out[count_out++] = part;

    out[count_out] = NULL;
    *size = count_out;
    return out;
}

size_t find(char* arr, char* str){  // return first occurence of str in the arr. 
    size_t len = strlen(str);
    for (int i = 0; i < strlen(arr) - len; i++){
        int match = 1;
        for (int j = 0; j < len; j++){
            if(str[j] != arr[i+j]){  // compare word by word of the substring. 
                match = 0;
                break;
            }
        }
        if (match){
            return i;
        }
    }
    return len;
}

char* slice(char* arr, int start, int end){  // slice the string starting from start upto and including end -1. return upto the end if end > length of string. 
    if (start < 0 || end < 0){
        perror("Cant slice negative index");
        exit(1);
    }
    if (end < start){
        perror("End index must be greater than start index");
        exit(1);
    }
    if (end > strlen(arr)){
        end = strlen(arr);
    }
    char* out = (char*) malloc(end - start + 10);
    int count = 0;
    for (int i = start; i < end; i++){
        out[count++] = arr[i];
    }
    out[count] = '\0';
    return out;
}

void strip(char* arr){
    int len = strlen(arr);
    for (int index = len-1; index >= 0; index--){
        if (!isspace(arr[index])){  // traverse from end and put null after first non white space encountered
            arr[index+1]='\0';
            break;
        }
    }
    int start = 0;
    while (arr[start] && isspace((unsigned char)arr[start])) {  // start from the beginning and add move the memory from first non white char 
        start++;
    }
    if (start > 0) {
        memmove(arr, arr + start, strlen(arr + start) + 1);  // +1 to copy the null terminator
    }
}

char* join(char** arr, size_t len, const char* delimiter){
    size_t tlen = 0;
    for (int i = 0; arr[i] != NULL; i++){  // calculate total length to allocate to the output string
        tlen += strlen(arr[i]) + strlen(delimiter);
    }
    char* out = (char*)malloc(tlen+1);
    out[0] = '\0';  // strcat require this to find where the string ends.
    for (size_t i = 0; arr[i] != NULL; i++){
        strcat(out, arr[i]);
        if (i < len-1){
            strcat(out, delimiter);  // add data and delimiter except the last iteration. 
        }
    }
    return out;
}

int hashCode(char* key){
    size_t len = strlen(key), size = 16;
    char* out = (char*) malloc(size);
    out[0] = '\0';
    char buf[16];
    for (int i = 0; i < len; i++){
        sprintf(buf, "%d", key[i]);
        if (strlen(buf) >= (size - strlen(out))){
            size *= 2;
            out = realloc(out, size);
            if (!out){
                perror("Memory allocation failed!");
            }
        }
        strcat(out, buf);
    }
    size_t data = atoll(out);
    free(out);
    return data;
}

size_t count(char* arr, char* str){  // return first occurence of str in the arr. 
    size_t len = strlen(str), count_ = 0;
    for (int i = 0; i < strlen(arr) - len; i++){
        int match = 1;
        for (int j = 0; j < len; j++){
            if(str[j] != arr[i+j]){  // compare word by word of the substring. 
                match = 0;
                break;
            }
        }
        if (match){
            count_ += 1;
        }
    }
    return count_;
}

#endif