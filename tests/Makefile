# Banc de tests natif du programme Arduino (§11.4)
#   make        : compile
#   make run    : compile et execute
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wno-unused-parameter -O0 -g -I. -Imocks
SOURCES   = test_sechoir.cpp mocks/Arduino.cpp
HEADERS   = $(wildcard mocks/*.h) ../sechoir_hybride/sechoir_hybride.ino

all: test_sechoir

test_sechoir: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES)

run: test_sechoir
	./test_sechoir

clean:
	rm -f test_sechoir

.PHONY: all run clean
