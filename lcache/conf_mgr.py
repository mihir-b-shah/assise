#!/usr/bin/env python3

import threading
import sys
import os
from socket import *
import time
import binascii

server_port = 37673
server_socket = socket(AF_INET, SOCK_DGRAM)
server_socket.bind(('', server_port))

active = {}
map_lock = threading.Lock()

class NodeState:
    def __init__(self, ts):
        self.last_ts = ts

# this is a snapshot, and as such it can change.
# not finding a block is a performance degradation, not a safety violation.
# currently, no state.

def end():
    server_socket.close()
    os._exit(0)

def server():
    print("Waiting on port %d"%(server_port))
    while True:
        message, addr = server_socket.recvfrom(2048)
        map_lock.acquire()

        if message[0] == 0x41:      # A for application node
            '''
            send current config
            number of nodes is small enough to send the whole thing
            a better system would send a delta, or something.    
            '''
            send_msg = b''.join(map(lambda ip_addr : inet_aton(ip_addr.decode()), active.keys()))
            print('appl node, sending ', str(bytearray(send_msg)))
            server_socket.sendto(send_msg, addr)

        elif message[0] == 0x43:    # C for cache node
            ib_addr = message[1:]
            active[ib_addr] = time.time()
            print('received heartbeat from %s'%(ib_addr))
            # send regulatory message, if necessary
        else:
            print('Unknown msg received.')
            # releasing lock doesn't matter.
            end()

        map_lock.release()

def keyboard_input():
    while True:
        key_input = sys.stdin.readline()
        if(not key_input):
            end()

TIMEOUT = 5
def timeout_handler():
    map_lock.acquire()
    cur_time = time.time()

    gone = []
    for addr, ns in active.items():
        if cur_time - ns.last_ts > TIMEOUT:
            gone.append(addr)
    # two step bc of del-while-iter
    for gone_addr in gone:
        del active[gone_addr]
    map_lock.release()

def run():
    timer_thread = threading.Thread(target=timeout_handler)
    timer_thread.start()
    server_thread = threading.Thread(target=server)
    server_thread.start()
    keyboard_input()
    server_thread.join()

run()
