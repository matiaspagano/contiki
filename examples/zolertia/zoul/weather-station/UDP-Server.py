#! /usr/bin/env python

#------------------------------------------------------------#
# UDP example to forward data from a local IPv6 network
# Antonio Lignan <antonio.lignan@gmail.com>
#------------------------------------------------------------#
import sys
import json
import datetime
from socket import*
from socket import error
from time import sleep
import struct
from ctypes import *
#------------------------------------------------------------#
PORT              = 5678
CMD_PORT          = 8765
BUFSIZE           = 1024
#------------------------------------------------------------#
# Message structure
#------------------------------------------------------------#
class WEATHER(Structure):
    _pack_   = 1
    _fields_ = [
                 ("counter",                    c_uint16),
                 ("temperature",                c_uint16),
                 ("humidity",                   c_uint16),
                 ("atmospheric_pressure",       c_uint16),
                 ("wind_speed",                 c_uint16),
                 ("wind_dir",                   c_uint16),
                 ("rain_mm",                    c_uint16),
                 ("wind_dir_avg_int",           c_uint16),
                 ("wind_speed_avg",             c_uint16),
                 ("wind_speed_avg_int",         c_uint16),
                 ("wind_speed_max",             c_uint16),
               ]

    def __new__(self, socket_buffer):
        return self.from_buffer_copy(socket_buffer)

    def __init__(self, socket_buffer):
        pass
#------------------------------------------------------------#
# Helper functions
#------------------------------------------------------------#
def print_recv_data(msg):
  print "***"
  for f_name, f_type in msg._fields_:
    print "{0}:{1}".format(f_name, getattr(msg, f_name))
  print
  print "***"
# -----------------------------------------------------------#
# UDP6 server
#------------------------------------------------------------#
def start_server():
  now = datetime.datetime.now()
  print "UDP6 server side application - " + str(now)
  try:
    s = socket(AF_INET6, SOCK_DGRAM)
    s.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)

    # Replace address below with "aaaa::1" if tunslip6 has
    # created a tun0 interface with this address
    s.bind(('', PORT))

  except Exception:
    print "ERROR: Server Port Binding Failed"
    return
  print 'UDP server ready: %s'% PORT
  print

  while True:
    data, addr = s.recvfrom(BUFSIZE)
    now = datetime.datetime.now()
    print str(now)[:19] + " -> " + str(addr[0]) + ":" + str(addr[1]) + " " + str(len(data))

    msg_recv = WEATHER(data)
    print_recv_data(msg_recv)

#------------------------------------------------------------#
# MAIN APP
#------------------------------------------------------------#
if __name__ == "__main__":
  start_server()

