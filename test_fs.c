#include "filesystem/filesystem.h"
#include <stdio.h>
int main() {
    filesystem_initialize();
    printf("is_dir(/): %d\n", filesystem_is_directory("/"));
    printf("is_dir(/run): %d\n", filesystem_is_directory("/run"));
    printf("is_dir(/run/home): %d\n", filesystem_is_directory("/run/home"));
    return 0;
}
