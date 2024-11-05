##############################################################################
#
# Simple Makefile example
#
# This Makefile will initiate compilation of all *.cpp files 
# in current directory into individual executables. 
#
##############################################################################

# Sources *.cpp can be changed to list of individual files
SOURCES=$(wildcard *.cpp)
OBJS=$(SOURCES:%.cpp=%.o)
TARGETS=$(SOURCES:%.cpp=%)

CPPFLAGS += -g -pthread -std=c++11 -Wall 
LDFLAGS += -pthread
LDLIBS += -lrt

.PHONY: all clean 

all: $(TARGETS)

%.o: %.cpp
	#g++ kompilace s tabulátorem (správně)
	g++ -c $(CPPFLAGS) $^ -o $@

$(TARGETS): %: %.o
	#g++ linking s tabulátorem (správně)
	g++ $(LDFLAGS) $^ $(LDLIBS) -o $@ 

clean:
	#Příkazy v clean také s tabulátorem
	rm -rf *.o $(TARGETS)
