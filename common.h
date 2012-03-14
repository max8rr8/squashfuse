#ifndef SQFS_COMMON_H
#define SQFS_COMMON_H

#define _POSIX_C_SOURCE 200112L

#include <stdint.h>
#include <sys/types.h>

typedef enum {
	SQFS_OK,
	SQFS_ERR,
	SQFS_FORMAT,
} sqfs_err;

#define SQFS_INODE_ID_BYTES 6
typedef uint64_t sqfs_inode_id;

typedef struct sqfs sqfs;
typedef struct sqfs_inode sqfs_inode;

typedef struct {
	size_t size;
	void *data;
} sqfs_block;

typedef struct {
	off_t block;
	size_t offset;
} sqfs_md_cursor;

#endif
