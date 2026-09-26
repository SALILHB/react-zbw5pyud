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

# Courbes de reference des 12 scenarios Simulink (firmware reel + meme modele
# physique) -> ../matlab/reference/scenario_NN.csv. Necessite Octave ou MATLAB
# (matlab/exporter_scenario.m) ; OCTAVE=... pour changer d'interpreteur.
OCTAVE ?= octave-cli
simulation_scenarios: simulation_scenarios.cpp mocks/Arduino.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ simulation_scenarios.cpp mocks/Arduino.cpp

reference: simulation_scenarios
	mkdir -p ../matlab/reference
	for n in 1 2 3 4 5 6 7 8 9 10 11 12; do \
	  f=$$(printf '../matlab/reference/scenario_%02d' $$n); \
	  $(OCTAVE) --no-gui --quiet --eval "addpath('../matlab'); exporter_scenario($$n, '$$f.txt')" && \
	  ./simulation_scenarios $$f.txt > $$f.csv && rm -f $$f.txt || exit 1; \
	done
	@echo "Courbes de reference ecrites dans matlab/reference/"

clean:
	rm -f test_sechoir simulation_cycle cycle.csv simulation_scenarios

.PHONY: all run clean simulation reference
