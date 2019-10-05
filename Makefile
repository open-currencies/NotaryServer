include ../rocksdb/make_config.mk

ifndef DISABLE_JEMALLOC
	ifdef JEMALLOC
		PLATFORM_CXXFLAGS += -DROCKSDB_JEMALLOC -DJEMALLOC_NO_DEMANGLE
	endif
	EXEC_LDFLAGS := $(JEMALLOC_LIB) $(EXEC_LDFLAGS) -lpthread
	PLATFORM_CXXFLAGS += $(JEMALLOC_INCLUDE)
endif

.PHONY: librocksdb

Release: MKDIR_Release_src MKDIR_bin_Release Database OtherServersHandler RequestProcessor InternalThread RequestBuilder MessageBuilder Main

MKDIR_Release_src:
	mkdir -p obj/Release/src

MKDIR_bin_Release:
	mkdir -p bin/Release

Database: librocksdb src/Database.cpp include/Database.h
	$(CXX) -Wall -Iinclude -I../EntriesHandling/include -I../cryptopp610 -I../rocksdb/include -c src/$@.cpp -o obj/Release/src/$@.o -std=c++11

OtherServersHandler: librocksdb src/OtherServersHandler.cpp include/OtherServersHandler.h
	$(CXX) -Wall -Iinclude -I../EntriesHandling/include -I../cryptopp610 -I../rocksdb/include -c src/$@.cpp -o obj/Release/src/$@.o -std=c++11

RequestProcessor: librocksdb src/RequestProcessor.cpp include/RequestProcessor.h
	$(CXX) -Wall -Iinclude -I../EntriesHandling/include -I../cryptopp610 -I../rocksdb/include -c src/$@.cpp -o obj/Release/src/$@.o -std=c++11

InternalThread: librocksdb src/InternalThread.cpp include/InternalThread.h
	$(CXX) -Wall -Iinclude -I../EntriesHandling/include -I../cryptopp610 -I../rocksdb/include -c src/$@.cpp -o obj/Release/src/$@.o -std=c++11

RequestBuilder: librocksdb src/RequestBuilder.cpp include/RequestBuilder.h
	$(CXX) -Wall -Iinclude -I../EntriesHandling/include -I../cryptopp610 -I../rocksdb/include -c src/$@.cpp -o obj/Release/src/$@.o -std=c++11

MessageBuilder: librocksdb src/MessageBuilder.cpp include/MessageBuilder.h
	$(CXX) -Wall -Iinclude -I../EntriesHandling/include -I../cryptopp610 -I../rocksdb/include -c src/$@.cpp -o obj/Release/src/$@.o -std=c++11

Main: librocksdb main.cpp obj/Release/src/Database.o obj/Release/src/OtherServersHandler.o obj/Release/src/RequestProcessor.o obj/Release/src/InternalThread.o obj/Release/src/RequestBuilder.o obj/Release/src/MessageBuilder.o
	$(CXX) $(CXXFLAGS) main.cpp -o bin/Release/NotaryServer -Iinclude obj/Release/src/Database.o obj/Release/src/OtherServersHandler.o obj/Release/src/RequestProcessor.o obj/Release/src/InternalThread.o obj/Release/src/RequestBuilder.o obj/Release/src/MessageBuilder.o ../EntriesHandling/libEntriesHandling.a -I../EntriesHandling/include ../cryptopp610/libcryptopp.a -I../cryptopp610 ../rocksdb/librocksdb.a -I../rocksdb/include -O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS) -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
