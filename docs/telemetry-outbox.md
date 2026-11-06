# Outbox de télémétrie sur SD

`TelemetryOutbox` stocke les messages MQTT avant publication, dans des fichiers
versionnés (`record_<séquence>.msg`). Le format v2 conserve le topic complet,
le numéro de séquence, les longueurs du topic/payload et un CRC-32. Le format v1 existant
reste lisible et est routé vers le topic principal `telemetry`. Un fichier
temporaire n'est promu en message qu'après écriture, flush et relecture valides.
Une écriture interrompue ne remplace aucun message déjà validé. Un message
corrompu bloque la lecture FIFO et reste sur la carte pour diagnostic, au lieu
d'être ignoré ou supprimé.

Deux métadonnées alternées avec CRC conservent le numéro réservé le plus élevé.
La réservation précède l'écriture du message : un arrêt pendant l'écriture peut
donc créer un trou de séquence, mais ne réutilise pas un numéro déjà attribué.
La publication rejoue le plus ancien message et ne l'efface qu'après confirmation
de succès par le callback. Un échec de publication ou d'effacement peut entraîner
un doublon (sémantique au-moins-une-fois), jamais un ACK local prématuré.

Il n'y a pas de limite arbitraire de rétention configurée : l'outbox utilise la
place disponible de la carte. Toute erreur d'écriture (dont SD pleine) est
remontée au task MQTT; il conserve le message qu'il n'a pas pu persister et ne
dépile pas le suivant.

## Branchement à l'application

`TelemetryService` et `MqttTelemetrySink` ne font qu'une admission non bloquante
dans une file FIFO FreeRTOS dédiée à la télémétrie (4 messages). Le topic principal
`telemetry` et les canaux `gps`, `fuel`, `battery`, `rfid`, `engine`, `alarm` et
`status` passent tous par cette file; la file MQTT prioritaire/non prioritaire
des autres messages reste séparée. Si la file est pleine ou si le payload/topic
est invalide, l'admission échoue immédiatement; les méthodes booléennes la
remontent et `TelemetryService` le journalise.

Le `MqttPublishTask` est l'unique propriétaire de `begin`, `enqueue`, replay et
ACK de l'outbox. Hors du cycle de contrôle/relais, il copie un message de la file
FIFO dans un emplacement local de réessai, l'écrit sur SD, puis tente le replay.
Une erreur d'initialisation ou d'écriture laisse ce message en attente et bloque
le dépilage du suivant; les quatre emplacements bornés fournissent une
contre-pression plutôt qu'une suppression silencieuse. Les enregistrements
persistés sont rejoués dans l'ordre et supprimés uniquement après succès de
`MqttClient::publish`. Le JSON transmis conserve ses champs et reçoit
`outboxSeq`; une panne de connexion ou de publication garde le fichier. Une perte
de connexion après acceptation broker mais avant suppression locale peut produire
un doublon : la garantie après persistance SD est « au moins une fois », pas
exactement une fois.

Le backend/consommateur de télémétrie doit dédupliquer sur `(deviceId, outboxSeq)`
avant mise à jour des données. Aucun consommateur/backend n'est présent dans ce
dépôt; sa prise en charge du nouveau champ `outboxSeq` et cette clé de
dédoublonnage, ainsi que l'acceptation de plusieurs topics, doivent être
confirmées avant déploiement.

La file de handoff est volatile : un arrêt brutal avant que le task MQTT ait
persisté un message déjà admis peut perdre les messages encore dans la file et
son emplacement de réessai. La taille bornée est de quatre messages en file plus
un message en cours de persistance. Éliminer cette fenêtre exigerait une
persistance synchrone (incompatible avec le cycle de contrôle non bloquant) ou un
support matériel de journal durable adapté; ne pas assimiler l'admission RAM à
un ACK durable.

## Rétention et capacité

Aucune éviction automatique ni limite arbitraire n'est appliquée : les messages
persistés non acquittés restent sur SD jusqu'à publication réussie et l'espace
disponible de la carte est la limite actuelle. Une carte pleine bloque le task à
la tête de la file, puis la contre-pression remplit le handoff et les nouvelles
admissions échouent explicitement. Les anciens messages ne sont jamais sacrifiés
pour faire de la place. La politique de capacité/rétention et la réservation
éventuelle d'espace pour les autres données SD restent des décisions requises
avant le déploiement opérationnel.

La garantie après coupure dépend aussi du comportement réel de la carte et du
système de fichiers : `flush` et relecture détectent les erreurs logicielles,
mais une validation HIL avec coupure pendant écriture/rename, SD pleine ou retirée,
ainsi que la reconnexion modem et le comportement de l'accusé broker, est
nécessaire pour caractériser la durabilité réelle.
