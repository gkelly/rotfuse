// Copyright 2009 Garret Kelly. All Rights Reserved.
// Author: gkelly@gkelly.org (Garret Kelly)

#define FUSE_USE_VERSION 26
#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse/fuse.h>
#include <fuse/fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#define ROTFUSE_RETURN_ERRNO(_value) { if (_value == -1) {\
    return -errno; \
  } \
  return 0; \
}

enum rotfuse_option_key {
  KEY_HELP,
};

static char *rotfuse_base_path = NULL;
static char rotfuse_lookup_table[0x100];

static void rotfuse_generate_lookup_table() {
  int i;
  for (i = 0; i < sizeof(rotfuse_lookup_table); ++i) {
    rotfuse_lookup_table[i] = i;
  }

  for (i = 0; i < 26; ++i) {
    int offset = (i + 13) % 26;
    rotfuse_lookup_table['A' + i] = 'A' + offset;
    rotfuse_lookup_table['a' + i] = 'a' + offset;
  }
}

static uint8_t *rotfuse_transform_bytes(const uint8_t *input,
    const size_t length) {
  uint8_t *output = malloc(length);

  int i;
  for (i = 0; i < length; ++i) {
    output[i] = rotfuse_lookup_table[(unsigned int)input[i]];
  }

  return output;
}

static char *rotfuse_transform_path(const char *path) {
  char *final_path = malloc(strlen(path) + strlen(rotfuse_base_path) + 1);
  strcpy(final_path, rotfuse_base_path);

  char *transformed_path =
      (char *)rotfuse_transform_bytes((uint8_t *)path, strlen(path) + 1);
  strcat(final_path, transformed_path);
  free(transformed_path);

  return final_path;
}

static int rotfuse_getattr(const char *path, struct stat *attributes) {
  char *transformed_path = rotfuse_transform_path(path);
  int result = lstat(transformed_path, attributes);
  free(transformed_path);

  ROTFUSE_RETURN_ERRNO(result);
}

static int rotfuse_readdir(const char *path, void *buffer,
    fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *file_info) {
  char *transformed_path = rotfuse_transform_path(path);
  DIR *directory = opendir(transformed_path); 
  if (!directory) {
    free(transformed_path);
    return -ENOENT;
  }

  struct dirent *entry;
  while ((entry = readdir(directory))) {
    char *transformed_name =
        (char *)rotfuse_transform_bytes((uint8_t *)entry->d_name,
        strlen(entry->d_name) + 1);
    filler(buffer, transformed_name, NULL, 0);
    free(transformed_name);
  }

  closedir(directory);
  free(transformed_path);
  return 0;
}

static int rotfuse_chmod(const char *path, mode_t mode) {
  char *transformed_path = rotfuse_transform_path(path);
  int return_value = chmod(transformed_path, mode);
  free(transformed_path);

  ROTFUSE_RETURN_ERRNO(return_value);
}

static int rotfuse_chown(const char *path, uid_t user, gid_t group) {
  char *transformed_path = rotfuse_transform_path(path);
  int return_value = chown(transformed_path, user, group); 
  free(transformed_path);

  ROTFUSE_RETURN_ERRNO(return_value);
}

static int rotfuse_open(const char *path, struct fuse_file_info *file_info) {
  char *transformed_path = rotfuse_transform_path(path);
  file_info->fh = open(transformed_path, file_info->flags);
  free(transformed_path);

  ROTFUSE_RETURN_ERRNO(file_info->fh);
}

static int rotfuse_read(const char *path, char *buffer, size_t count,
    off_t offset, struct fuse_file_info *file_info) {
  int read_bytes = pread(file_info->fh, buffer, count, offset);
  
  uint8_t *transformed_bytes =
      rotfuse_transform_bytes((uint8_t *)buffer, read_bytes); 
  memcpy(buffer, transformed_bytes, read_bytes);
  free(transformed_bytes);

  return read_bytes;
}

static int rotfuse_write(const char *path, const char *buffer, size_t count,
    off_t offset, struct fuse_file_info *file_info) {
  uint8_t *transformed_bytes =
      rotfuse_transform_bytes((uint8_t *)buffer, count);
  int written_bytes = pwrite(file_info->fh, transformed_bytes, count, offset);
  free(transformed_bytes);
  
  return written_bytes;
}

static int rotfuse_unlink(const char *path) {
  char *transformed_path = rotfuse_transform_path(path);
  int return_value = unlink(transformed_path);
  free(transformed_path);

  ROTFUSE_RETURN_ERRNO(return_value);
}

static int rotfuse_utimens(const char *path, const struct timespec tv[2]) {
  return 0;
}

static int rotfuse_create(const char *path, mode_t mode,
    struct fuse_file_info *file_info) {
  char *transformed_path = rotfuse_transform_path(path);
  file_info->fh = creat(transformed_path, mode);
  free(transformed_path);

  ROTFUSE_RETURN_ERRNO(file_info->fh);
}

static int rotfuse_mkdir(const char *path, mode_t mode) {
  char *transformed_path = rotfuse_transform_path(path);
  int result = mkdir(transformed_path, mode);
  free(transformed_path);

  ROTFUSE_RETURN_ERRNO(result);
}

static int rotfuse_rmdir(const char *path) {
  char *transformed_path = rotfuse_transform_path(path);
  int result = rmdir(transformed_path);
  free(transformed_path);

  ROTFUSE_RETURN_ERRNO(result);
}

static int rotfuse_rename(const char *from, const char *to) {
  char *from_transformed = rotfuse_transform_path(from);
  char *to_transformed = rotfuse_transform_path(to);
  int result = rename(from_transformed, to_transformed);
  free(from_transformed);
  free(to_transformed);

  ROTFUSE_RETURN_ERRNO(result);
}

static void rotfuse_display_usage() {
  printf("Usage:\n");
  printf("\trotfuse source_path mount_point\n");
}

static struct fuse_opt rotfuse_opts[] = {
  FUSE_OPT_KEY("--help",    KEY_HELP),
  FUSE_OPT_END,
};

static struct fuse_operations rotfuse_operations = {
  .getattr = rotfuse_getattr,
  .readdir = rotfuse_readdir,
  .chmod   = rotfuse_chmod,
  .chown   = rotfuse_chown,
  .open    = rotfuse_open,
  .read    = rotfuse_read,
  .write   = rotfuse_write,
  .unlink  = rotfuse_unlink,
  .utimens = rotfuse_utimens,
  .create  = rotfuse_create,
  .mkdir   = rotfuse_mkdir,
  .rmdir   = rotfuse_rmdir,
  .rename  = rotfuse_rename,
};

static int rotfuse_parse_option(void *data, const char *argument, int key,
    struct fuse_args *output_arguments) {
  switch (key) {
    case FUSE_OPT_KEY_NONOPT:
      if (rotfuse_base_path) {
        return 1;
      }
      rotfuse_base_path = canonicalize_file_name(argument);
      return 0;

    case FUSE_OPT_KEY_OPT:
      return 1;

    case KEY_HELP:
    default:
      rotfuse_display_usage();
      exit(1); 
  }

  return 1;
}

int main(int argc, char **argv) {
  rotfuse_generate_lookup_table();

  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  if (fuse_opt_parse(&args, NULL, rotfuse_opts, rotfuse_parse_option)) {
    fprintf(stderr, "Couldn't parse arguments.\n");
    return 1;
  } else if (rotfuse_base_path == NULL) {
    fprintf(stderr, "You must specify a source path.\n");
    return 1;
  }

  fuse_main(args.argc, args.argv, &rotfuse_operations, NULL);
  return 0;
}
