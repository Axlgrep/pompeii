CXX=g++
LDFLAGS=-lpthread -lsnappy
CXXFLAGS=-std=c++11

.PHONY : clean all

THIRD_PATH=../third

ROCKSDB_PATH=$(THIRD_PATH)/rocksdb
ROCKSDB_LIBRARY=$(ROCKSDB_PATH)/librocksdb.a

HIREDIS_PATH=$(THIRD_PATH)/hiredis-vip
HIREDIS_LIBRARY=$(HIREDIS_PATH)/libhiredis_vip.a

SLASH_PATH=$(THIRD_PATH)/slash
SLASH_LIBRARY=$(SLASH_PATH)/slash/lib/libslash.a

INCLUDE_PATH = -I./                        \
						 	 -I$(ROCKSDB_PATH)/include   \
							 -I$(HIREDIS_PATH)           \
							 -I$(SLASH_PATH)             \

LIB_PATH     =


LIBS         =

OBJECT : ROCKSDB HIREDIS SLASH redis_cli demo

all: $(OBJECT)

ROCKSDB:
	$(AM_V_at)make -j $(PROCESSOR_NUMS) -C $(ROCKSDB_PATH)/ static_lib

HIREDIS:
	make -C $(HIREDIS_PATH)

SLASH:
	$(AM_V_at)make -C $(SLASH_PATH)/slash

redis_cli: redis_cli.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(HIREDIS_LIBRARY)

demo : demo.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

clean:
	rm -rf redis_cli demo

distclean:
	make -C $(ROCKSDB_PATH) clean 
	make -C $(HIREDIS_PATH) clean
	make -C $(SLASH_PATH)/slash clean
	rm -rf redis_cli
	rm -rf demo


