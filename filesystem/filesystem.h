#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdbool.h>
#include <stdint.h>

/* Discover disks and load (or create) their persistent ArcturusFS volumes. */
void filesystem_initialize(void);

/* A removable USB mass-storage device is mounted at run/exdrive when present. */
void filesystem_attach_exdrive(void);

/* Terminal-facing filesystem operations.  Paths begin with run/<mount>. */
bool filesystem_list(const char *path);
bool filesystem_is_directory(const char *path);
bool filesystem_make_directory(const char *path);
bool filesystem_create_file(const char *path);
bool filesystem_write_file(const char *path, const char *text, bool append);
bool filesystem_read_file(const char *path);
bool filesystem_remove(const char *path);
void filesystem_print_mounts(void);

#endif
