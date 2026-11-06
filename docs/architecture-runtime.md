# Architecture réellement exécutée

Ce document décrit le câblage exécuté par `setup()` et les tâches démarrées
dans `src/main.cpp`. Le document Word « 02 - Firmware Architecture
Specification » décrit une architecture cible plus large; ses composants
non câblés ci-dessous ne sont pas des fonctions firmware disponibles.

## Chemin de démarrage et contrôle local

1. `setup()` force le relais à l'état coupé avant le reste de l'initialisation.
   Le profil `production` exige ensuite Secure Boot et Flash Encryption actifs;
   le profil de développement n'exécute pas cette garde.
2. NVS et le propriétaire du modem sont initialisés. Un appareil non provisionné
   reste dans l'état HALT, relais coupé, jusqu'à un provisioning série valide.
3. `Application` démarre `SystemController`, qui initialise l'authentification,
   la télémétrie, les diagnostics, le relais, RFID, EventBus et connectivité.
4. `Scheduler` appelle le cycle `SystemController::update()` toutes les 100 ms
   sur le cœur applicatif. Il traite les événements/commandes relais, l'état
   RFID et la machine à états locale. Les appels réseau, l'enregistrement modem,
   la connexion MQTT et le polling GNSS sont effectués dans des tâches réseau
   séparées.

## Composants câblés

| Domaine | Implémentation exécutée |
|---|---|
| Relais | `RelayManager` et `RelayDriver`; le contrôleur soumet les actions et le gestionnaire applique la politique |
| RFID | `RfidDriver` PN532 via `I2cHal`, `RfidService`, `AuthenticationService` et `AuthenticationManager` |
| Modem/GNSS/RTC | `Sim7600Driver`, `Sim7600Service`, `Sim7600GnssDriver`, `GpsService`, `RtcDriver`; `ConnectivityTask` gère les opérations connectivité |
| MQTT | `CellularMqttClient`, `MqttPublishTask`, `MqttCommandHandler`, file de publication et sécurité de commande |
| Télémétrie/outbox | `TelemetryService`, `TelemetryManager`, `TelemetryOutbox` et `SdOutboxStorage`; le replay réseau est best-effort |
| Persistance/sécurité | `StorageService`, `SecurityService`, NVS et SD |
| Tâches transverses | `Scheduler`, `EventBus`, `Logger`, `Watchdog`, diagnostic et gestion d'alertes |

`ConnectivityTask` et `MqttPublishTask` s'exécutent sur le cœur réseau.
Le cycle de contrôle est lancé par `Scheduler` sur le cœur applicatif.
L'UART du SIM7600 et le bus I2C partagé sont protégés par mutex à attentes
bornées dans les chemins documentés dans
[connectivity-task-isolation.md](./connectivity-task-isolation.md).
Cette propriété logicielle ne constitue pas une validation HIL.

## Modules prévus mais absents ou différés

- `gps_driver.*` était un stub vide remplacé par `Sim7600GnssDriver`; les deux
  fichiers ont été retirés.
- `mqtt_service.*` et `topic_router.*` étaient des stubs vides remplacés dans
  le chemin actif par `CellularMqttClient`, `MqttPublishTask` et
  `MqttCommandHandler`; les stubs ont été retirés.
- `network_service.*` était un stub vide remplacé par `ConnectivityTask` et
  `Sim7600Service`; les deux fichiers ont été retirés.
- `ota_service.*` était un stub vide et n'est pas implémenté. Le dépôt possède
  `OtaManager` et un vérificateur de signature partiels, mais aucun chemin de
  téléchargement/installation/rollback OTA de bout en bout; les stubs ont été
  retirés et l'OTA reste hors périmètre de release.
- `TrackingManager` n'est pas câblé au runtime. Ses deux fichiers vides sont
  conservés explicitement hors périmètre, car la spécification Word le cite
  encore. Les données GNSS actives passent par `GpsService` et le contrôleur.

Les autres définitions de `task_config.h` décrivent des valeurs de référence;
elles ne prouvent pas à elles seules la création d'une tâche. Pour la
validation matérielle, se reporter au [plan HIL](./hil-test-plan.md).
