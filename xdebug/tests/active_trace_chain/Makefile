TOP ?= ../../..

include $(TOP)/mk/common_flags.mk
include $(TOP)/mk/npi.mk

CXXFLAGS += $(NPI_CXXFLAGS) -I. -I$(TOP)/xdebug/src
LDFLAGS  += $(NPI_LDFLAGS) -Wl,-rpath,$(NPI_LIB)

BUILD ?= build

.PHONY: all clean chain_test

all: chain_test

chain_test: chain_test.cpp chain_test.h $(TOP)/xdebug/src/core/npi/compat.h $(TOP)/xdebug/src/core/common/unique_resource.h
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $(BUILD)/$@ chain_test.cpp $(LDFLAGS)

clean:
	rm -rf $(BUILD)
