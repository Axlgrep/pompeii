CXX=g++
LDFLAGS=-lpthread -lsnappy
CXXFLAGS=-std=c++11

.PHONY : clean all

THIRD_PATH=../third

ROCKSDB_PATH=$(THIRD_PATH)/rocksdb
ROCKSDB_LIBRARY=$(ROCKSDB_PATH)/librocksdb.a

HIREDIS_PATH=$(THIRD_PATH)/hiredis-vip
HIREDIS_LIBRARY=$(HIREDIS_PATH)/libhiredis_vip.a

INCLUDE_PATH = -I./                        \
						 	 -I$(ROCKSDB_PATH)/include   \
							 -I$(HIREDIS_PATH)           \

LIB_PATH     =


LIBS         =

OBJECT : ROCKSDB HIREDIS redis_cli 

all: $(OBJECT)

ROCKSDB:
	$(AM_V_at)make -j $(PROCESSOR_NUMS) -C $(ROCKSDB_PATH)/ static_lib

HIREDIS:
	make -C $(HIREDIS_PATH)

redis_cli: redis_cli.cc
	$(CXX) $(CXXFLAGS) $^ -o $@  $(INCLUDE_PATH) $(HIREDIS_LIBRARY)

clean:
	make -C $(ROCKSDB_PATH) clean 
	make -C $(HIREDIS_PATH) clean
	rm -rf redis_cli


