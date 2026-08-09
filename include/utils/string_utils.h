/**
 * @file    string_utils.h
 * @brief   Utilitaires de chaînes de caractères — Fortico.
 *
 * @details Usages attendus : parsing de réponses AT (Sim7600Driver,
 *          Document 04 §5), payloads MQTT/JSON (Document 05, 08, 09),
 *          UID RFID (RfidDriver), clés NVS et valeurs de configuration.
 *          Regroupé ici pour éviter que chaque driver/service ne réimplémente
 *          sa propre version de trim()/startsWith() (source d'incohérences
 *          et de bugs subtils déjà vus dans des firmwares similaires : ex.
 *          un trim() qui ne gère pas \r en plus de \n dans une réponse AT).
 *
 * @note    [AJOUT] contains() ajouté en complément des 4 méthodes demandées
 *          (trim/startsWith/endsWith/equalsIgnoreCase) : usage très fréquent
 *          pour repérer un marqueur dans une réponse AT (ex. "OK", "ERROR",
 *          "+CGPSINFO:") sans vouloir extraire une sous-chaîne précise.
 */
#pragma once

#include <string>

namespace fortico {
namespace utils {

class StringUtils {
public:
    /// Retire les espaces, tabulations, \r et \n en début et fin de chaîne
    /// (les réponses AT du SIM7600E-L1C terminent typiquement par \r\n)
    static std::string trim(const std::string& str);

    static bool startsWith(const std::string& str, const std::string& prefix);
    static bool endsWith(const std::string& str, const std::string& suffix);
    static bool equalsIgnoreCase(const std::string& a, const std::string& b);

    /// [AJOUT] Repère un marqueur sans extraction (ex. contains(resp, "ERROR"))
    static bool contains(const std::string& str, const std::string& needle);
};

}  // namespace utils
}  // namespace fortico