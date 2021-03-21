#!make
# This make needs:
# g++
#

include ../MakeHelper

#The Directories, Source, Includes, Objects, Binary and Resources
BUILDDIR	?= $(CURDIR)/obj/$(BUILD)
TARGETDIR	?= $(CURDIR)/bin/$(BUILD)

#Compiler and Linker
BUILD	?= release
BITS	?= 64
CC		:= g++

#Flags, Libraries and Includes
#CFLAGS.common	:= -std=c++17 -m$(BITS) -Wall -Wextra
CFLAGS.common   := -std=c++17 -Wall -Wextra
CFLAGS.debug 	:= $(CFLAGS.common) -g
CFLAGS.release	:= $(CFLAGS.common) -Werror -O3
CFLAGS			?= $(CFLAGS.$(BUILD))
LIB		:= -L$(TARGETDIR) -leasysocket -lpthread -Wl,-rpath='$${ORIGIN}'
INC		:= -I$(CURDIR)/../easysocket/include

build:
	(cd ../easysocket && make TARGETDIR="$(TARGETDIR)")
	$(CC) $(CFLAGS) $(INC) scslog.c  -o $(TARGETDIR)/$(call MakeExe,scslog) $(LIB)
	$(CC) $(CFLAGS) $(INC) scstcp.c  -o $(TARGETDIR)/$(call MakeExe,scstcp) $(LIB)
	$(CC) $(CFLAGS) $(INC) scsmonitor.c  -o $(TARGETDIR)/$(call MakeExe,scsmonitor) $(LIB)
	$(CC) $(CFLAGS) $(INC) scsfirmware.c  -o $(TARGETDIR)/$(call MakeExe,scsfirmware) $(LIB)
	$(CC) $(CFLAGS) $(INC) scsgate_x.c scs_mqtt.c scs_hue.c -lpaho-mqtt3c -o $(TARGETDIR)/$(call MakeExe,scsgate_x) $(LIB)
	$(CC) $(CFLAGS) $(INC) scsgate_y.c scs_mqtt_y.c scs_hue.c -lpaho-mqtt3c -o $(TARGETDIR)/$(call MakeExe,scsgate_y) $(LIB)
	$(CC) $(CFLAGS) $(INC) scsdiscover.c scs_mqtt_disc.c -lpaho-mqtt3c -o $(TARGETDIR)/$(call MakeExe,scsdiscover) $(LIB)

template:
	$(call MD,$(TARGETDIR))
	$(call MD,$(BUILDDIR))

clean:
	$(call RM,$(BUILDDIR))

cleaner: clean
	$(call RM,$(TARGETDIR))

clean-deps: clean
	(cd ../easysocket && make clean)

cleaner-deps: cleaner
	(cd ../easysocket && make cleaner)

.PHONY: build template clean cleaner clean-deps cleaner-deps
