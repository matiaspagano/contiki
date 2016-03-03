all: 03-udp-client

# Linker optimizations
SMALL = 1

# Enable to pull-in Z1SP specific test/source files
ZOLERTIA_Z1SP=0

CONTIKI_SOURCEFILES += light-ziglet.c

# Includes the project-conf configuration file
CFLAGS += -DPROJECT_CONF_H=\"../project-conf.h\"

CONTIKI = ../../../../..

# This flag includes the IPv6 libraries
CONTIKI_WITH_IPV6 = 1

include $(CONTIKI)/Makefile.include
