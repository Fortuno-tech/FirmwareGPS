/**
 * @file    main.cpp
 * @brief   Point d'entrée Arduino — Fortico ESP32.
 *          setup()/loop() obligatoires pour le framework Arduino.
 *          Toute la logique tourne dans des tâches FreeRTOS lancées ici.
 */
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    Serial.println("Demarrage Fortico...");

}

void loop() {

    vTaskDelay(pdMS_TO_TICKS(1000));
}