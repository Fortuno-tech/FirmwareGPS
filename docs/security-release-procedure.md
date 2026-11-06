# Sécurité matérielle et procédure de release

## État actuel

Le profil `production` compile avec `FORTICO_PRODUCTION_BUILD`. Au démarrage,
il doit couper le relais puis refuser de démarrer si Secure Boot ou Flash
Encryption est inactif. Le profil de développement `esp32doit-devkit-v1` ne
programme pas d’eFuse et reste utilisable sur une carte de développement.

Ce garde-fou logiciel ne configure ni ne programme les protections matérielles.
La chaîne ESP-IDF/PlatformIO actuelle ne produit pas encore un artefact signé
et chiffré vérifiable pour une révision précise de puce. Aucune release
production n’est autorisée tant que ce pipeline n’est pas livré et validé sur
une cible dédiée.

## Procédure de qualification sur cible dédiée

1. Réserver une carte de test distincte des cartes de développement et de
   production. Relever le modèle du module, la révision du silicium, le numéro
   de série et la version du modem. Photographier le câblage et préparer une
   méthode de récupération documentée avant toute modification irréversible.
2. Avec la carte en lecture seule, exécuter `espefuse.py --port <PORT> summary`
   depuis la version d’ESP-IDF/esptool retenue. Archiver la sortie avec
   l’identifiant de la cible. Ne pas utiliser de commande `burn_efuse` ni
   d’option de programmation sur la carte de développement.
3. Construire le profil de garde avec
   `python -m platformio run -e production`. Vérifier l’empreinte de l’image,
   les paramètres du bootloader et la signature avec les outils de release
   approuvés. Cette étape ne doit pas programmer la carte.
4. Avant d’activer des eFuses sur la cible dédiée, valider la compatibilité
   Secure Boot (version adaptée à la révision), les clés hors ligne, le mode
   Flash Encryption, le chemin de récupération et le mécanisme de mise à jour.
   L’opérateur doit suivre la procédure Espressif correspondant exactement à
   la puce et à la version d’ESP-IDF approuvées. Cette procédure n’est pas
   encore automatisée dans ce dépôt; ne pas improviser ni transposer des
   commandes d’une autre cible.
5. Après activation sur la seule cible dédiée, démarrer une image signée et
   chiffrée, vérifier les états matériels au démarrage, tester une image non
   signée et une image altérée (elles doivent être refusées), puis exécuter le
   plan de récupération sans désactiver les protections.
6. Faire approuver le procès-verbal de test et la récupération avant de créer
   une release. Conserver les clés privées de signature dans le système de
   gestion de clés de release; elles ne doivent jamais être provisionnées
   dans le firmware ou la carte.

## Provisioning des appareils

- Attribuer une identité unique par appareil et provisionner les secrets depuis
  un coffre de secrets ou un HSM approuvé, par une fixture physique contrôlée.
- Ne jamais réutiliser une clé racine, un identifiant ou des identifiants MQTT
  entre appareils. Ne pas enregistrer le payload série ni les secrets dans les
  journaux du poste de provisioning.
- La clé publique OTA doit provenir de la clé privée de signature de release;
  elle doit être passée depuis l’artefact approuvé du signataire, jamais
  générée aléatoirement comme valeur indépendante.
- Le transfert série actuel transporte des secrets en clair. Le fixture doit
  donc être local, contrôlé et isolé; l’outil de provisioning actuel est un
  outil de fixture uniquement et ne doit pas être utilisé sur un lien exposé.
  Il demande la clé racine et le mot de passe MQTT en entrée interactive sans
  les afficher; le flux série reste en clair et doit être considéré comme
  sensible.
- Vérifier l’identité provisionnée, le certificat CA installé et l’absence de
  valeurs de démonstration avant d’autoriser l’appareil à rejoindre le broker.

## TLS SIM7600

Le client configure TLS 1.2, l’authentification du certificat serveur, le
certificat CA provisionné et SNI. `ignorelocaltime` est désactivé afin que la
validation temporelle du certificat ne soit pas contournée. Une date fiable
doit être disponible; en son absence, la connexion doit échouer sans repli
vers une validation permissive.

Le support SIM7600 et sa version de firmware doivent encore être qualifiés
pour confirmer que `authmode=1` valide la chaîne **et** le nom d’hôte/SNI. Le
test de release doit rejeter un certificat expiré, une CA inconnue et un nom
d’hôte différent. Tant que ces essais sur modem ne sont pas concluants, la
validation du nom d’hôte reste un blocage de release.

## OTA

L’OTA reste désactivée en production tant que téléchargement, manifeste,
vérification Ed25519, installation, validation après redémarrage et rollback
ne sont pas livrés et testés de bout en bout.
