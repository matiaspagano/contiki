#! /usr/bin/env python

#------------------------------------------------------------#
# UDP example to forward data from a local IPv6 network
# Antonio Lignan <antonio.lignan@gmail.com>
#------------------------------------------------------------#
from __future__ import division

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
PORT              = 5678
CMD_PORT          = 8765
BUFSIZE           = 1024
#------------------------------------------------------------#
URL_PWS_REF       = "http://api.wunderground.com/api/xxx/conditions/q/pws:ICERDANY6.json"
#------------------------------------------------------------#
WITH_UBIDOTS      = 1

if WITH_UBIDOTS:
  from ubidots import ApiClient
  UBIDOTS_API       = "xxx"
  UBI_COUNTER       = "xxx"
  UBI_TEMP          = "xxx"
  UBI_HUM           = "xxx"
  UBI_PRES          = "xxx"
  UBI_WIND_SP       = "xxx"
  UBI_WIND_SP_AVG   = "xxx"
  UBI_WIND_SP_INT   = "xxx"
  UBI_WIND_SP_MAX   = "xxx"
  UBI_WIND_DIR      = "xxx"
  UBI_WIND_DIR_INT  = "xxx"
  UBI_RAIN          = "xxx"

  UBI_ICERDANY6_TEMP = "xxx"
  UBI_ICERDANY6_HUM  = "xxx"
  UBI_ICERDANY6_PRES = "xxx"
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

  # Use Ubidots for testing the deployment
  if WITH_UBIDOTS:
    try:
      ubidots = ApiClient(UBIDOTS_API)
    except Exception:
      print "ERROR: Failed to retrieve credentials"
      return

  while True:
    data, addr = s.recvfrom(BUFSIZE)
    now = datetime.datetime.now()
    print str(now)[:19] + " -> " + str(addr[0]) + ":" + str(addr[1]) + " " + str(len(data))

    ws = WEATHER(data)
    # print_recv_data(ws)

    # Read reference data from PWS nearby (ICERDANY6)
    # pws = requests.get(URL_PWS_REF).json()
    # pws_temp  = float(pws['current_observation']['temp_c'])
    # pws_hum   = float(pws['current_observation']['relative_humidity'][:-1])
    # pws_pres  = float(pws['current_observation']['pressure_mb'])

    if WITH_UBIDOTS:
      ubidots.save_collection([
         { "variable" : UBI_COUNTER, "value" : ws.counter },
         { "variable" : UBI_TEMP, "value" : ws.temperature / 100 },
         { "variable" : UBI_HUM, "value" : ws.humidity / 100 },
         { "variable" : UBI_PRES, "value" : ws.atmospheric_pressure / 10 },
         { "variable" : UBI_RAIN, "value" : ws.rain_mm * 0.2794 },
         { "variable" : UBI_WIND_DIR, "value" : ws.wind_dir / 10 },
         { "variable" : UBI_WIND_DIR_INT, "value" : ws.wind_dir_avg_int / 10 },
         { "variable" : UBI_WIND_SP, "value" : ws.wind_speed },
         { "variable" : UBI_WIND_SP_AVG, "value" : ws.wind_speed_avg },
         { "variable" : UBI_WIND_SP_INT, "value" : ws.wind_speed_avg_int },
         { "variable" : UBI_WIND_SP_MAX, "value" : ws.wind_speed_max },
         # Reference values using a PWS accross the street
         # { "variable" : UBI_ICERDANY6_TEMP, "value" : pws_temp },
         # { "variable" : UBI_ICERDANY6_HUM, "value" : pws_hum },
         # { "variable" : UBI_ICERDANY6_PRES, "value" : pws_pres },
       ])

#------------------------------------------------------------#
# MAIN APP
#------------------------------------------------------------#
if __name__ == "__main__":
  start_server()

