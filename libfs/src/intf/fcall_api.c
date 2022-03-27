
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syscall.h>
#include <errno.h>
#include <fcntl.h>

#include <posix/posix_interface.h>
#include <global/types.h>
#include <global/util.h>

#define FD_START 1000000 //should be consistent with FD_START in libfs/param.h

#define PATH_BUF_SIZE 4095
#define MLFS_PREFIX (char *)"/mlfs"
#define CMP_LEN 5

static inline int check_mlfs_fd(int fd)
{
  if (fd >= g_fd_start)
    return 1;
  else
    return 0;
}

static inline int get_mlfs_fd(int fd)
{
  if (fd >= g_fd_start)
    return fd - g_fd_start;
  else
    return fd;
}

int det_open(const char *filename, int flags, mode_t mode)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, CMP_LEN) != 0){
    printf("Fell back to open() syscall for %s\n", path_buf);
    printf("strncmp(%s,%s,%d): %d\n", path_buf, MLFS_PREFIX, CMP_LEN, strncmp(path_buf, MLFS_PREFIX, CMP_LEN));
    return open((char*) filename, flags, mode);
  } else {
    ret = mlfs_posix_open((char*) filename, flags, mode);
    if (!check_mlfs_fd(ret)) {
      printf("incorrect fd %d: file %s\n", ret, filename);
    }
    return ret;
  }
}

int det_creat(const char *filename, mode_t mode)
{
  int ret;
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, CMP_LEN) != 0){
    return creat((char*) filename, mode);
  } else {
    ret = mlfs_posix_creat((char*) filename, mode);
    if (!check_mlfs_fd(ret)) {
      printf("incorrect fd %d\n", ret);
    }
    return ret;
  }
}

int det_rename(char *oldname, char *newname)
{
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(oldname, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, CMP_LEN) != 0){
    return rename(oldname, newname); 
  } else {
    return mlfs_posix_rename(oldname, newname);
  }
}

int det_stat(const char *filename, struct stat *statbuf)
{
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(filename, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, CMP_LEN) != 0){
    return stat((char*) filename, statbuf);
  } else {
    return mlfs_posix_stat((char*) filename, statbuf);
  }
}

int det_mkdir(const char *path, mode_t mode)
{
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(path, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, CMP_LEN) != 0){
    return mkdir((char*) path, mode);
  } else {
    return mlfs_posix_mkdir((char*) path, mode);
  }
}

int det_unlink(const char *path)
{
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(path, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, CMP_LEN) != 0){
    return unlink((char*) path);
  } else {
    return mlfs_posix_unlink((char*) path);
  }
}

int det_access(const char *pathname, int mode)
{
  char path_buf[PATH_BUF_SIZE];

  memset(path_buf, 0, PATH_BUF_SIZE);
  collapse_name(pathname, path_buf);

  if (strncmp(path_buf, MLFS_PREFIX, CMP_LEN) != 0){
    return access((char*) pathname, mode);
  } else {
    return mlfs_posix_access((char *)pathname, mode);
  }
}

ssize_t det_read(int fd, void *buf, size_t count)
{
  if (check_mlfs_fd(fd)) {
    return mlfs_posix_read(get_mlfs_fd(fd), buf, count);
  } else {
    return read(fd, buf, count);
  }
}

ssize_t det_pread64(int fd, void *buf, size_t count, loff_t off)
{
  if (check_mlfs_fd(fd)) {
    return mlfs_posix_pread64(get_mlfs_fd(fd), buf, count, off);
  } else {
    return pread(fd, buf, count, off);
  }
}

ssize_t det_write(int fd, void *buf, size_t count)
{
  if (check_mlfs_fd(fd)) {
    return mlfs_posix_write(get_mlfs_fd(fd), buf, count);
  } else {
    return write(fd, buf, count);
  }
}

ssize_t det_pwrite64(int fd, void *buf, size_t count, loff_t off)
{
  if (check_mlfs_fd(fd)) {
    return mlfs_posix_pwrite64(get_mlfs_fd(fd), buf, count, off);
  } else {
    return pwrite(fd, buf, count, off);
  }
}

int det_close(int fd)
{
  if (check_mlfs_fd(fd)) {
    return mlfs_posix_close(get_mlfs_fd(fd));
  } else {
    return close(fd);
  }
}

off_t det_lseek(int fd, off_t offset, int origin)
{
  if (check_mlfs_fd(fd)) {
    return mlfs_posix_lseek(get_mlfs_fd(fd), offset, origin);
  } else {
    return lseek(fd, offset, origin);
  }
}

int det_fsync(int fd)
{
  if (check_mlfs_fd(fd)) {
    return mlfs_posix_fsync(fd);
  } else {
    return fsync(fd);
  }
}

int det_fcntl(int fd, int cmd, void *arg)
{
  if (check_mlfs_fd(fd)) {
    return mlfs_posix_fcntl(get_mlfs_fd(fd), cmd, arg);
  } else {
    return fcntl(fd, cmd, arg);
  }
}
