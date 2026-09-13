# Mission — Audit approfondi RTOS + concurrence I²C + diagnostic PN532

Tu travailles sur le firmware ESP32 du projet **Fortico**.

Le problème observé concerne le lecteur RFID **PN532**.

## 1. Symptôme

Dans un sketch minimal isolé, le PN532 fonctionne parfaitement :

```cpp
#include <Wire.h>
#include <Adafruit_PN532.h>

Adafruit_PN532 nfc(-1, -1);

void setup() {
    Serial.begin(115200);
    delay(1000);

    Wire.begin(21, 22);

    nfc.begin();

    uint32_t versiondata = nfc.getFirmwareVersion();

    if (!versiondata) {
        Serial.println("PN532 non détecté");
        while (1) {
            delay(500);
        }
    }

    Serial.print("PN532 firmware : ");
    Serial.print((versiondata >> 24) & 0xFF, HEX);
    Serial.print('.');
    Serial.println((versiondata >> 16) & 0xFF, HEX);

    nfc.SAMConfig();
}

void loop() {
    uint8_t uid[7];
    uint8_t uidLength = 0;

    if (nfc.readPassiveTargetID(
            PN532_MIFARE_ISO14443A,
            uid,
            &uidLength,
            500)) {

        Serial.print("UID : ");

        for (uint8_t i = 0; i < uidLength; i++) {
            if (uid[i] < 0x10) {
                Serial.print('0');
            }

            Serial.print(uid[i], HEX);

            if (i < uidLength - 1) {
                Serial.print(':');
            }
        }

        Serial.println();
    }
}
```

Ce sketch détecte continuellement les badges.

Exemple :

```text
Puce PN532 trouvée ! Version firmware : 32.1
Lecteur prêt ! Approchez une carte RFID...
Badge détecté ! UID : FB:C2:1C:07
Badge détecté ! UID : 91:51:E2:06
```

Le câblage et le PN532 sont donc considérés comme fonctionnels.

---

# 2. Problème dans Fortico

Lorsque le système complet démarre avec :

* SIM7600
* GNSS/GPS
* MQTT
* TLS
* RFID PN532
* tâches FreeRTOS

le PN532 finit par produire :

```text
requestFrom(): i2cRead returned Error 263
```

Il s'agit d'un timeout/échec de communication I²C bas niveau.

La RfidTask est actuellement créée avec :

```cpp
xTaskCreatePinnedToCore(
    ...,
    tasks::CORE_NETWORK
);
```

Le cœur concerné exécute également d'autres traitements importants du système.

---

# 3. OBJECTIF PRINCIPAL

Ne modifie pas immédiatement le code.

Commence par effectuer un **audit complet de l'organisation des tâches et des accès I²C du projet**.

Je veux que tu détermines précisément si le problème vient de :

1. contention CPU ;
2. concurrence d'accès au bus I²C ;
3. absence de mutex I²C ;
4. utilisation simultanée de `Wire` par plusieurs tâches ;
5. mauvaise utilisation de `TwoWire` ;
6. double initialisation de `Wire` ;
7. mauvais ordre d'initialisation ;
8. tâche bloquante sur le même cœur ;
9. priorité RTOS incorrecte ;
10. starvation ou préemption excessive de `RfidTask` ;
11. accès simultané au PN532 depuis plusieurs contextes ;
12. problème de timeout I²C ;
13. corruption/réinitialisation de l'état du bus ;
14. interaction avec une autre bibliothèque utilisant I²C ;
15. ou toute autre cause identifiée dans le code réel.

Ne pars pas du principe que la contention CPU est la cause.

**Prouve-la ou réfute-la à partir du code.**

---

# 4. ÉTAPE 1 — CARTOGRAPHIE COMPLÈTE DES TÂCHES RTOS

Avant toute modification, recherche dans tout le firmware :

```cpp
xTaskCreate(
xTaskCreatePinnedToCore(
xTaskCreateStatic(
xTaskCreateStaticPinnedToCore(
vTaskDelay(
vTaskDelayUntil(
taskYIELD(
ulTaskNotifyTake(
xTaskNotifyWait(
xQueueReceive(
xQueueSend(
xSemaphoreTake(
xSemaphoreGive(
```

Pour chaque tâche, établis un tableau contenant :

| Tâche | Fonction | Core | Priorité | Stack | Fréquence | Peut bloquer ? | Périphériques utilisés |
| ----- | -------- | ---- | -------: | ----: | --------- | -------------- | ---------------------- |

Identifie notamment :

* RfidTask
* SIM7600 task
* GNSS task
* MQTT task
* TLS/network task
* sensor tasks
* logging task
* communication tasks
* tâches Arduino
* `loopTask`
* `IDLE0`
* `IDLE1`

Détermine également quelles tâches sont :

```text
CORE 0
CORE 1
tskNO_AFFINITY
```

et quelles tâches utilisent réellement beaucoup de CPU.

---

# 5. ÉTAPE 2 — CARTOGRAPHIE DES ACCÈS I²C

Recherche dans tout le projet :

```cpp
Wire
TwoWire
Wire.begin(
Wire.end(
beginTransmission(
endTransmission(
requestFrom(
write(
read(
Adafruit_PN532
```

Je veux connaître **TOUS les utilisateurs du bus I²C**.

Construis une cartographie :

```text
                 I²C BUS
                    │
        ┌───────────┼───────────┐
        │           │           │
      PN532      Sensor A     Sensor B
        │           │           │
    RfidTask     Task X      Task Y
```

Pour chaque périphérique, indique :

* adresse I²C ;
* classe/driver ;
* tâche qui l'utilise ;
* cœur ;
* fréquence d'accès ;
* bibliothèque utilisée ;
* fonctions `Wire` appelées ;
* présence ou absence de mutex.

---

# 6. ÉTAPE 3 — VÉRIFIER LA CONCURRENCE SUR `Wire`

C'est une étape CRITIQUE.

Détermine si plusieurs tâches peuvent exécuter simultanément :

```cpp
Wire.beginTransmission()
Wire.write()
Wire.endTransmission()
Wire.requestFrom()
Wire.read()
```

ou des appels indirects à ces fonctions via une bibliothèque.

Attention :

Le code suivant est potentiellement dangereux :

```cpp
TaskA:
    Wire.beginTransmission(...);
    Wire.write(...);
    Wire.endTransmission();

TaskB:
    Wire.requestFrom(...);
```

si aucun mécanisme de synchronisation commun ne protège le bus.

Détermine si le projet possède déjà :

```cpp
SemaphoreHandle_t
xSemaphoreCreateMutex()
xSemaphoreTake()
xSemaphoreGive()
```

dédié au bus I²C.

Si oui, vérifie que **TOUS** les accès I²C passent réellement par ce mutex.

---

# 7. ÉTAPE 4 — VÉRIFIER LE PN532

Analyse précisément :

```cpp
Adafruit_PN532
```

et notamment :

```cpp
Adafruit_PN532 nfc(-1, -1);
nfc.begin();
nfc.getFirmwareVersion();
nfc.SAMConfig();
nfc.readPassiveTargetID();
```

Compare l'initialisation Fortico avec le sketch fonctionnel.

Le fonctionnement de référence est :

```cpp
Wire.begin(21, 22);

Adafruit_PN532 nfc(-1, -1);

nfc.begin();

nfc.getFirmwareVersion();

nfc.SAMConfig();

nfc.readPassiveTargetID();
```

Identifie toute différence significative.

Vérifie notamment :

* qui initialise `Wire` ;
* quand `Wire` est initialisé ;
* combien de fois `Wire.begin()` est appelé ;
* qui possède l'objet `Adafruit_PN532` ;
* si l'objet est global, statique, local ou détruit/recréé ;
* si le PN532 est utilisé depuis plusieurs tâches ;
* si le PN532 est protégé par un mutex ;
* si le même objet est utilisé depuis plusieurs contextes.

---

# 8. ÉTAPE 5 — VÉRIFIER LA PROPRIÉTÉ DU BUS

Je veux une réponse explicite à cette question :

> Quelle couche de l'architecture Fortico est propriétaire du bus I²C ?

Il doit y avoir idéalement une architecture de ce type :

```text
Application
     │
     ├── RfidService
     │       │
     │       └── RfidDriver
     │               │
     │               └── I2C abstraction
     │
     ├── SensorService
     │       │
     │       └── I2C abstraction
     │
     └── autres périphériques
```

et non plusieurs modules faisant indépendamment :

```cpp
Wire.begin(...)
```

Vérifie donc toute initialisation multiple.

---

# 9. ÉTAPE 6 — MUTEX I²C

Si plusieurs tâches utilisent le même bus I²C, propose une architecture de mutex appropriée.

Exemple conceptuel :

```cpp
class I2CBus {
public:
    bool begin();
    bool lock(TickType_t timeout);
    void unlock();

private:
    SemaphoreHandle_t _mutex;
};
```

Puis :

```cpp
if (i2c.lock(pdMS_TO_TICKS(100))) {

    // transaction I2C complète

    i2c.unlock();
}
```

IMPORTANT :

Le mutex doit protéger **la transaction complète**, et pas seulement une fonction isolée.

Par exemple, ceci :

```cpp
lock
beginTransmission
write
endTransmission
unlock
```

doit rester atomique vis-à-vis des autres utilisateurs du bus.

Pour :

```cpp
requestFrom
read
```

la séquence entière doit également être protégée.

---

# 10. ATTENTION À `Adafruit_PN532`

Ne modifie pas aveuglément la bibliothèque Adafruit.

Avant de proposer un mutex autour du PN532, détermine si :

```cpp
readPassiveTargetID()
```

effectue plusieurs transactions I²C internes.

Si c'est le cas, le mutex doit couvrir **l'appel complet à `readPassiveTargetID()`**, afin qu'une autre tâche ne puisse pas utiliser `Wire` entre deux transactions internes du driver.

---

# 11. ÉTAPE 7 — ANALYSE DES PRIORITÉS RTOS

Pour chaque tâche, relève :

```cpp
uxTaskPriorityGet()
```

ou la priorité donnée lors de la création.

Cherche notamment :

* tâche réseau très prioritaire ;
* tâche MQTT/TLS bloquante ;
* traitement SIM7600 très long ;
* parsing GNSS intensif ;
* absence de `vTaskDelay()` dans une boucle ;
* boucle qui tourne en permanence ;
* attente active ;
* appels réseau bloquants ;
* sections critiques trop longues ;
* mutex détenu trop longtemps.

Je veux que tu identifies toute tâche pouvant monopoliser un cœur.

---

# 12. ÉTAPE 8 — ANALYSE DES COEURS CPU

Vérifie précisément :

```cpp
tasks::CORE_NETWORK
```

et trouve sa valeur réelle.

Ne suppose pas que :

```cpp
tskNO_AFFINITY
```

résout forcément le problème.

Compare :

```text
Cas A
RfidTask -> CORE_NETWORK

Cas B
RfidTask -> tskNO_AFFINITY

Cas C
RfidTask -> autre cœur
```

Explique les conséquences de chaque configuration.

---

# 13. ÉTAPE 9 — INSTRUMENTATION TEMPORAIRE

Avant une grosse correction, ajoute si nécessaire une instrumentation temporaire permettant de mesurer :

```cpp
xPortGetCoreID()
uxTaskPriorityGet(NULL)
uxTaskGetStackHighWaterMark(NULL)
```

ainsi que des timestamps autour des opérations I²C.

Par exemple :

```text
[RfidTask] core=1 priority=...
[RfidTask] I2C start
[RfidTask] I2C end duration=...
```

Si possible, mesure également :

```text
RfidTask execution time
I2C transaction duration
time waiting for I2C mutex
```

L'objectif est de déterminer si l'erreur 263 apparaît après :

* une longue attente CPU ;
* une attente de mutex ;
* une transaction I²C concurrente ;
* ou sans relation évidente avec ces événements.

---

# 14. ÉTAPE 10 — TEST D'ISOLATION CPU

Après l'analyse seulement, effectuer un test minimal.

Dans :

```cpp
RfidDriver::start()
```

identifier :

```cpp
xTaskCreatePinnedToCore(...)
```

et remplacer temporairement :

```cpp
tasks::CORE_NETWORK
```

par :

```cpp
tskNO_AFFINITY
```

Ne modifier RIEN d'autre.

Demander ensuite un test d'au moins 5 minutes avec :

* SIM7600 actif ;
* GNSS actif ;
* MQTT actif ;
* TLS actif ;
* RFID actif.

Comparer les erreurs :

```text
Error 263
```

avant/après.

---

# 15. TEST CPU PLUS PROBANT

Si `tskNO_AFFINITY` améliore le problème, effectuer ensuite un deuxième test :

épingler explicitement `RfidTask` sur l'autre cœur.

Comparer :

```text
CORE_NETWORK
vs
tskNO_AFFINITY
vs
OTHER_CORE
```

Ne conclure à la contention CPU que si les résultats expérimentaux le justifient.

---

# 16. ÉTAPE 11 — CORRECTION DE LA CONCURRENCE I²C

Si l'audit montre plusieurs utilisateurs concurrents de `Wire`, implémente une synchronisation correcte.

Contraintes :

1. un seul mutex partagé pour le bus physique ;
2. toutes les tâches utilisant le bus doivent utiliser ce mutex ;
3. le mutex protège la transaction complète ;
4. ne pas créer un mutex différent par périphérique ;
5. ne pas protéger uniquement `Wire.write()` ;
6. ne pas protéger uniquement `Wire.requestFrom()` ;
7. éviter de garder le mutex pendant des traitements CPU inutiles ;
8. ne pas appeler de code bloquant inutilement sous mutex ;
9. gérer proprement l'échec de `xSemaphoreTake()`.

---

# 17. ÉTAPE 12 — BACKOFF RFID

Le projet possède actuellement :

```cpp
_readFailureCount
```

avec un backoff :

```text
1 s
5 s
15 s
```

Ne considère PAS :

```cpp
readPassiveTargetID() == false
```

comme automatiquement une erreur I²C.

Une absence de badge est un fonctionnement normal.

Avant chaque tentative de lecture, utiliser un probe I²C approprié au driver et à l'adresse réellement utilisée par le PN532.

Conceptuellement :

```cpp
Wire.beginTransmission(PN532_I2C_ADDRESS);
const uint8_t probeErr = Wire.endTransmission();
```

Si :

```cpp
probeErr != 0
```

alors :

```text
vraie erreur de communication
→ incrémenter _readFailureCount
→ backoff
```

Si :

```cpp
probeErr == 0
```

alors :

```text
PN532 joignable
→ reset _readFailureCount
→ appeler readPassiveTargetID()
```

et :

```cpp
readPassiveTargetID() == false
```

doit être interprété comme :

```text
pas de badge
```

et non comme une panne I²C.

IMPORTANT :

Vérifier d'abord dans la bibliothèque utilisée :

* l'adresse I²C réelle ;
* le mode I²C ;
* le comportement de `readPassiveTargetID()` ;
* si le probe proposé est compatible avec le driver ;
* s'il existe une meilleure méthode pour distinguer erreur de transport et absence de badge.

Ne copie pas aveuglément le code du prompt.

---

# 18. NE PAS INTRODUIRE DE RÉGRESSION

Ne modifie pas :

* le protocole MQTT ;
* le protocole SIM7600 ;
* le GNSS ;
* les topics MQTT ;
* les structures métier ;
* les API publiques ;
* l'architecture applicative existante ;

sauf si l'analyse démontre qu'une modification est nécessaire.

Le changement doit être minimal et architecturalement cohérent.

---

# 19. LIVRABLE 1 — RAPPORT D'AUDIT

Avant de modifier le code, produire un rapport :

## A. Organisation RTOS

```text
Task
Core
Priority
Stack
Blocking calls
Dependencies
```

## B. Organisation I²C

```text
Bus
SDA
SCL
Devices
Addresses
Tasks
Mutex
```

## C. Concurrence détectée

Indiquer explicitement :

```text
SAFE
ou
RACE CONDITION
ou
POTENTIAL CONTENTION
```

## D. Cause probable de Error 263

Classer :

```text
Très probable
Probable
Possible
Peu probable
Réfuté
```

## E. Preuves dans le code

Citer les fichiers et fonctions concernés.

---

# 20. LIVRABLE 2 — PLAN DE CORRECTION

Avant d'éditer les fichiers, proposer :

```text
Fichier
Modification
Raison
Impact
```

Ne modifier le code qu'après cette analyse.

---

# 21. LIVRABLE 3 — MODIFICATIONS

Après validation de l'analyse, implémenter la correction minimale.

Pour chaque fichier modifié, fournir :

```text
fichier
ancienne logique
nouvelle logique
raison
```

---

# 22. LIVRABLE 4 — DIFF

Fournir le diff exact :

```bash
git diff -- fichier1 fichier2 ...
```

Ne pas simplement résumer les modifications.

---

# 23. LIVRABLE 5 — VALIDATION

Décrire précisément comment tester :

### Test 1 — fonctionnement normal

```text
ESP32
+ SIM7600
+ GNSS
+ MQTT
+ TLS
+ PN532
```

pendant au moins 5 minutes.

Vérifier :

* aucune erreur `Error 263` ;
* badge détecté rapidement ;
* plusieurs badges détectables ;
* absence de badge ne déclenche pas de backoff.

### Test 2 — absence de badge

Laisser le système sans badge pendant plusieurs minutes.

Vérifier :

```text
_readFailureCount == 0
```

ou l'équivalent logique.

### Test 3 — déconnexion PN532

Déconnecter physiquement le PN532 ou couper son alimentation.

Vérifier :

```text
I2C probe failure
→ _readFailureCount augmente
→ backoff
```

### Test 4 — reconnexion

Reconnecter le PN532.

Vérifier :

```text
probe OK
→ _readFailureCount reset
→ détection normale
```

### Test 5 — concurrence

Pendant que MQTT + SIM7600 + GNSS fonctionnent :

présenter plusieurs badges successivement.

Mesurer si les lectures restent fiables.

---

# 24. RÈGLE ABSOLUE

**Ne dis pas simplement "le problème vient de FreeRTOS" ou "le problème vient du mutex".**

Je veux que tu recherches le problème dans le projet réel.

La référence de fonctionnement est le sketch minimal PN532 fourni au début.

Compare systématiquement :

```text
SKETCH FONCTIONNEL
        ↓
Wire.begin(21,22)
        ↓
PN532
        ↓
I2C
        ↓
readPassiveTargetID()
```

avec :

```text
FORTICO
        ↓
RTOS
        ↓
Tasks
        ↓
Drivers
        ↓
I2C abstraction
        ↓
PN532
```

L'objectif final est d'expliquer précisément pourquoi :

```text
Sketch minimal → fonctionne
Fortico        → Error 263
```

et de corriger la cause racine avec le minimum de modifications.

**Ne touche pas au code avant d'avoir terminé la cartographie RTOS + I²C et présenté les causes probables.**
