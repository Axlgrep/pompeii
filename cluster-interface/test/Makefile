CXX=g++
LDFLAGS=-lpthread -lsnappy
CXXFLAGS= -std=c++11 -g

.PHONY : clean all

THIRD_PATH=../../third

HIREDIS_PATH=$(THIRD_PATH)/hiredis-vip
HIREDIS_LIBRARY=$(HIREDIS_PATH)/libhiredis_vip.a

GOOGLETEST_PATH=$(THIRD_PATH)/googletest
GOOGLETEST_LIBRARY=$(GOOGLETEST_PATH)/googletest/libgtest.a

INCLUDE_PATH = -I./                                    \
               -I$(HIREDIS_PATH)                       \
               -I$(GOOGLETEST_PATH)/googletest/include \

LIB_PATH     =


LIBS         =

OBJECTS = HIREDIS GOOGLETEST gtest_exists gtest_unlink gtest_transaction

all: $(OBJECTS)

test: $(OBJECTS)
	@./gtest_exists
	@./gtest_unlink
	@./gtest_transaction

HIREDIS:
	make -C $(HIREDIS_PATH)

GOOGLETEST:
	@cd $(GOOGLETEST_PATH); if [ ! -f ./Makefile ]; then  cmake .; fi; make;

gtest_exists: gtest_exists.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(INCLUDE_PATH) $(HIREDIS_LIBRARY) $(GOOGLETEST_LIBRARY)

gtest_unlink: gtest_unlink.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(INCLUDE_PATH) $(HIREDIS_LIBRARY) $(GOOGLETEST_LIBRARY)

gtest_transaction: gtest_transaction.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(INCLUDE_PATH) $(HIREDIS_LIBRARY) $(GOOGLETEST_LIBRARY)

clean:
	rm -rf gtest_exists
	rm -rf gtest_unlink
	rm -rf gtest_transaction

distclean:
	make -C $(HIREDIS_PATH) clean
	rm -rf gtest_exists
	rm -rf gtest_unlink
	rm -rf gtest_transaction


