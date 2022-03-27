
#ifndef _FCALL_API_H_
#define _FCALL_API_H_

#include <sys/stat.h>
#include <global/types.h>

extern "C" {

int det_open(const char *filename, int flags, mode_t mode);
int det_creat(const char *filename, mode_t mode);
ssize_t det_read(int fd, void *buf, size_t count);
ssize_t det_pread64(int fd, void *buf, size_t count, loff_t off);
ssize_t det_write(int fd, void *buf, size_t count);
ssize_t det_pwrite64(int fd, void *buf, size_t count, loff_t off);
int det_close(int fd);
off_t det_lseek(int fd, off_t offset, int origin);
int det_rename(char *oldname, char *newname);
int det_stat(const char *filename, struct stat *statbuf);
int det_mkdir(const char *path, mode_t mode);
int det_unlink(const char *path);
int det_access(const char *pathname, int mode);
int det_fsync(int fd);
int det_fcntl(int fd, int cmd, void *arg);

}

#endif
