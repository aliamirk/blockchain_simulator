CXX      = g++
CXXFLAGS = -std=c++17 -O2 -UCPPHTTPLIB_OPENSSL_SUPPORT
MYSQL    = $(shell mysql_config --cflags --libs)
MYSQL_LIB_DIR = $(shell mysql_config --variable=pkglibdir 2>/dev/null || dirname $(shell mysql_config --libs | grep -oE '\-L[^ ]+' | head -1 | sed 's/-L//'))
LIBS     = -lpthread -Wl,-rpath,$(shell mysql_config --libs | grep -oE '\-L[^ ]+' | head -1 | sed 's/-L//')

# API server (for React frontend)
api: blockchain_api.cpp httplib.h json.hpp
	$(CXX) $(CXXFLAGS) blockchain_api.cpp -o blockchain_api $(MYSQL) $(LIBS)

# Original console UI (unchanged)
console: blockchain.cpp
	$(CXX) $(CXXFLAGS) blockchain.cpp -o blockchain $(MYSQL)

clean:
	rm -f blockchain_api blockchain

.PHONY: api console clean
