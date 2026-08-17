/**
 * @file    timer_utils.cpp
 * @brief   Implémentation de Timer — Fortico.
 */
#include "utils/timer_utils.h"

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <chrono>
#endif

namespace fortico {
namespace utils {

namespace {

uint32_t realMillis() {
#ifdef ARDUINO
    return ::millis();
#else
    // Fallback natif (tests unitaires, Document 11) : horloge monotone du
    // process, non liée à l'horloge murale — suffisant pour tester la
    // logique de timeout()/wraparound sans matériel.
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now() - start).count());
#endif
}

// État global partagé, volontairement : Timer est une classe à méthodes
// statiques (pas d'instance), cohérent avec son usage prévu comme simple
// indirection utilitaire dans tout le firmware (Document 03).
Timer::ClockSourceFn g_clockSource = &realMillis;

}  // namespace

void Timer::setClockSourceForTesting(ClockSourceFn fn) {
    g_clockSource = (fn != nullptr) ? fn : &realMillis;
}

void Timer::resetClockSourceToDefault() {
    g_clockSource = &realMillis;
}

uint32_t Timer::millis() {
    return g_clockSource();
}

uint64_t Timer::micros() {
#ifdef ARDUINO
    return ::micros();
#else
    using namespace std::chrono;
    static const auto start = steady_clock::now();
    return static_cast<uint64_t>(duration_cast<microseconds>(steady_clock::now() - start).count());
#endif
}

bool Timer::timeout(uint32_t start, uint32_t delayMs) {
    // Arithmétique non signée : (now - start) reste correct même si millis()
    // a débordé (wraparound) entre start et maintenant — pas de branchement
    // spécial nécessaire, c'est une propriété du type uint32_t lui-même.
    uint32_t now = millis();
    uint32_t elapsed = now - start;
    return elapsed >= delayMs;
}

}  // namespace utils
}  // namespace fortico