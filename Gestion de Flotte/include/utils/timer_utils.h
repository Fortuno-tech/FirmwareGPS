    /**
     * @file    timer_utils.h
     * @brief   Utilitaires de temps — Fortico.
     */
    #pragma once

    #include <cstdint>

    namespace fortico {
    namespace utils {

    class Timer {
    public:
        using ClockSourceFn = uint32_t (*)();

        /// [Test uniquement] Injecte une horloge factice déterministe.
        /// JAMAIS appelé en production.
        static void setClockSourceForTesting(ClockSourceFn fn);

        /// Restaure l'horloge réelle.
        /// À appeler en teardown de test pour ne pas polluer le test suivant.
        static void resetClockSourceToDefault();

        static uint32_t millis();
        static uint64_t micros();

        /// Vrai si (now - start) >= delayMs.
        /// Wraparound-safe par arithmétique non signée 32 bits.
        static bool timeout(uint32_t start, uint32_t delayMs);
    };

    }  // namespace utils
    }  // namespace fortico