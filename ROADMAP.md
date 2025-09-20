# Multidisplay_ESP32_Rev2 Roadmap

Diese Roadmap enthält Vorschläge und geplante Features für die Weiterentwicklung des Projekts. Sie kann direkt als Basis für GitHub Issues verwendet werden.

## Bereits implementiert
- NTP-Zeitsynchronisation
- MQTT-Kommunikation (Topics, Verbindung, Callback)
- Sensor-Datenverarbeitung (PV, Grid, Batterie, etc.)
- Display-Layouts und Visualisierung
- Fehler- und Statusanzeigen
- Konfigurationsmanagement
- Energiemanagement (PV, Batterie, Grid)
- Logging (Serial-Ausgaben, Statusmeldungen)

## Vorschläge für die Weiterentwicklung

### 1. OTA-Update
- [ ] OTA-Update-Funktion vollständig integrieren und testen

### 2. Web-Interface
- [ ] Web-Interface zur Live-Anzeige und Konfiguration (z.B. ESPAsyncWebServer)

### 3. Energiesparmodus
- [ ] Sleep-Funktionen für den ESP32 implementieren

### 4. Sensor-Erweiterung
- [ ] Unterstützung für weitere Sensortypen (CO₂, Luftfeuchtigkeit, Temperatur, Helligkeit)

### 5. Alarm- und Benachrichtigungen
- [ ] Schwellenwerte und Benachrichtigungen (MQTT/Email)

### 6. Mehrsprachigkeit
- [ ] Mehrsprachige Benutzeroberfläche für Display und Web-Interface

### 7. Datenexport/-import
- [ ] Export/Import von Sensordaten (CSV/JSON über Web oder SD-Karte)

### 8. Bluetooth/BLE
- [ ] Integration für mobile Steuerung und Konfiguration

### 9. Modularisierung
- [ ] Unterstützung für mehrere Displays und Erweiterungsboards

### 10. Fehlerdiagnose
- [ ] Erweiterte Fehlerdiagnose und Selbsttest-Routinen (Sensor-Check, Display-Test)

### 11. Visualisierung
- [ ] Animationen und Icons für ansprechende Darstellung

### 12. Cloud-Integration
- [ ] WebDAV, SMB, FTP, Google Drive, etc. anbinden

### 13. API/REST
- [ ] REST-Schnittstelle für externe Systeme und Smart Home

### 14. Dokumentation
- [ ] Ausführliche Dokumentation und Beispielprojekte

### 15. Zeitsynchronisation
- [ ] Alternative Quellen (DCF77, GPS) integrieren

### 16. User- und Rollenmanagement
- [ ] Benutzerverwaltung für Web-Interface (Admin/User)

### 17. Test- und Diagnosefunktionen
- [ ] Test- und Diagnosefunktionen für Hardware und Software

---
Diese Roadmap kann nach Bedarf erweitert und als Grundlage für GitHub Issues genutzt werden.