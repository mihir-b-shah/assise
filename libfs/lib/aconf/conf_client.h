
#ifndef _CONF_CLIENT_H_
#define _CONF_CLIENT_H_

#include <pthread.h>
#include <stdint.h>
#include <netinet/in.h>

struct config {
  volatile int n;
  volatile int version;
  struct in_addr* ips;
  pthread_mutex_t mutex;
};
typedef struct config config_t;

struct conf_cmd {
  union {
    uint8_t type;
  } cmd;
  pthread_mutex_t mutex;
};
typedef struct conf_cmd conf_cmd_t;

void init_conf(config_t* conf);
void init_cmd(conf_cmd_t* cmd);

void start_appl_client(config_t* conf);
void start_cache_client(conf_cmd_t* cmd);

#endif
