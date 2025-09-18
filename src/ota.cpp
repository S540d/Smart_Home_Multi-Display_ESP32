#include "ota.h"
#include "display.h"

// Globale OTA-Status Variable
OTAStatus otaStatus;

// ═══════════════════════════════════════════════════════════════════════════════
//                              OTA-INITIALISIERUNG
// ═══════════════════════════════════════════════════════════════════════════════

void initializeOTA() {
  Serial.println("Initialisiere OTA...");
  
  // Hostname setzen
  ArduinoOTA.setHostname(OTAConfig::HOSTNAME);
  
  // Passwort setzen (optional, aber empfohlen)
  ArduinoOTA.setPassword(OTAConfig::PASSWORD);
  
  // Port setzen (optional)
  ArduinoOTA.setPort(OTAConfig::PORT);
  
  // Callback-Funktionen registrieren
  ArduinoOTA.onStart(onOTAStart);
  ArduinoOTA.onProgress(onOTAProgress);
  ArduinoOTA.onEnd(onOTAEnd);
  ArduinoOTA.onError(onOTAError);
  
  // OTA starten
  ArduinoOTA.begin();
  
  Serial.printf("   OTA bereit - Hostname: %s, Port: %d\n", 
               OTAConfig::HOSTNAME, OTAConfig::PORT);
  Serial.printf("   IP-Adresse: %s\n", WiFi.localIP().toString().c_str());
  
  otaStatus.reset();
  otaStatus.isActive = true;
}

void handleOTA() {
  if (!otaStatus.isActive) return;
  
  ArduinoOTA.handle();
  
  // Timeout-Überwachung während Update
  if (otaStatus.isInProgress) {
    unsigned long now = millis();
    if (now - otaStatus.lastActivity > 30000) { // 30 Sekunden Timeout
      Serial.println("WARNUNG - OTA-Timeout erkannt");
      otaStatus.lastError = "Update-Timeout";
      otaStatus.isInProgress = false;
      renderManager.markFullRedrawRequired(); // Display neu zeichnen
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              STATUS-ABFRAGEN
// ═══════════════════════════════════════════════════════════════════════════════

bool isOTAActive() {
  return otaStatus.isActive;
}

int getOTAProgress() {
  return otaStatus.progress;
}

String getOTAStatus() {
  if (!otaStatus.isActive) return "Deaktiviert";
  if (otaStatus.isInProgress) return otaStatus.currentOperation;
  if (!otaStatus.lastError.isEmpty()) return "Fehler: " + otaStatus.lastError;
  return "Bereit";
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              OTA CALLBACK-FUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

void onOTAStart() {
  String type = (ArduinoOTA.getCommand() == U_FLASH) ? "Firmware" : "Dateisystem";
  
  Serial.printf("🚀 OTA-Update gestartet: %s\n", type.c_str());
  
  otaStatus.isInProgress = true;
  otaStatus.startTime = millis();
  otaStatus.progress = 0;
  otaStatus.currentOperation = "Update: " + type;
  otaStatus.lastError = "";
  otaStatus.lastActivity = millis();
  
  // Display für OTA-Modus vorbereiten
  displayOTAProgress();
  
  logOTAStatus();
}

void onOTAProgress(unsigned int progress, unsigned int total) {
  int percentage = (progress * 100) / total;
  
  otaStatus.updateProgress(percentage, otaStatus.currentOperation);
  
  // Alle 10% Progress loggen
  static int lastLoggedPercent = -1;
  if (percentage != lastLoggedPercent && percentage % 10 == 0) {
    Serial.printf("OTA-Progress: %d%% (%u/%u bytes)\n", 
                 percentage, progress, total);
    lastLoggedPercent = percentage;
    
    // Display-Update während OTA
    displayOTAProgress();
  }
}

void onOTAEnd() {
  unsigned long duration = (millis() - otaStatus.startTime) / 1000;
  
  Serial.printf("OTA-Update abgeschlossen in %lu Sekunden\n", duration);
  Serial.println("🔄 Neustart in 3 Sekunden...");
  
  otaStatus.currentOperation = "Abgeschlossen";
  otaStatus.progress = 100;
  
  // Finales Display-Update
  displayOTAProgress();
  
  // Kurz warten, dann Neustart
  delay(3000);
  ESP.restart();
}

void onOTAError(ota_error_t error) {
  String errorMsg = getOTAErrorString(error);
  
  Serial.printf("OTA-Fehler: %s\n", errorMsg.c_str());
  
  otaStatus.lastError = errorMsg;
  otaStatus.isInProgress = false;
  otaStatus.currentOperation = "Fehler";
  
  // Fehler auf Display anzeigen
  displayOTAProgress();
  
  // Nach Fehler normalen Betrieb fortsetzen
  delay(5000);
  renderManager.markFullRedrawRequired();
}

// ═══════════════════════════════════════════════════════════════════════════════
//                              HILFSFUNKTIONEN
// ═══════════════════════════════════════════════════════════════════════════════

void logOTAStatus() {
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                        OTA-STATUS                           ║");
  Serial.println("╠══════════════════════════════════════════════════════════════╣");
  Serial.printf("║ Aktiv       : %s                                           ║\n", 
               otaStatus.isActive ? "Ja" : "Nein");
  Serial.printf("║ In Betrieb  : %s                                           ║\n", 
               otaStatus.isInProgress ? "Ja" : "Nein");
  Serial.printf("║ Operation   : %-45s║\n", otaStatus.currentOperation.c_str());
  Serial.printf("║ Fortschritt : %d%%                                         ║\n", 
               otaStatus.progress);
  
  if (!otaStatus.lastError.isEmpty()) {
    Serial.printf("║ Letzter Fehler: %-40s║\n", otaStatus.lastError.c_str());
  }
  
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
}

void displayOTAProgress() {
  // Ganzen Bildschirm für OTA-Anzeige verwenden
  extern TFT_eSPI tft;
  
  tft.fillScreen(Colors::BG_MAIN);
  
  // Titel
  tft.setTextColor(Colors::TEXT_MAIN);
  tft.drawString("OTA UPDATE", 10, 10, 4);
  
  // Status
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("Status:", 10, 60, 2);
  
  uint16_t statusColor = Colors::TEXT_MAIN;
  if (otaStatus.isInProgress) statusColor = Colors::STATUS_YELLOW;
  if (!otaStatus.lastError.isEmpty()) statusColor = Colors::STATUS_RED;
  if (otaStatus.progress == 100) statusColor = Colors::STATUS_GREEN;
  
  tft.setTextColor(statusColor);
  tft.drawString(otaStatus.currentOperation, 10, 80, 2);
  
  // Fortschrittsbalken
  if (otaStatus.isInProgress || otaStatus.progress > 0) {
    tft.setTextColor(Colors::TEXT_LABEL);
    tft.drawString("Fortschritt:", 10, 120, 2);
    
    // Progress Bar
    int barWidth = 280;
    int barHeight = 20;
    int barX = 20;
    int barY = 145;
    
    // Rahmen
    tft.drawRect(barX, barY, barWidth, barHeight, Colors::BORDER_MAIN);
    tft.fillRect(barX + 1, barY + 1, barWidth - 2, barHeight - 2, Colors::BG_MAIN);
    
    // Fortschritt
    int progressWidth = (barWidth - 2) * otaStatus.progress / 100;
    if (progressWidth > 0) {
      uint16_t progressColor = otaStatus.progress == 100 ? 
                              Colors::STATUS_GREEN : Colors::STATUS_YELLOW;
      tft.fillRect(barX + 1, barY + 1, progressWidth, barHeight - 2, progressColor);
    }
    
    // Prozent-Anzeige
    tft.setTextColor(Colors::TEXT_MAIN);
    String percentText = String(otaStatus.progress) + "%";
    tft.drawString(percentText, barX + barWidth/2 - 15, barY + 25, 2);
  }
  
  // Fehler-Anzeige
  if (!otaStatus.lastError.isEmpty()) {
    tft.setTextColor(Colors::STATUS_RED);
    tft.drawString("Fehler:", 10, 180, 2);
    tft.drawString(otaStatus.lastError, 10, 200, 1);
  }
  
  // IP-Adresse für Debugging
  tft.setTextColor(Colors::TEXT_LABEL);
  tft.drawString("IP: " + WiFi.localIP().toString(), 10, 220, 1);
}

String getOTAErrorString(ota_error_t error) {
  switch (error) {
    case OTA_AUTH_ERROR:    return "Authentifizierung fehlgeschlagen";
    case OTA_BEGIN_ERROR:   return "Begin-Fehler";
    case OTA_CONNECT_ERROR: return "Verbindungsfehler";
    case OTA_RECEIVE_ERROR: return "Empfangsfehler";
    case OTA_END_ERROR:     return "End-Fehler";
    default:                return "Unbekannter Fehler (" + String(error) + ")";
  }
}