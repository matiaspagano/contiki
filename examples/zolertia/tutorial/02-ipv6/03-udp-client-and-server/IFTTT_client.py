#! /usr/bin/env python

#------------------------------------------------------------#
# UDP example to forward data from a local IPv6 DODAG
# Antonio Lignan <alinan@zolertia.com>
# Install: sudo pip install requests
#------------------------------------------------------------#
import sys
import json
import datetime
from socket import*
from socket import error
from time import sleep
import struct
from ctypes import *
import requests
#------------------------------------------------------------#
ID_STRING      = "V0.1"
#------------------------------------------------------------#
PORT              = 5678
CMD_PORT          = 8765
BUFSIZE           = 1024
#------------------------------------------------------------#
IFTTT_URL         = "https://maker.ifttt.com/trigger/"
IFTTT_EVENT1       = "curtain_up"
IFTTT_EVENT2       = "curtain_down"
IFTTT_KEY         = "b2Fytm1K8HXlxju7Dn7LZOLxQ1NW0e2NmWeHKEf1fh2"
#------------------------------------------------------------#
# Message structure
#------------------------------------------------------------#
class SENSOR(Structure):
    _pack_   = 1
    _fields_ = [
                 ("id",                         c_uint8),
                 ("counter",                    c_uint16),
                 ("light",	                c_int16),
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
    print "{0}:{1} -".format(f_name, getattr(msg, f_name)),
  print
  print "***"
# -----------------------------------------------------------#
def start_client():
  now = datetime.datetime.now()
  print "UDP6 server side application "  + ID_STRING
  print "Started " + str(now)
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
  print "msg structure size: ", sizeof(SENSOR)
  print

  while True:
    data, addr = s.recvfrom(BUFSIZE)
    now = datetime.datetime.now()
    print str(now)[:19] + " -> " + str(addr[0]) + ":" + str(addr[1]) + " " + str(len(data))

    msg_recv = SENSOR(data)
    print_recv_data(msg_recv)

    # Create an empty dictionary and store the values to send
    report = {}
    report["value1"] = msg_recv.light
    report["value2"] = msg_recv.counter

    # Chequeamos que evento se produce, dependiendo de los lumenes detectados    
    event = ''
    if (msg_recv.light > 1000):
      event = IFTTT_EVENT1
    elif(msg_recv.light < 50):
      event = IFTTT_EVENT2
    if (event != ''):
      print "Evento disparado!! "+event+"\n"
      requests.post(IFTTT_URL + event + "/with/key/" + IFTTT_KEY, data=report) 
#------------------------------------------------------------#
# MAIN APP
#------------------------------------------------------------#
if __name__ == "__main__":
  start_client()

