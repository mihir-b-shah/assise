
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <assert.h>

/*
 * Right now, we just assert syscalls succeed, and there's a device.
 * Documentation: https://man7.org/linux/man-pages/man7/rdma_cm.7.html
 */

int main(){
  int num_devices;
  struct ibv_context** devices = rdma_get_devices(&num_devices);
  assert(num_devices >= 1);
  
  struct ibv_context* context = devices[0];
  printf("VNIC %s detected.\n", ibv_get_device_name(context->device));

  return 0;
}
