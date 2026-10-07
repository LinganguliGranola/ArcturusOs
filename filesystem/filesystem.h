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

/* Buffer-oriented file operations for in-memory editing. */
#define FS_MAX_FILE_SIZE (32U * 512U)
bool filesystem_file_exists(const char *path);
uint32_t filesystem_file_size(const char *path);
bool filesystem_load_file_data(const char *path, uint8_t *buffer,
                               uint32_t capacity, uint32_t *size_out);
bool filesystem_save_file_data(const char *path, const uint8_t *buffer,
                               uint32_t size);

#endif
