/**
 * @file    ota_config.h
 * @brief   Politique d'activation OTA — Fortico.
 *
 * L'OTA reste désactivée tant que le téléchargement, la vérification de
 * signature et l'installation ne sont pas implémentés de bout en bout.
 */
#pragma once

namespace fortico {
namespace ota {

/// Le chemin OTA complet (téléchargement, vérification, installation) n'est
/// pas encore implémenté.
constexpr bool UPDATE_PIPELINE_COMPLETE = false;

/// Kill-switch de production : l'OTA ne peut être activée avant livraison du
/// chemin complet et une décision explicite de configuration.
constexpr bool ENABLED_IN_PRODUCTION = false;

static_assert(!ENABLED_IN_PRODUCTION || UPDATE_PIPELINE_COMPLETE,
              "Production OTA requires a complete update pipeline");

}  // namespace ota
}  // namespace fortico
