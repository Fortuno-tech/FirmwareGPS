# Rapport post-correction — FirmwareGPS

## Portée et statut des preuves

Ce rapport reprend les constats de [l'audit initial](./rapport.pdf) et les
travaux T0 à T13. Le point de départ est le tag `v0-baseline`
(`8e8fff9`). Les changements sont présents dans l'historique sous forme de
commits distincts par tâche; la branche courante est la branche d'intégration
T13.

Statuts utilisés :

- **IMPLÉMENTÉ** : changement de code ou documentation présent.
- **TESTÉ EN LOGICIEL** : test natif, test logiciel ou build exécuté et réussi.
- **VALIDÉ SUR MATÉRIEL** : réservé à un résultat de banc fourni et vérifiable.
- **OUVERT** : décision ou preuve manquante.

Aucun résultat de banc n'a été fourni. Aucun point de ce rapport n'est donc
**VALIDÉ SUR MATÉRIEL**.

## Constats de l'audit initial

| Constat | Statut | Références et preuve logicielle | Limite restante |
|---|---|---|---|
| Commandes MQTT sensibles sans authentification cryptographique ni anti-rejeu persistant. | TESTÉ EN LOGICIEL | Canonicalisation et validation HMAC/temps/séquence dans [mqtt_command_security.cpp](../src/services/mqtt/mqtt_command_security.cpp#L106); vecteurs et cas `REPLAY`, `EXPIRED`, `BAD_MAC`, horloge non synchronisée dans [test_mqtt_command_security.cpp](../test/test_mqtt_command_security/test_mqtt_command_security.cpp#L134) et contrat [backend-contract.md](./backend-contract.md#L61). | Le test natif compare les vecteurs documentés avec un vérificateur de vecteurs; il ne démontre ni les secrets provisionnés sur appareil ni l'intégration broker/backend. |
| ACK MQTT interprété comme confirmation d'exécution du relais. | TESTÉ EN LOGICIEL | ACK de réception et confirmation d'exécution corrélée par `id` décrits dans [backend-contract.md](./backend-contract.md#L82); cas d'ordre, échec, ACK perdu et absence de double résultat dans [test_remote_command_execution.cpp](../test/test_remote_command_execution/test_remote_command_execution.cpp#L12). | La livraison MQTT peut être retardée ou perdue; il faut que le backend distingue les deux phases et réconcilie les identifiants. |
| Badge RFID partagé `91:51:E2:06` autorisé et réutilisable entre appareils. | TESTÉ EN LOGICIEL | Le démarrage retire ce badge de l'allowlist existante dans [storage_service.cpp](../src/services/storage/storage_service.cpp#L62); tests de migration et de frontière RFID dans [test_storage_migration.cpp](../test/test_storage_migration/test_storage_migration.cpp#L66) et [test_rfid_auth_boundary.cpp](../test/test_rfid_auth_boundary/test_rfid_auth_boundary.cpp#L65). | Le nettoyage d'une NVS déjà provisionnée, le badge neuf et l'effet opérationnel doivent être vérifiés sur cible. Le format versionné reste désactivé par défaut. |
| Opérations réseau susceptibles de retarder le cycle de contrôle. | TESTÉ EN LOGICIEL | Contrôle planifié toutes les 100 ms dans [application.cpp](../src/app/application/application.cpp#L65); attente réseau séparée, budgets, mutex et watchdog documentés dans [connectivity-task-isolation.md](./connectivity-task-isolation.md#L1). Test logiciel `testControlCyclesContinueWhileNetworkWorkerIsUnavailable` dans [test_connectivity_handoff.cpp](../test/test_connectivity_handoff/test_connectivity_handoff.cpp#L9). | Les priorités, la latence réelle, l'équité des mutex, le watchdog et l'absence de contention physique restent à mesurer sur ESP32/modem/PN532. La boucle HALT de provisioning est volontairement persistante avec relais coupé. |
| Secure Boot et Flash Encryption exigés en production sans preuve de l'état eFuse ou de l'image. | TESTÉ EN LOGICIEL | La garde est compilée seulement sous `FORTICO_PRODUCTION_BUILD` dans [main.cpp](../src/main.cpp#L421); la configuration de build se trouve dans [platformio.ini](../platformio.ini#L37). Les builds production et développement réussissent. | Aucun eFuse n'a été lu/programmé dans cette tâche et aucune image signée/chiffrée n'a été qualifiée sur cible. Le pipeline d'artefact de release reste incomplet; aucune release production n'est autorisée sur la seule base du build. |
| Validation TLS SIM7600, notamment date et nom d'hôte, à qualifier. | OUVERT | Le client garde la validation temporelle stricte (`ignorelocaltime=0`); cas attendus et résultats laissés vides dans [tls-qualification.md](./tls-qualification.md#L74). | Expiration, mauvais hôte, mauvaise CA et horloge invalide n'ont pas été testés sur le SIM7600. La validation hostname dépend du firmware modem et reste un blocage de release. |
| OTA signée présente par morceaux, sans chemin complet de mise à jour et de rollback. | OUVERT | [security-release-procedure.md](./security-release-procedure.md#L79) indique explicitement que l'OTA reste désactivée en production jusqu'à livraison et qualification du flux complet; l'option d'activation est référencée dans [project_config.h](../include/config/project_config.h#L64). | Téléchargement, manifeste, vérification, installation, redémarrage, validation et rollback bout en bout ne sont pas livrés. |
| Tests PlatformIO précédemment inopérants ou sans configuration CI fiable. | TESTÉ EN LOGICIEL | Le workflow [ci.yml](../.github/workflows/ci.yml#L3) exécute les tests natifs et les builds; le run local intégré final a réussi. | La protection de branche GitHub exige encore de configurer le check CI comme obligatoire hors du dépôt. |
| Contrat MQTT/backend divergent ou non partagé (topics, payloads, QoS, auth, déduplication). | TESTÉ EN LOGICIEL | Topics, QoS, JSON, HMAC, fenêtre temporelle, ACK, outbox, erreurs et vecteurs sont décrits dans [backend-contract.md](./backend-contract.md#L1); [simulateur backend](../tools/backend_sim/README.md#L1) et ses 14 tests logiciels couvrent les scénarios définis. | Aucun broker Mosquitto/HiveMQ ni backend de production n'a été utilisé. L'acceptation du contrat et la déduplication côté backend restent à confirmer. |
| Modules vides et architecture documentaire ne reflétant pas les composants réellement lancés. | TESTÉ EN LOGICIEL | Les stubs remplacés par des composants câblés ont été supprimés; `TrackingManager` est explicitement différé. Voir [architecture-runtime.md](./architecture-runtime.md#L1), [task_config.h](../include/config/task_config.h#L80) et les deux fichiers conservés sous `src/managers/tracking_manager/`. Build production réussi après ces suppressions. | La spécification Word d'architecture est toujours une référence historique plus large; le document as-built est la description du chemin exécuté. |
| Outbox SD, espace plein, reprise FIFO et perte de la file RAM à caractériser. | TESTÉ EN LOGICIEL | CRC, séquence persistante, FIFO, échec de publication et politiques `REJECT_NEW`/`OVERWRITE_OLDEST` dans [telemetry-outbox.md](./telemetry-outbox.md#L1) et [test_telemetry_outbox.cpp](../test/test_telemetry_outbox/test_telemetry_outbox.cpp#L230). | Une interruption avant persistance peut perdre jusqu'à quatre messages en attente et un en cours. Les comportements réels de la carte, de l'espace libre, du flush et des coupures sont à mesurer sur banc. |

## Bilan par tâche

| Tâche | Fichiers principaux | Tests/preuves | Statut | Reste à faire |
|---|---|---|---|---|
| T0 — Hygiène | Tag `v0-baseline`; état Git; inventaire de `recovery/all-commits` | Arbre propre avant changements et après les commits; tag existant vérifié | OUVERT | L'historique ne permet pas de distinguer de façon fiable chaque fichier « restauré » d'un simple delta de branche; voir « Incertitudes ». Le fichier `.pyc` suivi est à retirer seulement après accord. |
| T1 — Sémantique ACK | Gestionnaire MQTT, contrôleur, exécution relais, contrat | `test_remote_command_execution`: ordre, échec, ACK perdu, pas de double résultat | TESTÉ EN LOGICIEL | Confirmer les deux phases côté backend et effectuer l'essai sur cible. |
| T2 — Contrat backend | [backend-contract.md](./backend-contract.md), vecteurs et test sécurité MQTT | Vecteur valide, rejeu, expiré exercés par `test_mqtt_command_security` | TESTÉ EN LOGICIEL | Faire approuver le contrat par l'équipe backend. |
| T3 — Simulateur backend | [tools/backend_sim/](../tools/backend_sim/), README, dépendances | `python -m unittest discover -s tools\\backend_sim\\tests -v`: 14 réussis, 0 échoué | TESTÉ EN LOGICIEL | Exécuter avec un broker et un appareil de test provisionné; aucun secret réel dans le dépôt. |
| T4 — Politiques sûreté | [safety_config.h](../include/config/safety_config.h), [safety-policy.md](./safety-policy.md), politique relais | `test_remote_relay_policy`: limites 0,50/0,51 km/h, GPS frais/périmé, perte de fix, verrou | TESTÉ EN LOGICIEL | Faire approuver les valeurs/risques puis tester avec GPS et relais isolés. |
| T5 — Horloge désynchronisée | Sécurité commande et [backend-contract.md](./backend-contract.md#L47) | `testUnsyncedClockUsesPersistentSequenceWithoutTimeWindow` | TESTÉ EN LOGICIEL | Essai de démarrage réel sans RTC/réseau; TLS doit rester fail-closed si la date n'est pas validable. |
| T6 — Outbox SD | Outbox, adaptateur SD, [telemetry-outbox.md](./telemetry-outbox.md) | Suite native outbox: corruption, séquence, FIFO, politiques pleine, échec publish | TESTÉ EN LOGICIEL | Scénarios SD pleine/coupure HIL; sélectionner une politique approuvée. |
| T7 — Migration NVS | `StorageService`, configuration sécurité, [nvs-badge-migration.md](./nvs-badge-migration.md) | Ancien/nouveau format, idempotence, NVS vide, migration désactivée | TESTÉ EN LOGICIEL | Décider d'activer le format versionné après sauvegarde et essai sur appareil représentatif. |
| T8 — Isolation réseau/contrôle | `ConnectivityTask`, SIM7600, I2C, watchdog, mesures optionnelles; [connectivity-task-isolation.md](./connectivity-task-isolation.md) | Tests natifs connectivité/contrôle et timing; builds avec timing validés dans la tâche | TESTÉ EN LOGICIEL | Mesurer contention, latence, watchdog et relais sur HIL. |
| T9 — CI | [ci.yml](../.github/workflows/ci.yml) | Même suite native et deux builds réussis localement | TESTÉ EN LOGICIEL | Rendre le workflow obligatoire dans les règles de protection de branche du dépôt hébergé. |
| T10 — TLS/production | [tls-qualification.md](./tls-qualification.md), garde de build production | Build production et build développement réussis; aucune programmation d'eFuse | IMPLÉMENTÉ | Exécuter les quatre essais TLS sur modem et terminer la qualification secure boot/chiffrement uniquement selon la procédure approuvée. |
| T11 — Plan HIL | [hil-test-plan.md](./hil-test-plan.md) | Scénarios et résultats vides documentés | IMPLÉMENTÉ | Exécuter les cas sur banc isolé et conserver les résultats expurgés. |
| T12 — Modules/architecture | [architecture-runtime.md](./architecture-runtime.md), `task_config.h`, stubs GPS/MQTT/réseau/OTA retirés | Build production et développement réussis; arbre de source vérifié | TESTÉ EN LOGICIEL | Décider ultérieurement du périmètre du `TrackingManager`; ne pas l'implémenter sans besoin produit. |
| T13 — Revue post-correction | Ce rapport | Relecture des constats, tests et limites | IMPLÉMENTÉ | Mettre à jour après résultats backend et HIL. |

## Résultats exacts de validation

Exécuté sur la chaîne intégrée incluant T12 :

| Commande | Résultat |
|---|---|
| `python -m platformio test -e native` | Réussi : 13 suites PlatformIO, 82 tests réussis, 0 échoué. |
| `python -m platformio run -e production` | Réussi : `[SUCCESS]`, 92,97 s. |
| `python -m platformio run -e esp32doit-devkit-v1` | Réussi : `[SUCCESS]`, 105,47 s. |
| `python -m unittest discover -s tools\\backend_sim\\tests -v` | Réussi : 14 tests réussis, 0 échoué. |

Ces résultats sont **TESTÉS EN LOGICIEL** uniquement. Aucun test HIL, broker
réel, programmation ou lecture d'eFuse n'a été effectué dans cette tâche.

## Décisions ouvertes

- **Sûreté — KILL à 0,5 km/h :** valeur par défaut conservée. Une vitesse non
  nulle peut déclencher une coupure en mouvement; la réduire peut rendre la
  commande inutilisable avec le bruit GNSS.
- **Sûreté — fraîcheur GPS 25 s :** valeur par défaut conservée. Une valeur plus
  grande accepte une position plus ancienne; une valeur plus petite risque de
  rejeter KILL lors d'une perte de satellites.
- **Sûreté — verrou KILL jusqu'au redémarrage :** valeur par défaut conservée.
  Toute levée logicielle peut permettre un réengagement inattendu; le
  redémarrage peut retarder la remise en service.
- **Produit — ENGINE_ON :** RFID locale et état sûr exigés par défaut. Toute
  modification pourrait autoriser une mise en marche distante sans présence
  locale ou depuis un état d'urgence.
- **Déploiement — migration NVS :** `FORTICO_ENABLE_NVS_MIGRATION=0` par défaut.
  Définir un plan de sauvegarde/retour arrière avant activation graduelle. Le
  retrait du badge partagé demeure indépendant et actif pour préserver le
  comportement préexistant.
- **Données — SD pleine :** `REJECT_NEW` reste la politique par défaut.
  `OVERWRITE_OLDEST` favorise les mesures récentes au prix de l'historique.
  L'écriture SD avant admission RAM n'est pas activée car elle pourrait ajouter
  de la latence au chemin de contrôle.
- **Temps — horloge non fiable :** la fenêtre anti-expiration de commande est
  inactive; MAC et compteur persistant restent requis. Le comportement TLS
  reste strict et peut empêcher MQTT tant que le SIM7600 ne dispose pas d'une
  date validable.
- **Backend — contrat :** l'équipe backend doit approuver les topics, formats,
  QoS, erreurs, identité et déduplication `(deviceId, outboxSeq)` avant
  intégration en production.
- **CI — blocage de merge :** configurer le check GitHub correspondant comme
  requis dans la protection de branche.
- **OTA et release matérielle :** ne pas activer ni publier avant réalisation
  de la chaîne OTA complète et qualification secure boot/flash encryption.

## À valider sur matériel

Utiliser un ESP32/SIM7600 de test, une alimentation limitée en courant, un
relais chargé uniquement sur simulateur, un broker non-production et des
identifiants de test provisionnés hors dépôt. Ne jamais brûler d'eFuse.

1. Suivre intégralement [le plan HIL](./hil-test-plan.md) : modem absent 10 min,
   reconnexion longue, commande rejouée/expirée, démarrage sans horloge, KILL
   sans GPS valide, perte réseau 30 min, coupure pendant l'écriture outbox, SD
   pleine, badge neuf, certificat expiré.
2. Suivre [la qualification TLS SIM7600](./tls-qualification.md) : certificat
   expiré, mauvais hôte, mauvaise CA et horloge invalide. Un succès TLS en cas
   d'erreur est un échec de qualification et bloque la release.
3. Mesurer latence commande→relais, durée maximale du cycle de contrôle,
   budget des tâches, contention UART/I2C, watchdog et état du relais avec
   l'instrumentation optionnelle activée puis désactivée.
4. Tester une image de développement sur cible dédiée; pour Secure Boot/Flash
   Encryption, suivre la procédure release approuvée seulement sur cible
   dédiée, avec récupération documentée. Ne pas exécuter de commande eFuse
   dans ce plan.
5. Vérifier la compatibilité des blobs NVS provisionnés représentatifs avant
   d'activer la migration; tester la suppression du badge partagé et le
   provisioning/retrait d'un badge de test.

La colonne « Résultat » des deux plans reste vide tant qu'un opérateur n'a pas
fourni les procès-verbaux de banc.

## Incertitudes et risques de régression

- `recovery/all-commits` diffère de `v0-baseline` sur 192 chemins, mais aucun
  manifeste ne prouve lesquels ont été restaurés manuellement. Ce delta de
  branche ne permet donc pas une liste fiable des seules restaurations.
- `docs/rapport.pdf` est la source de l'audit et est conservé. Le fichier
  suivi `tools/__pycache__/upload_certificate.cpython-314.pyc` n'est pas requis
  par le runtime; sa suppression est proposée pour une tâche d'hygiène
  ultérieure, mais n'a pas été faite sans accord. `tools/certificates/hivemq-ca.pem`
  est référencé comme CA publique par les outils de provisioning; aucune clé
  privée ni donnée d'identification n'a été ajoutée dans ces corrections.
- Les tests de vecteurs natifs valident le texte canonique et les signatures
  pré-calculées avec un vérificateur de test; l'algorithme de test n'est pas
  l'implémentation cryptographique embarquée. La primitive et son intégration
  nécessitent une preuve séparée avec des tests cryptographiques de référence.
- Le simulateur backend valide sa logique locale mais son dédoublonnage est en
  mémoire et n'est pas un backend durable.
- `OVERWRITE_OLDEST` dépend de l'estimation d'espace du système de fichiers;
  la carte SD peut se comporter différemment après coupure ou défaut matériel.
- La séparation et les mutex T8 sont démontrés au niveau du code seulement.
  La latence physique du relais et le comportement du watchdog ne sont pas
  mesurés.
- Le `TrackingManager` reste mentionné dans la spécification Word mais n'est
  pas créé par le runtime; les fichiers sont explicitement marqués hors
  périmètre jusqu'à décision produit.
