
#ifndef _DLC_CLIENT_H_
#define _DLC_CLIENT_H_

void handle_read(size_t inum, size_t count, size_t offset, uint8_t* buf);
void handle_evict(size_t inum, uint8_t* buf, size_t count, size_t offset); 

#endif
