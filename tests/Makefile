# Banc de tests natif du programme Arduino (§11.4)
#   make             : compile
#   make run         : compile et execute le banc principal (pupitre LCD)
#   make liaison     : banc de la liaison interface, sans puis avec pupitre
#   make comparaison : historique d'etats identique au programme d'origine
#   make tout        : les trois
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wno-unused-parameter -O0 -g -I. -Imocks
# Les bancs existants actionnent les boutons du pupitre : AVEC_PUPITRE_LCD 1.
PUPITRE   = -DAVEC_PUPITRE_LCD=1
SOURCES   = test_sechoir.cpp mocks/Arduino.cpp
HEADERS   = $(wildcard mocks/*.h) ../sechoir_hybride/sechoir_hybride.ino $(wildcard ../sechoir_hybride/*.h)

all: test_sechoir

test_sechoir: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(PUPITRE) -o $@ $(SOURCES)

run: test_sechoir
	./test_sechoir

# Cycle complet : firmware reel + modele thermique illustratif -> cycle.csv
simulation_cycle: simulation_cycle.cpp mocks/Arduino.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(PUPITRE) -o $@ simulation_cycle.cpp mocks/Arduino.cpp

simulation: simulation_cycle
	./simulation_cycle > cycle.csv
	@echo "cycle.csv ecrit (t_min, T_sec, palier_pct, etat, H_sec, H_fin)"

# Courbes de reference des 16 scenarios Simulink (firmware reel + meme modele
# physique) -> ../matlab/reference/scenario_NN.csv. Necessite Octave ou MATLAB
# (matlab/exporter_scenario.m) ; OCTAVE=... pour changer d'interpreteur.
OCTAVE ?= octave-cli
simulation_scenarios: simulation_scenarios.cpp mocks/Arduino.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(PUPITRE) -o $@ simulation_scenarios.cpp mocks/Arduino.cpp

reference: simulation_scenarios
	mkdir -p ../matlab/reference
	for n in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do \
	  f=$$(printf '../matlab/reference/scenario_%02d' $$n); \
	  $(OCTAVE) --no-gui --quiet --eval "addpath('../matlab'); exporter_scenario($$n, '$$f.txt')" && \
	  ./simulation_scenarios $$f.txt > $$f.csv && rm -f $$f.txt || exit 1; \
	done
	@echo "Courbes de reference ecrites dans matlab/reference/"

# Etudes E1 a E4 du memoire (firmware reel + modele physique etendu) :
# lancees depuis MATLAB/Octave par matlab/etudes/simuler_etude.m (moteur 'programme')
simulation_etudes: simulation_etudes.cpp mocks/Arduino.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(PUPITRE) -O2 -o $@ simulation_etudes.cpp mocks/Arduino.cpp

# Liaison interface operateur (USB / ESP32) : sans pupitre puis avec pupitre
test_liaison_lcd0: test_liaison.cpp mocks/Arduino.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -DAVEC_PUPITRE_LCD=0 -o $@ test_liaison.cpp mocks/Arduino.cpp

test_liaison_lcd1: test_liaison.cpp mocks/Arduino.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -DAVEC_PUPITRE_LCD=1 -o $@ test_liaison.cpp mocks/Arduino.cpp

liaison: test_liaison_lcd0 test_liaison_lcd1
	./test_liaison_lcd0
	./test_liaison_lcd1

# Meme scenario de 90 min : programme d'origine (boutons), programme avec
# liaison (boutons), programme sans pupitre (commandes a distance).
HIST = comparaison_historique.cpp mocks/Arduino.cpp
comparaison: $(HIST) $(HEADERS) reference_origine/sechoir_hybride.ino
	$(CXX) $(CXXFLAGS) -O2 -DVERSION_ORIGINE -o hist_origine $(HIST)
	$(CXX) $(CXXFLAGS) -O2 -DAVEC_PUPITRE_LCD=1 -o hist_pupitre $(HIST)
	$(CXX) $(CXXFLAGS) -O2 -DAVEC_PUPITRE_LCD=0 -DPAR_COMMANDES -o hist_commandes $(HIST)
	./hist_origine > hist_origine.txt
	./hist_pupitre > hist_pupitre.txt
	./hist_commandes > hist_commandes.txt
	diff hist_origine.txt hist_pupitre.txt
	diff hist_origine.txt hist_commandes.txt
	@echo "Historiques identiques ($$(wc -l < hist_origine.txt) changements) : origine = pupitre = commandes a distance"

tout: run liaison comparaison

clean:
	rm -f test_sechoir simulation_cycle cycle.csv simulation_scenarios simulation_etudes
	rm -f test_liaison_lcd0 test_liaison_lcd1 hist_origine hist_pupitre hist_commandes hist_*.txt

.PHONY: all run clean simulation reference liaison comparaison tout
