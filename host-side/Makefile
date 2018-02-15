prefix = $(shell pwd)

PROJ_TOPDIR ?= $(prefix)

ifeq "$(PLATFORM)" ""
  PLATFORM = "UNIX"
endif

SDK_PATH = /usr/local/angstrom/arm
PATH_ENV = $(SDK_PATH)/bin

include $(PROJ_TOPDIR)/Makedefs

LDFLAGS += -Wl,--start-group
LDFLAGS += -lusb-1.0 -lpthread -lrt
LDFLAGS += -Wl,--end-group

SRCS= hostside.c

TARGET=host-side

all: $(TARGET) doc

include $(PROJ_TOPDIR)/Makefile.rules

$(OBJS): Makefile

$(TARGET): clean $(TARGET).bin

doc: README 

README: README.texi
	makeinfo --plaintext $< > $@

-include $(OBJS:.o=.d) 

.PHONY: doc clean all 



