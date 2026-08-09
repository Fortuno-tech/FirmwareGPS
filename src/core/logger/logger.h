/**
 * @file    logger.h
 * @brief   Logger texte (debug/diagnostic) — Fortico.
 *
 * @details Distinct de la boîte noire structurée (StorageService/
 *          StorageManager, LogRecord, Document 09 §7) — voir
 *          logging_config.h §4. Logger produit un flux texte lisible
 *          (niveau + tag + message), typiquement vers la console série.
 *
 * @details Tout appel à Logger::log()/error()/warn()/info()/debug() est un
 *          PRODUCTEUR : le message est formaté dans un buffer borné puis
 *          empilé dans une file FreeRTOS (non bloquant, timeout 0 — jamais
 *          d'attente, cohérent avec R-01). Un unique consommateur
 *          (LoggerTask, Document 07 §2) dépile et écrit réellement vers le
 *          sink configuré — ceci évite toute écriture concurrente sur
 *          Serial depuis plusieurs tâches à la fois.
 *
 * @note    ILogSink est injecté (pas de dépendance directe à Serial/Arduino
 *          dans ce fichier) : permet un sink réel (SerialLogSink, écrit dans
 *          le .cpp d'intégration Arduino) et un sink de test (capture en
 *          mémoire, voir test/unit/test_logger.cpp) sans recompilation
 *          conditionnelle.
 */
#pragma once

#include <cstdint>
#include <cstdarg>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "config/logging_config.h"
#include "config/task_config.h"

namespace fortico {
namespace core {

/// Destination réelle des lignes de log formatées — implémentée séparément
/// selon l'environnement (Serial en production, buffer en test natif).
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void write(const char* line) = 0;
};

class Logger {
public:
    Logger();
    ~Logger();

    /// Configure le niveau minimal et le sink. N'alloue rien tant que
    /// start() n'est pas appelé. sink doit rester valide pendant toute la
    /// durée de vie du Logger (pas de possession — ownership au démarrage).
    bool init(logging::Level minLevel, ILogSink* sink);

    /// Crée LoggerTask (Document 07 §2 : Core 0, priorité 1).
    bool start();

    /// Change le niveau minimal à chaud (ex. ConfigService, document 07 §4bis.3)
    void setMinLevel(logging::Level level);

    // ------------------------------------------------------------------
    // API de journalisation — non bloquante, style printf borné
    // ------------------------------------------------------------------

    void log(logging::Level level, const char* tag, const char* fmt, ...);

    void error(const char* tag, const char* fmt, ...);
    void warn(const char* tag, const char* fmt, ...);
    void info(const char* tag, const char* fmt, ...);
    void debug(const char* tag, const char* fmt, ...);

    /// Nombre de messages perdus faute de place dans la file depuis le
    /// dernier démarrage — exposé pour DiagnosticsManager (document 06 §10).
    uint32_t droppedCount() const { return _droppedCount; }

    /// [Document 11 — support de test natif] Dépile et écrit AU PLUS UNE
    /// ligne. Réservé aux tests ; la production utilise start()/run().
    bool pumpOnce();

private:
    struct LogEntry {
        logging::Level level;
        char tag[16];
        char message[logging::LOG_LINE_MAX_LENGTH];
        uint32_t timestampMs;
    };

    void logVariadic(logging::Level level, const char* tag, const char* fmt, va_list args);
    void writeEntry(const LogEntry& entry);
    static const char* levelToString(logging::Level level);
    static void taskEntry(void* param);
    void run();

    QueueHandle_t _queue;
    TaskHandle_t _taskHandle;
    ILogSink* _sink;
    logging::Level _minLevel;
    uint32_t _droppedCount;
    bool _initialized;
};

}  // namespace core
}  // namespace fortico