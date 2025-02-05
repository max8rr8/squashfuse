#include "nonstd.h"
#include "squashfs_fs.h"
#include "squashfuse.h"
#include "stat.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


#define PROGNAME "squashfuse_extract"

#define ERR_MISC	(1)
#define ERR_USAGE	(2)
#define ERR_OPEN	(3)

static void usage() {
    fprintf(stderr, "Usage: %s ARCHIVE PATH_TO_EXTRACT\n", PROGNAME);
    fprintf(stderr, "       %s ARCHIVE -a\n", PROGNAME);
    exit(ERR_USAGE);
}

static void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(ERR_MISC);
}

static bool starts_with(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
    lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

int main(int argc, char *argv[]) {
    sqfs_err err = SQFS_OK;
    sqfs_traverse trv;
    sqfs fs;
    char *image;
    char *path_to_extract;
    char *prefix;
    char prefixed_path_to_extract[1024];
    struct stat st;
    
    prefix = "squashfs-root/";
    
    if (access(prefix, F_OK ) == -1 ) {
        if (mkdir(prefix, 0777) == -1) {
            perror("mkdir error");
            exit(EXIT_FAILURE);
        }
    }
    
    if (argc != 3)
        usage();
    image = argv[1];
    path_to_extract = argv[2];
    
    if ((err = sqfs_open_image(&fs, image, 0, NULL)))
        exit(ERR_OPEN);
    
    if ((err = sqfs_traverse_open(&trv, &fs, sqfs_inode_root(&fs))))
        die("sqfs_traverse_open error");
    while (sqfs_traverse_next(&trv, &err)) {
        if (!trv.dir_end) {
            if ((starts_with(path_to_extract, trv.path) != 0) || (strcmp("-a", path_to_extract) == 0)){
                fprintf(stderr, "trv.path: %s\n", trv.path);
                fprintf(stderr, "sqfs_inode_id: %llu\n", (unsigned long long)trv.entry.inode);
                sqfs_inode inode;
                if (sqfs_inode_get(&fs, &inode, trv.entry.inode))
                    die("sqfs_inode_get error");
                fprintf(stderr, "inode.base.inode_type: %i\n", inode.base.inode_type);
                fprintf(stderr, "inode.xtra.reg.file_size: %llu\n", (unsigned long long)inode.xtra.reg.file_size);
                strcpy(prefixed_path_to_extract, "");
                strcat(strcat(prefixed_path_to_extract, prefix), trv.path);
                if (inode.base.inode_type == SQUASHFS_DIR_TYPE){
                    fprintf(stderr, "inode.xtra.dir.parent_inode: %ui\n", inode.xtra.dir.parent_inode);
                    fprintf(stderr, "mkdir: %s/\n", prefixed_path_to_extract);
                    if (access(prefixed_path_to_extract, F_OK ) == -1 ) {
                        if (mkdir(prefixed_path_to_extract, 0777) == -1) {
                            perror("mkdir error");
                            exit(1);
                        }
                    }
                } else if (inode.base.inode_type == SQUASHFS_REG_TYPE){
                    fprintf(stderr, "Extract to: %s\n", prefixed_path_to_extract);
                    if (sqfs_stat(&fs, &inode, &st) != 0)
                        die("sqfs_stat error");
                    printf("Permissions: ");
                    printf( (S_ISDIR(st.st_mode)) ? "d" : "-");
                    printf( (st.st_mode & S_IRUSR) ? "r" : "-");
                    printf( (st.st_mode & S_IWUSR) ? "w" : "-");
                    printf( (st.st_mode & S_IXUSR) ? "x" : "-");
                    printf( (st.st_mode & S_IRGRP) ? "r" : "-");
                    printf( (st.st_mode & S_IWGRP) ? "w" : "-");
                    printf( (st.st_mode & S_IXGRP) ? "x" : "-");
                    printf( (st.st_mode & S_IROTH) ? "r" : "-");
                    printf( (st.st_mode & S_IWOTH) ? "w" : "-");
                    printf( (st.st_mode & S_IXOTH) ? "x" : "-");
                    printf("\n");
        
                    // Read the file in chunks
                    off_t bytes_already_read = 0;
                    sqfs_off_t bytes_at_a_time = 64*1024;
                    FILE * f;
                    f = fopen (prefixed_path_to_extract, "w+");
                    if (f == NULL)
                        die("fopen error");
                    while (bytes_already_read < inode.xtra.reg.file_size)
                    {
                        char buf[bytes_at_a_time];
                        if (sqfs_read_range(&fs, &inode, (sqfs_off_t) bytes_already_read, &bytes_at_a_time, buf))
                            die("sqfs_read_range error");
                        // fwrite(buf, 1, bytes_at_a_time, stdout);
                        fwrite(buf, 1, bytes_at_a_time, f);                 
                        bytes_already_read = bytes_already_read + bytes_at_a_time;
                    }
                    fclose(f);
                    chmod (prefixed_path_to_extract, st.st_mode);
                } else if (inode.base.inode_type == SQUASHFS_SYMLINK_TYPE){
                    size_t size = strlen(trv.path)+1;
                    char buf[size];
                    int ret = sqfs_readlink(&fs, &inode, buf, &size);
                    if (ret != 0)
                        die("sqfs_readlink error");
                    fprintf(stderr, "Symlink: %s to %s \n", prefixed_path_to_extract, buf);
                    unlink(prefixed_path_to_extract);
                    ret = sqfs_symlink(buf, prefixed_path_to_extract);
                    if (ret != 0)
                        die("symlink error");
                } else {
                    fprintf(stderr, "TODO: Implement inode.base.inode_type %i\n", inode.base.inode_type);
                }
                fprintf(stderr, "\n");
            }
        }
    }
    if (err)
        die("sqfs_traverse_next error");
    sqfs_traverse_close(&trv);
    sqfs_fd_close(fs.fd);
    return 0;
}
