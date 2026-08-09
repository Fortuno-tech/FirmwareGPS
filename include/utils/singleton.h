/**
 * @file    singleton.h
 * @brief   Template singleton générique — Fortico.
 *
 * @warning À utiliser avec parcimonie : la plupart des composants Fortico
 *          (drivers, managers, services) sont possédés explicitement par
 *          SystemController/TaskManager (document 02 §3), pas accédés en
 *          singleton global — préférer l'injection de dépendance. Singleton
 *          reste utile pour de rares composants réellement uniques par
 *          nature (ex. Logger, si son accès global simplifie les macros de
 *          log dans tout le firmware).
 */
#pragma once

namespace fortico {
namespace utils {

template <typename T>
class Singleton {
public:
    static T& instance() {
        static T inst;
        return inst;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    Singleton() = default;
    ~Singleton() = default;
};

}  // namespace utils
}  // namespace fortico
