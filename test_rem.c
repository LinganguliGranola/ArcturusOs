#include <stdio.h>
#include <string.h>

void remove_last_component(char *path, size_t *length) {
    if (*length <= 1)
        return;
    while (*length > 0 && path[*length - 1] != '/')
        (*length)--;
    if (*length > 1)
        (*length)--;
    else if (*length == 1 && path[0] != '/')
        (*length) = 0;
    path[*length] = '\0';
}
int main() {
    char path[100] = "run/home";
    size_t len = strlen(path);
    remove_last_component(path, &len);
    printf("run/home -> '%s'\n", path);
    remove_last_component(path, &len);
    printf("run -> '%s'\n", path);
    return 0;
}
