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

OBJECT : ROCKSDB HIREDIS SLASH redis_cli demo pipe_example duplex_pipe_example popen_example fifo_example fifo_client_example fifo_server_example fifo_main_client_example fifo_main_server_example

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

pipe_example : pipe_example.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

duplex_pipe_example : duplex_pipe_example.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

popen_example : popen_example.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

fifo_example : fifo_example.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

fifo_client_example : fifo_client_example.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

fifo_server_example : fifo_server_example.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

fifo_main_client_example : fifo_main_client_example.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

fifo_main_server_example : fifo_main_server_example.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@  $(INCLUDE_PATH) $(SLASH_LIBRARY)

clean:
	rm -rf redis_cli demo

distclean:
	make -C $(ROCKSDB_PATH) clean 
	make -C $(HIREDIS_PATH) clean
	make -C $(SLASH_PATH)/slash clean
	rm -rf redis_cli
	rm -rf demo
	rm -rf pipe_example
	rm -rf duplex_pipe_example
	rm -rf popen_example 
	rm -rf fifo_example
	rm -rf fifo_client_example
	rm -rf fifo_server_example
	rm -rf fifo_main_client_example
	rm -rf fifo_main_server_example


