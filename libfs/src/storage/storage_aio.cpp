
#include <stdio.h>

#include "storage/aio/async.h"
#include "storage/aio/buffer.h"
#include "storage/storage.h"

#include <fcntl.h>

// only ever called once.
static AIO::AIO* controls[1+g_n_devices];

extern "C" uint8_t* mlfs_aio_init(uint8_t dev, char *dev_path) {
  int fd = open(dev_path, O_RDWR | O_DIRECT);
  size_t block_size = 4096;
  if (fd < 0) {
    perror("cannot open device");
    exit(-1);
  }
  int result = 1; // ioctl(fd, _IO(0x12, 104), &block_size); // BLKSSZGET
  if (result < 0) {
    std::cout << "Cannot get block size " << result << std::endl;
    exit(-1);
  }
  printf("About to perform emplace\n");
  if(controls[dev] == nullptr){
    controls[dev] = new AIO::AIO(fd, block_size);
  }
  return nullptr;
}

extern "C" int mlfs_aio_read(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size) {
  int ret = controls[dev]->pread(buf, io_size, blockno << g_block_size_shift);
  return ret;
}

extern "C" int mlfs_aio_write(uint8_t dev, uint8_t *buf, addr_t blockno, uint32_t io_size) {
  return controls[dev]->pwrite(buf, io_size, blockno << g_block_size_shift);
}

extern "C" int mlfs_aio_commit(uint8_t dev) {
  return controls[dev]->commit();
}

extern "C" int mlfs_aio_erase(uint8_t dev, addr_t blockno, uint32_t io_size) {
  return controls[dev]->trim_block(blockno, io_size);
}

extern "C" int mlfs_aio_readahead(uint8_t dev, addr_t blockno, uint32_t io_size) {
  // TODO
  return controls[dev]->readahead(blockno, io_size);
}


extern "C" int mlfs_aio_wait_io(uint8_t dev, int read) {
  return controls[dev]->wait();
}

extern "C" void mlfs_aio_exit(uint8_t dev) {
  int fd = controls[dev]->file_descriptor();
  controls[dev] = nullptr;
  close(fd);
}
