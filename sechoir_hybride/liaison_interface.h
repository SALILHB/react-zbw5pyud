/* ============================================================================
 *  LIAISON INTERFACE OPÉRATEUR (PC / téléphone) — déclarations
 *
 *  Inclus en tête de sechoir_hybride.ino ; la mise en œuvre est dans
 *  liaison_interface_impl.h, incluse à la FIN du .ino (elle a besoin des
 *  variables et fonctions `static` du programme : un .cpp séparé ne pourrait
 *  pas y accéder).
 *
 *  Le Mega reste le seul automate. L'interface envoie des demandes, traduites
 *  en appuis de boutons virtuels consommés par frontPris(), exactement comme
 *  les boutons du pupitre. Voir docs/PROTOCOLE.md.
 *
 *  JOURNAL : les Serial.print/println du programme sont redirigés vers un
 *  petit tampon (journalSerie). La liaison les réémet ligne par ligne entre
 *  deux trames JSON, sur l'USB et sur Serial1 (ESP32) : une ligne de débogage
 *  ne peut jamais couper une trame, et aucune émission ne bloque loop().
 * ==========================================================================*/
#pragma once

#include <Arduino.h>

/* Port USB réel, capturé AVANT la redirection de `Serial` ci-dessous. */
typedef decltype(Serial) PortSerie;
static PortSerie& PORT_USB = Serial;

/* --- Tampon du journal : lignes complètes uniquement (FIFO de caractères) --- */
static const uint8_t TAILLE_JOURNAL       = 200;   // octets en attente d'émission
static const uint8_t TAILLE_LIGNE_JOURNAL = 80;    // ligne en cours de composition
static char     jr_fifo[TAILLE_JOURNAL];
static uint8_t  jr_debut = 0, jr_nb = 0;
static char     jr_ligne[TAILLE_LIGNE_JOURNAL];
static uint8_t  jr_long = 0;
static uint16_t jr_perdues = 0;                    // lignes jetées (tampon plein)

class JournalSerie {
public:
  void begin(unsigned long baud) {
    PORT_USB.begin(baud);
    Serial1.begin(baud);    // ESP32 (étape B) : même débit, même protocole
    /* RX1 (broche 19) au repos à l'état haut quand l'ESP32 n'est pas branché :
     * sans cela, l'entrée en l'air capterait des caractères parasites. */
    pinMode(19, INPUT_PULLUP);
  }
  void print(const char* s)     { while (*s) ajouter(*s++); }
  void print(int v)             { print((long)v); }
  void print(unsigned int v)    { print((unsigned long)v); }
  void print(long v)            { char b[12]; snprintf(b, sizeof(b), "%ld", v); print(b); }
  void print(unsigned long v)   { char b[12]; snprintf(b, sizeof(b), "%lu", v); print(b); }
  void println()                { finirLigne(); }
  void println(const char* s)   { print(s); finirLigne(); }
  void println(int v)           { print(v); finirLigne(); }
  void println(unsigned int v)  { print(v); finirLigne(); }
  void println(long v)          { print(v); finirLigne(); }
  void println(unsigned long v) { print(v); finirLigne(); }

private:
  void ajouter(char c) {
    if (jr_long < TAILLE_LIGNE_JOURNAL - 1) jr_ligne[jr_long++] = c;   // au-delà : tronquée
  }
  void finirLigne() {
    if (jr_nb + jr_long + 1 > TAILLE_JOURNAL) {   // pas la place : ligne jetée, comptée
      jr_perdues++;
    } else {
      for (uint8_t i = 0; i < jr_long; i++) {
        jr_fifo[(uint8_t)((jr_debut + jr_nb) % TAILLE_JOURNAL)] = jr_ligne[i];
        jr_nb++;
      }
      jr_fifo[(uint8_t)((jr_debut + jr_nb) % TAILLE_JOURNAL)] = '\n';
      jr_nb++;
    }
    jr_long = 0;
  }
};

static JournalSerie journalSerie;

/* Toute la suite du .ino écrit dans le journal (annulé en fin de fichier). */
#define Serial journalSerie

/* Points d'appel dans setup() et loop() */
void liaisonInit();
void liaisonAvantFSM();   // entre lireEntrees() et pasFSM() : commandes -> boutons virtuels
void liaisonApresFSM();   // juste après pasFSM() : accusés de réception + émission
