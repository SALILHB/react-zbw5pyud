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

# Cycle complet : firmware reel + modele thermique illustratif -> cycle.csv
simulation_cycle: simulation_cycle.cpp mocks/Arduino.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ simulation_cycle.cpp mocks/Arduino.cpp

simulation: simulation_cycle
	./simulation_cycle > cycle.csv
	@echo "cycle.csv ecrit (t_min, T_sec, palier_pct, etat, H_sec, H_fin)"

clean:
	rm -f test_sechoir simulation_cycle cycle.csv

.PHONY: all run clean simulation
