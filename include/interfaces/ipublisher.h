    /**
     * @file    ipublisher.h
     * @brief   Contrat d'émission d'événement EventBus — Fortico.
     *
     * @note    [CHANGÉ 13/07/2026] publish() retourne désormais
     *          constants::PublishResult au lieu d'un bool — un échec de
     *          publication peut avoir plusieurs causes distinctes (file pleine,
     *          bus non initialisé), utiles à distinguer en diagnostic/test.
     */
    #pragma once

    #include "types/system_types.h"
    #include "constants/events.h"

    namespace fortico {
    namespace interfaces {

    class IPublisher {
    public:
        virtual ~IPublisher() = default;
        virtual constants::PublishResult publish(const types::Event& event) = 0;
    };

    }  // namespace interfaces
    }  // namespace fortico