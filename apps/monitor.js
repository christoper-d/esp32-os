// =========================================================
// apps/monitor.js - Configuración del Osciloscopio ADC
// =========================================================
function render(tft) {
  tft.statusBar("MONITOR ADC (JS)");
  tft.card(10, 32, 220, 175);
  tft.text("OSCILOSCOPIO GPIO 34", 22, 42, 1, "#00E5FF");
  tft.oscilloscope(16, 65, 208, 120, "#00E676");
  tft.text("Pulsa cualquier botón para salir", 22, 225, 1, "#FFFFFF");
}
