#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
//                       LovyanGFX-Treiberkonfiguration (Issue #22)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Ersetzt TFT_eSPI als Display-Treiber. Die Pin-/Panel-/Bus-Parameter sind 1:1 aus
// den bisherigen TFT_eSPI-Build-Flags (platformio.ini) übernommen, damit sich am
// gerenderten Bild nichts ändert. Hardware: JC2432W328 (ESP32-WROOM, ST7789 320x240).

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;

public:
  LGFX() {
    // ── SPI-Bus ────────────────────────────────────────────────────────────────
    // Pins 13/14/15 sind die HSPI-Default-Pins des ESP32 → HSPI_HOST.
    {
      auto cfg = _bus.config();
      cfg.spi_host    = HSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 55000000;   // war SPI_FREQUENCY
      cfg.freq_read   = 20000000;   // war SPI_READ_FREQUENCY
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 14;         // war TFT_SCLK
      cfg.pin_mosi    = 13;         // war TFT_MOSI
      cfg.pin_miso    = -1;
      cfg.pin_dc      = 2;          // war TFT_DC
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    // ── Panel (ST7789) ──────────────────────────────────────────────────────────
    {
      auto cfg = _panel.config();
      cfg.pin_cs          = 15;     // war TFT_CS
      cfg.pin_rst         = -1;     // war TFT_RST (nicht angeschlossen)
      cfg.pin_busy        = -1;
      cfg.panel_width     = 240;    // war TFT_WIDTH
      cfg.panel_height    = 320;    // war TFT_HEIGHT
      cfg.offset_x        = 0;
      cfg.offset_y        = 0;
      cfg.offset_rotation = 0;
      cfg.readable        = false;
      cfg.invert          = false;  // entspricht invertDisplay(false)
      cfg.rgb_order       = false;  // false = BGR (war TFT_RGB_ORDER=TFT_BGR)
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = false;  // Bus wird nur vom Display genutzt (Touch = I2C)
      _panel.config(cfg);
    }

    // ── Backlight (PWM, aktiv HIGH) ──────────────────────────────────────────────
    {
      auto cfg = _light.config();
      cfg.pin_bl      = 27;         // war TFT_BL (TFT_BACKLIGHT_ON=HIGH)
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;          // frei — App nutzt kein LEDC
      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);
  }
};
