/**
 * @file    auth_types.h
 * @brief   Types d'authentification — conducteur (RFID) ET device/cloud
 *          (JWT) — Fortico.
 *
 * @note    Deux enums distincts, tous deux nécessaires (pas un remplacement
 *          l'un de l'autre) :
 *            - AuthenticationResult : résultat d'UNE lecture de badge
 *              (ponctuel, remonté par RfidDriver vers AuthenticationManager)
 *            - AuthState : état PERSISTANT porté par AuthenticationManager
 *              (Document 06 §3) entre deux lectures.
 *          La proposition initiale ne couvrait que le premier ; le second
 *          était déjà spécifié au Document 06 §3 et est ajouté ici pour
 *          cohérence.
 */
#pragma once

namespace fortico {
namespace types {

/// Résultat d'une lecture de badge RFID (RfidDriver -> AuthenticationManager)
enum class AuthenticationResult {
    AUTHORIZED,
    DENIED,
    UNKNOWN_CARD,
    EXPIRED,     ///< [ASSUMPTION] notion d'expiration de badge non détaillée dans les documents sources
};

/// État persistant d'AuthenticationManager (Document 06 §3)
enum class AuthState {
    UNAUTHENTICATED,
    AUTHENTICATED,
    AUTH_LOST,   ///< badge retiré en cours de conduite — ne coupe jamais le relais directement (Document 02 §5.2)
};

}  // namespace types
}  // namespace fortico
