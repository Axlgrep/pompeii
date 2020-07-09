CXX=g++
LDFLAGS=-lpthread -lsnappy -levent
CXXFLAGS= -std=c++11 -g

.PHONY : clean all

THIRD_PATH=../../third

HIREDIS_PATH=$(THIRD_PATH)/hiredis-vip
HIREDIS_LIBRARY=$(HIREDIS_PATH)/libhiredis_vip.a

INCLUDE_PATH = -I./                                    \
               -I$(HIREDIS_PATH)                       \

LIB_PATH     =


LIBS         =

OBJECTS = HIREDIS replay subscribe_client publish_client

all: $(OBJECTS)

test: $(OBJECTS)
	@./replay

HIREDIS:
	make -C $(HIREDIS_PATH)

replay: replay.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(INCLUDE_PATH) $(HIREDIS_LIBRARY) $(GOOGLETEST_LIBRARY)

subscribe_client: subscribe_client.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(INCLUDE_PATH) $(HIREDIS_LIBRARY) $(GOOGLETEST_LIBRARY)

publish_client: publish_client.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(INCLUDE_PATH) $(HIREDIS_LIBRARY) $(GOOGLETEST_LIBRARY)

clean:
	rm -rf replay
	rm -rf subscribe_client
	rm -rf publish_client

distclean:
	make -C $(HIREDIS_PATH) clean
	rm -rf replay
	rm -rf subscribe_client
	rm -rf publish_client


