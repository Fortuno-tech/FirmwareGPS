/**
 * @file    macros.h
 * @brief   Macros utilitaires génériques — Fortico.
 *
 * @warning [CORRECTION] La proposition initiale définissait ces macros
 *          vides (#define UNUSED(x) sans corps), ce qui ne produit AUCUN
 *          effet et laisserait par exemple les avertissements "variable
 *          inutilisée" du compilateur intacts. Implémentations correctes
 *          ci-dessous.
 *
 */
#pragma once

#define FORTICO_UNUSED(x) ((void)(x))
#define FORTICO_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define FORTICO_BIT(x) (1u << (x))
#define FORTICO_KB(x) ((x) * 1024u)
#define FORTICO_MB(x) ((x) * 1024u * 1024u)
