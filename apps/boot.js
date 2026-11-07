// =========================================================
// boot.js - Se ejecuta automáticamente al encender el ESP32
// =========================================================
function onBoot(tft, system) {
  tft.fillScreen("#000000");

  // 1. Pulso de radar y logo central
  tft.drawCircle(120, 85, 30, system.theme.accent);
  tft.drawRoundRect(106, 71, 28, 28, 6, system.theme.accentBg, system.theme.accent);
  tft.fillRect(116, 81, 8, 8, system.theme.accent);

  // 2. Textos de bienvenida
  tft.drawText("G4BRIEL OS", 62, 130, 2, "#FFFFFF");
  tft.drawText("JS Engine • GitHub OTA", 46, 154, 1, system.theme.textSecondary);

  // 3. Pasos de carga del sistema
  return [
    "Iniciando SPI & LittleFS...",
    "Leyendo WiFi de NVS Flash...",
    "Sincronizando manifest.json...",
    "Cargando orden de menus...",
    "¡G4BRIEL OS Listo!"
  ];
}
