/**
 * @file    version.h
 * @brief   Version numérique du firmware — Fortico.
 *
 * @note    [CHEVAUCHEMENT RÉSOLU] project_config.h (Document Sprint 0 §6)
 *          définit déjà FIRMWARE_VERSION en chaîne ("1.0.0"). Ce fichier
 *          devient la source AUTORITATIVE des composants numériques
 *          (MAJOR/MINOR/PATCH), à partir desquels la chaîne est dérivée.
 *          project_config.h::FIRMWARE_VERSION doit être considéré comme
 *          dérivé de ces constantes — à harmoniser lors de la prochaine
 *          révision de project_config.h (actuellement une chaîne littérale
 *          indépendante, non générée depuis version.h).
 */
#pragma once

#include "config/project_config.h"

namespace fortico {
namespace constants {

constexpr const char* FW_VERSION_STRING = project::FIRMWARE_VERSION;

}  // namespace constants
}  // namespace fortico
