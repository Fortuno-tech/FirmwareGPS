#include <cassert>
#include <cstdio>

#include "utils/timer_utils.h"
#include "utils/timer_utils.cpp"

using namespace fortico;

// Horloge factice controlee par le test, pour un temps deterministe.
namespace {
uint32_t g_fakeNow = 0;
uint32_t fakeClock() { return g_fakeNow; }
}

int main() {
    utils::Timer::setClockSourceForTesting(&fakeClock);

    // --- Test 1: millis() reflete bien la source injectee ---
    g_fakeNow = 1000;
    assert(utils::Timer::millis() == 1000);
    g_fakeNow = 5000;
    assert(utils::Timer::millis() == 5000);

    // --- Test 2: timeout() cas normal, delai non atteint ---
    uint32_t start = 1000;
    g_fakeNow = 1500;  // 500ms ecoulees
    assert(utils::Timer::timeout(start, 1000) == false && "500ms < 1000ms, timeout ne doit pas etre atteint");

    // --- Test 3: timeout() cas normal, delai tout juste atteint ---
    g_fakeNow = 2000;  // exactement 1000ms ecoulees
    assert(utils::Timer::timeout(start, 1000) == true && "1000ms >= 1000ms, timeout doit etre atteint (limite inclusive)");

    // --- Test 4: timeout() cas normal, delai depasse ---
    g_fakeNow = 3000;  // 2000ms ecoulees
    assert(utils::Timer::timeout(start, 1000) == true);

    // --- Test 5 (LE POINT CRITIQUE) : wraparound de l'entier 32 bits ---
    // start proche de UINT32_MAX, now ayant deborde a une petite valeur.
    // C'est exactement le scenario qui arrive apres ~49.7 jours de
    // fonctionnement continu (2^32 ms) - un firmware embarque DOIT survivre
    // a ce cas sans watchdog reset intempestif ni blocage.
    uint32_t startNearMax = 0xFFFFFFF0;  // 16 ms avant le debordement
    g_fakeNow = 5;                        // 5ms APRES le debordement
    // Ecart reel = 16 (jusqu'au wrap) + 5 (apres le wrap) = 21ms
    assert(utils::Timer::timeout(startNearMax, 21) == true && "21ms ecoulees (avec wraparound) >= 21ms doit etre vrai");
    assert(utils::Timer::timeout(startNearMax, 22) == false && "21ms ecoulees < 22ms doit etre faux, meme avec wraparound");
    assert(utils::Timer::timeout(startNearMax, 20) == true && "21ms >= 20ms doit etre vrai");

    // --- Test 6: resetClockSourceToDefault() ne doit pas planter ---
    utils::Timer::resetClockSourceToDefault();
    uint32_t realNow = utils::Timer::millis();
    (void)realNow;  // pas d'assertion sur la valeur reelle, juste verifier l'absence de crash

    // --- Test 7: micros() ne doit pas planter (pas de mock pour micros ici) ---
    uint64_t nowUs = utils::Timer::micros();
    (void)nowUs;

    printf("Tous les tests Timer sont passes (7/7), y compris le wraparound 32 bits.\n");
    return 0;
}