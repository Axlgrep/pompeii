CXX=g++
LDFLAGS=-lpthread -lsnappy
CXXFLAGS=-std=c++11 -static-libstdc++ -g

.PHONY : clean all

THIRD_PATH=../../third

HIREDIS_PATH=$(THIRD_PATH)/hiredis-vip
HIREDIS_LIBRARY=$(HIREDIS_PATH)/libhiredis_vip.a
INCLUDE_PATH = -I./                        \
							 -I$(HIREDIS_PATH)           \

LIB_PATH     =


LIBS         =

OBJECT : HIREDIS client

all: $(OBJECT)

HIREDIS:
	make -C $(HIREDIS_PATH)

client: client.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(HIREDIS_LIBRARY)

clean:
	rm -rf client

distclean:
	make -C $(HIREDIS_PATH) clean
	rm -rf client


