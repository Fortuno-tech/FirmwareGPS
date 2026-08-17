/**
 * @file    logger.cpp
 * @brief   Implémentation de Logger — Fortico.
 */
#include "core/logger/logger.h"

#include <cstdio>
#include <cstring>

namespace fortico {
namespace core {

Logger::Logger()
    : _queue(nullptr),
      _taskHandle(nullptr),
      _sink(nullptr),
      _minLevel(logging::Level::LOG_INFO),
      _droppedCount(0),
      _initialized(false) {
}

Logger::~Logger() {
    // [ASSUMPTION] Comme EventBus (voir core/event_bus/event_bus.cpp) :
    // Logger est un singleton vivant jusqu'à la coupure d'alimentation en
    // production. Destructeur présent pour la propreté RAII et les tests
    // natifs (ASan/LeakSanitizer), pas pour un scénario réel de destruction.
    if (_queue != nullptr) {
        vQueueDelete(_queue);
    }
}

bool Logger::init(logging::Level minLevel, ILogSink* sink) {
    if (sink == nullptr) {
        return false;
    }

    _queue = xQueueCreate(logging::QUEUE_LEN_LOGGER, sizeof(LogEntry));
    if (_queue == nullptr) {
        return false;
    }

    _sink = sink;
    _minLevel = minLevel;
    _droppedCount = 0;
    _initialized = true;
    return true;
}

bool Logger::start() {
    if (!_initialized) {
        return false;
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        &Logger::taskEntry,
        "LoggerTask",
        tasks::LOGGER_TASK_STACK_WORDS,
        this,
        tasks::LOGGER_TASK_PRIORITY,
        &_taskHandle,
        tasks::CORE_NETWORK);  // Core 0, Document 07 §1

    return result == pdTRUE;
}

void Logger::setMinLevel(logging::Level level) {
    _minLevel = level;
}

void Logger::log(logging::Level level, const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logVariadic(level, tag, fmt, args);
    va_end(args);
}

void Logger::error(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logVariadic(logging::Level::LOG_ERROR, tag, fmt, args);
    va_end(args);
}

void Logger::warn(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logVariadic(logging::Level::LOG_WARN, tag, fmt, args);
    va_end(args);
}

void Logger::info(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logVariadic(logging::Level::LOG_INFO, tag, fmt, args);
    va_end(args);
}

void Logger::debug(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logVariadic(logging::Level::LOG_DEBUG, tag, fmt, args);
    va_end(args);
}

void Logger::logVariadic(logging::Level level, const char* tag, const char* fmt, va_list args) {
    if (!_initialized) {
        return;
    }

    // Filtrage précoce : ne pas formater du tout si le niveau est masqué —
    // évite le coût de vsnprintf() pour des logs DEBUG en production
    // (Level::LOG_DEBUG a la valeur numérique la PLUS HAUTE, donc "plus
    // verbeux que _minLevel" se traduit par level > _minLevel).
    if (static_cast<uint8_t>(level) > static_cast<uint8_t>(_minLevel)) {
        return;
    }

    LogEntry entry{};
    entry.level = level;
    entry.timestampMs = 0;  // TODO(Sprint 1, utils::Timer) : renseigner via Timer::millis() une fois lié

    std::strncpy(entry.tag, tag != nullptr ? tag : "?", sizeof(entry.tag) - 1);
    entry.tag[sizeof(entry.tag) - 1] = '\0';

    // vsnprintf borne TOUJOURS l'écriture à sizeof(entry.message), quel que
    // soit le contenu formaté — pas de dépassement possible même si fmt+args
    // produiraient une chaîne plus longue (troncature silencieuse acceptée,
    // préférable à un débordement mémoire, Document 03 §6).
    std::vsnprintf(entry.message, sizeof(entry.message), fmt, args);

    // Timeout 0 : ne bloque JAMAIS l'appelant (R-01). Si la file est pleine,
    // le message est perdu et comptabilisé — jamais de blocage d'une tâche
    // métier pour un simple log.
    if (xQueueSend(_queue, &entry, 0) != pdTRUE) {
        _droppedCount++;
    }
}

bool Logger::pumpOnce() {
    LogEntry entry{};
    if (xQueueReceive(_queue, &entry, 0) == pdTRUE) {
        writeEntry(entry);
        return true;
    }
    return false;
}

void Logger::taskEntry(void* param) {
    static_cast<Logger*>(param)->run();
}

void Logger::run() {
    for (;;) {
        LogEntry entry{};
        if (xQueueReceive(_queue, &entry, portMAX_DELAY) == pdTRUE) {
            writeEntry(entry);
        }
        // TODO(Sprint 1, Watchdog) : feed watchdog ici si LoggerTask est un
        // jour enregistrée (Document 07 §6 ne l'y liste pas actuellement).
    }
}

void Logger::writeEntry(const LogEntry& entry) {
    if (_sink == nullptr) {
        return;
    }

    char line[logging::LOG_LINE_MAX_LENGTH + 32];
    
    std::snprintf(line, sizeof(line), "[%8lu][%s][%s] %s",
                  static_cast<unsigned long>(entry.timestampMs),
                  levelToString(entry.level),
                  entry.tag,
                  entry.message);

    _sink->write(line);
}

const char* Logger::levelToString(logging::Level level) {
    switch (level) {
        case logging::Level::LOG_ERROR: return "ERROR";
        case logging::Level::LOG_WARN:  return "WARN ";
        case logging::Level::LOG_INFO:  return "INFO ";
        case logging::Level::LOG_DEBUG: return "DEBUG";
        default:                        return "?????";
    }
}

}  // namespace core
}  // namespace fortico