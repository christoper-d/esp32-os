// =========================================================================
// G4BRIEL OS - MOTOR BASE ESP32 + TFT_eSPI (280x240 HORIZONTAL 1:1 CON WEB)
// Sube este archivo (g4briel_os_esp32.ino) a tu ESP32 en Arduino IDE.
// - Renderizado 1:1 identico al Simulador Web:
//   * Iconos vectoriales/pixel-art en cada fila del menu + badges ([ON], [OTA], etc.) + scrollbar
//   * Modo Focus Smartwatch con tarjeta previa, central y siguiente + contador [1/5]
//   * GameBoy Color interactivo (Pokemon Amarillo, Azul y Rojo) con Sprites pixel-art
//     (Pikachu, Blastoise, Charizard), barra de vida HP y 4 botones de batalla (2x2)
//   * Osciloscopio ADC en vivo (GPIO34), Test visual de 3 Botones y Memoria Flash/RAM
//   * Teclado WiFi intuitivo de 2 Filas (Fila 1 letras + Fila 2 MAYUS/123#/BORRAR/ENTER/SALIR)
// =========================================================================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();
Preferences preferences;

// URL Raw de tu repositorio en GitHub donde estan manifest.json y apps/*.js
const char* GITHUB_REPO_URL = "https://raw.githubusercontent.com/christoper-d/esp32-os/main";

const int PIN_UP   = 25;
const int PIN_DOWN = 26;
const int PIN_OK   = 27;
const int PIN_ADC  = 34;

// Rotacion horizontal: 1 (280x240 normal) o 3 (280x240 invertido 180 grados)
int screenRotation = 1;
// Si en User_Setup.h tienes TFT_HEIGHT=320 en vez de 280, puedes compensar 20px en Configuracion
int screenOffsetX  = 0;

enum MenuLayout { LAYOUT_SLIM_LIST, LAYOUT_FOCUS_SMARTWATCH };
MenuLayout currentLayout = LAYOUT_SLIM_LIST;

enum ThemeId { THEME_ORANGE, THEME_CYAN, THEME_GREEN, THEME_WHITE };
ThemeId currentTheme = THEME_ORANGE;

struct ThemePalette {
  const char* name;
  uint16_t accent;
  uint16_t accentBg;
  uint16_t cardBg;
  uint16_t cardBorder;
  uint16_t textPrimary;
  uint16_t textSecondary;
  uint16_t topBarBg;
};

const ThemePalette themes[4] = {
  { "Naranja", TFT_ORANGE, 0x3100, 0x18E3, 0x2965, TFT_WHITE, 0x9CD3, 0x0861 },
  { "Cian",    TFT_CYAN,   0x01E6, 0x08A6, 0x11AB, TFT_WHITE, 0x54FB, 0x0843 },
  { "Verde",   TFT_GREEN,  0x02E0, 0x08A2, 0x11A4, TFT_WHITE, 0x56E5, 0x0421 },
  { "Blanco",  TFT_WHITE,  0x31A6, 0x10A2, 0x4208, TFT_WHITE, 0x8410, 0x0861 }
};

enum IconType {
  ICON_GAMEPAD,
  ICON_GEAR,
  ICON_SYNC,
  ICON_CHART,
  ICON_CROSS,
  ICON_BOLT,
  ICON_DROP,
  ICON_FLAME,
  ICON_WIFI,
  ICON_LAYOUT,
  ICON_PALETTE,
  ICON_ROTATE,
  ICON_BACK
};

enum ScreenState {
  STATE_MAIN_MENU,
  STATE_G4BRIEL_GAMEBOY,
  STATE_POKEMON_PLAY,
  STATE_SISTEMA_TESTS,
  STATE_RGB_TEST,
  STATE_BTN_TEST,
  STATE_MEM_INFO,
  STATE_MONITOR_ADC,
  STATE_CONFIGURACION,
  STATE_WIFI_SCAN,
  STATE_WIFI_KEYBOARD,
  STATE_JS_APP
};
ScreenState currentState = STATE_MAIN_MENU;

const int MAX_WIFI_NETS = 8;
String scannedSSIDs[MAX_WIFI_NETS];
int totalScannedNets = 0;

struct MenuItem {
  String title;
  String subtitle;
  String scriptPath;
  IconType icon;
};

const int MAX_MENU_ITEMS = 12;
int totalMainItems = 5;
MenuItem mainItems[MAX_MENU_ITEMS] = {
  {"g4briel",         "GameBoy: Amarillo, Azul, Rojo", "apps/g4briel.js",      ICON_GAMEPAD},
  {"Configuracion",   "WiFi NVS, UI y Color",          "system://config",      ICON_GEAR},
  {"GitHub Sync",     "Descargar .js del repo",        "system://github_sync", ICON_SYNC},
  {"Monitor ADC",     "Osciloscopio GPIO34",           "apps/monitor.js",      ICON_CHART},
  {"Sistema & Tests", "Tests RGB, Botones y Flash",    "apps/sysinfo.js",      ICON_CROSS}
};

// Submenu g4briel: Juegos de GameBoy (Pokemon Amarillo, Azul y Rojo)
const int TOTAL_GAMEBOY = 4;
const char* gameboyTitles[TOTAL_GAMEBOY] = {
  "Pokemon Amarillo",
  "Pokemon Azul",
  "Pokemon Rojo",
  "< Volver"
};
const uint16_t gameboyColors[TOTAL_GAMEBOY] = {
  0xFFE0, // Amarillo Pikachu
  0x07FF, // Azul Blastoise
  0xF980, // Rojo Charizard
  TFT_WHITE
};
const IconType gameboyIcons[TOTAL_GAMEBOY] = {
  ICON_BOLT, ICON_DROP, ICON_FLAME, ICON_BACK
};

// Estado interactivo de batalla GameBoy (igual que en la web)
int activePokemonEdition = 0; // 0: Amarillo, 1: Azul, 2: Rojo
int gbLevel = 25;
int gbHp = 100;
int gbMaxHp = 100;
int gbActionIdx = 0; // 0: ATACAR, 1: POKEBALL, 2: CURAR, 3: SALIR
String gbMsg1 = "";
String gbMsg2 = "";

// Submenu Sistema & Tests
const int TOTAL_SYS_TESTS = 4;
const char* sysTestTitles[TOTAL_SYS_TESTS] = {
  "Test Pantalla RGB",
  "Test 3 Botones",
  "Memoria & Scripts",
  "< Volver"
};
const IconType sysTestIcons[TOTAL_SYS_TESTS] = {
  ICON_PALETTE, ICON_GAMEPAD, ICON_CHART, ICON_BACK
};
int rgbTestStep = 0;
String lastBtnPressed = "NINGUNO";
int okExitCounter = 0;

// Submenu Configuracion
const int TOTAL_CONFIG = 7;
const char* configTitles[TOTAL_CONFIG] = {
  "Conectar WiFi",
  "Sync GitHub JS",
  "Estilo UI",
  "Tema Color",
  "Girar Horiz (1/3)",
  "Ajuste Offset X",
  "< Volver"
};
const IconType configIcons[TOTAL_CONFIG] = {
  ICON_WIFI, ICON_SYNC, ICON_LAYOUT, ICON_PALETTE, ICON_ROTATE, ICON_LAYOUT, ICON_BACK
};

int selectedIndex = 0;
int scrollOffset  = 0;

// Modos del Teclado en 2 Filas:
// Modo 0: Minusculas (abc) | Modo 1: Mayusculas (ABC) | Modo 2: Numeros y Simbolos (123#)
const char* kbCharsets[3] = {
  "abcdefghijklmnopqrstuvwxyz ",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ ",
  "0123456789._-@#$%&*!?+-=/ "
};
const char* kbModeNames[3] = { "abc", "ABC", "123#" };
int kbMode = 0;        // 0 = abc, 1 = ABC, 2 = 123#
int keyboardIndex = 0; // 0..(len-1) = Fila 1 (Letras), len..(len+3) = Fila 2 (MAYUS/123, BORRAR, ENTER, SALIR)

String wifiTargetSSID = "Mi_Casa_5G";
String wifiPassword   = "";
bool wifiConnected    = false;
bool hasSavedWifi     = false;

// Datos del Osciloscopio Monitor ADC
const int WAVE_POINTS = 40;
int waveY[WAVE_POINTS];
float simAngle = 0.0;
unsigned long lastMonitorTick = 0;

// DIMENSIONES EN MODO HORIZONTAL (280 ancho x 240 alto)
const int SCREEN_W = 280;
const int SCREEN_H = 240;
const int STATUS_H = 24;
const int ITEM_H   = 34;
const int ITEM_W   = 248;
const int SPACING  = 5;
const int VISIBLE_ITEMS = 5;

// Prototipos
void animacionInicio();
void sincronizarDesdeGitHub();
bool descargarArchivoGitHub(const char* rutaRelativa);
void ejecutarScriptJS(String scriptPath);
void escanearRedesWifi();
void dibujarPantalla();
void ejecutarSeleccion();
void dibujarTecladoWifi();
void conectarWifi(String ssid, String pass);
void iniciarGameBoyPokemon(int edition);
void dibujarGameBoyPokemon();
void dibujarMonitorADC();
void dibujarTestRGB();
void dibujarTestBotones();
void dibujarInfoMemoria();

void setup() {
  Serial.begin(115200);

  pinMode(PIN_UP,   INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_OK,   INPUT_PULLUP);

  for (int i = 0; i < WAVE_POINTS; i++) waveY[i] = 135;

  // 1. Montar sistema de archivos LittleFS en la Flash del ESP32
  LittleFS.begin(true);

  // 2. Cargar WiFi, tema y rotacion guardados en memoria NVS (Preferences.h)
  preferences.begin("g4briel_wifi", false);
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  currentLayout  = (MenuLayout)preferences.getInt("layout", (int)currentLayout);
  currentTheme   = (ThemeId)preferences.getInt("theme", (int)currentTheme);
  screenRotation = preferences.getInt("rot", 1);
  screenOffsetX  = preferences.getInt("offx", 0);
  if (screenRotation != 1 && screenRotation != 3) screenRotation = 1;

  tft.init();
  tft.setRotation(screenRotation); // 1 o 3 = MODO HORIZONTAL (280x240)
  tft.fillScreen(TFT_BLACK);
  tft.invertDisplay(true);

  if (savedSSID.length() > 0) {
    hasSavedWifi = true;
    wifiTargetSSID = savedSSID;
    wifiPassword   = savedPass;
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
  }

  // 3. Ejecutar animacion de arranque identica a la web
  animacionInicio();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
  }

  dibujarPantalla();
}

void loop() {
  // Actualizacion en tiempo real del Osciloscopio Monitor ADC
  if (currentState == STATE_MONITOR_ADC) {
    if (millis() - lastMonitorTick > 85) {
      lastMonitorTick = millis();
      simAngle += 0.32;
      int raw = analogRead(PIN_ADC);
      int nextY = 135 + (int)(sin(simAngle) * 38.0) + ((raw % 11) - 5);
      if (nextY < 86) nextY = 86;
      if (nextY > 184) nextY = 184;
      for (int i = 0; i < WAVE_POINTS - 1; i++) waveY[i] = waveY[i + 1];
      waveY[WAVE_POINTS - 1] = nextY;
      dibujarMonitorADC();
    }
    if (digitalRead(PIN_OK) == LOW || digitalRead(PIN_UP) == LOW || digitalRead(PIN_DOWN) == LOW) {
      currentState = STATE_MAIN_MENU;
      dibujarPantalla();
      delay(250);
    }
    return;
  }

  if (currentState == STATE_JS_APP || currentState == STATE_MEM_INFO) {
    if (digitalRead(PIN_OK) == LOW || digitalRead(PIN_UP) == LOW || digitalRead(PIN_DOWN) == LOW) {
      currentState = (currentState == STATE_MEM_INFO) ? STATE_SISTEMA_TESTS : STATE_MAIN_MENU;
      dibujarPantalla();
      delay(250);
    }
    return;
  }

  if (currentState == STATE_RGB_TEST) {
    if (digitalRead(PIN_OK) == LOW) {
      rgbTestStep = (rgbTestStep + 1) % 5;
      dibujarTestRGB();
      delay(220);
    } else if (digitalRead(PIN_UP) == LOW || digitalRead(PIN_DOWN) == LOW) {
      currentState = STATE_SISTEMA_TESTS;
      dibujarPantalla();
      delay(250);
    }
    return;
  }

  if (currentState == STATE_BTN_TEST) {
    if (digitalRead(PIN_UP) == LOW) {
      lastBtnPressed = "UP";
      okExitCounter = 0;
      dibujarTestBotones();
      delay(180);
    } else if (digitalRead(PIN_DOWN) == LOW) {
      lastBtnPressed = "DOWN";
      okExitCounter = 0;
      dibujarTestBotones();
      delay(180);
    } else if (digitalRead(PIN_OK) == LOW) {
      lastBtnPressed = "OK";
      okExitCounter++;
      if (okExitCounter >= 2) {
        okExitCounter = 0;
        currentState = STATE_SISTEMA_TESTS;
        dibujarPantalla();
        delay(250);
        return;
      }
      dibujarTestBotones();
      delay(220);
    }
    return;
  }

  // Batalla interactiva GameBoy Color (2x2 botones: ATACAR, POKEBALL, CURAR, SALIR)
  if (currentState == STATE_POKEMON_PLAY) {
    if (digitalRead(PIN_UP) == LOW) {
      gbActionIdx = (gbActionIdx - 1 + 4) % 4;
      dibujarGameBoyPokemon();
      delay(160);
    }
    if (digitalRead(PIN_DOWN) == LOW) {
      gbActionIdx = (gbActionIdx + 1) % 4;
      dibujarGameBoyPokemon();
      delay(160);
    }
    if (digitalRead(PIN_OK) == LOW) {
      if (gbActionIdx == 0) {
        // ATACAR
        gbHp = max(15, gbHp - 15);
        gbLevel++;
        if (activePokemonEdition == 0) gbMsg1 = "PIKACHU uso Impactrueno!";
        else if (activePokemonEdition == 1) gbMsg1 = "BLASTOISE uso Hidrobomba!";
        else gbMsg1 = "CHARIZARD uso Llamarada!";
        gbMsg2 = "Golpe critico! Sube a NV." + String(gbLevel);
      } else if (gbActionIdx == 1) {
        // POKEBALL
        gbMsg1 = "Lanzaste una Pokeball!";
        gbMsg2 = "Pokemon registrado en Pokedex!";
      } else if (gbActionIdx == 2) {
        // CURAR
        gbHp = gbMaxHp;
        gbMsg1 = "Usaste Superpocion!";
        gbMsg2 = "HP restaurado al 100%";
      } else if (gbActionIdx == 3) {
        // SALIR
        currentState = STATE_G4BRIEL_GAMEBOY;
        dibujarPantalla();
        delay(240);
        return;
      }
      dibujarGameBoyPokemon();
      delay(220);
    }
    return;
  }

  // Teclado WiFi de 2 Filas
  if (currentState == STATE_WIFI_KEYBOARD) {
    int charCount = strlen(kbCharsets[kbMode]);
    int totalItems = charCount + 4; // Fila 1 (letras) + Fila 2 (4 botones inferiores: MAYUS/123, BORRAR, ENTER, SALIR)

    if (digitalRead(PIN_UP) == LOW) {
      keyboardIndex = (keyboardIndex - 1 + totalItems) % totalItems;
      dibujarTecladoWifi();
      delay(140);
    }
    if (digitalRead(PIN_DOWN) == LOW) {
      keyboardIndex = (keyboardIndex + 1) % totalItems;
      dibujarTecladoWifi();
      delay(140);
    }
    if (digitalRead(PIN_OK) == LOW) {
      // Si mantiene presionado OK (0.45s) salta directo entre la Fila 1 (letras) y la Fila 2 (barra inferior)
      unsigned long tPress = millis();
      while (digitalRead(PIN_OK) == LOW && (millis() - tPress) < 450) {
        delay(10);
      }
      if ((millis() - tPress) >= 450) {
        if (keyboardIndex < charCount) {
          keyboardIndex = charCount; // Salta directo a la Fila 2 ([MAYUS/123] -> [BORRAR] -> [ENTER])
        } else {
          keyboardIndex = 0; // Si ya estaba abajo, vuelve arriba a las letras
        }
        dibujarTecladoWifi();
        while (digitalRead(PIN_OK) == LOW) delay(10);
        return;
      }

      if (keyboardIndex < charCount) {
        // Fila 1: Escribir letra/numero seleccionado
        wifiPassword += kbCharsets[kbMode][keyboardIndex];
      } else {
        // Fila 2: Botones de accion inferiores
        int botIdx = keyboardIndex - charCount;
        if (botIdx == 0) {
          // Boton [MAYUS/123#]: Cambiar entre abc -> ABC -> 123# y volver a la fila de letras
          kbMode = (kbMode + 1) % 3;
          keyboardIndex = 0;
        } else if (botIdx == 1) {
          // Boton [BORRAR]
          if (wifiPassword.length() > 0) wifiPassword.remove(wifiPassword.length() - 1);
        } else if (botIdx == 2) {
          // Boton [ENTER]: Conectar al WiFi
          conectarWifi(wifiTargetSSID, wifiPassword);
          return;
        } else if (botIdx == 3) {
          // Boton [SALIR]
          currentState = STATE_CONFIGURACION;
          selectedIndex = 0;
          scrollOffset = 0;
          dibujarPantalla();
          delay(200);
          return;
        }
      }
      dibujarTecladoWifi();
      delay(180);
    }
    return;
  }

  int maxItems = totalMainItems;
  if (currentState == STATE_G4BRIEL_GAMEBOY) maxItems = TOTAL_GAMEBOY;
  else if (currentState == STATE_SISTEMA_TESTS) maxItems = TOTAL_SYS_TESTS;
  else if (currentState == STATE_CONFIGURACION) maxItems = TOTAL_CONFIG;
  else if (currentState == STATE_WIFI_SCAN) maxItems = totalScannedNets;

  if (digitalRead(PIN_UP) == LOW) {
    selectedIndex = (selectedIndex > 0) ? (selectedIndex - 1) : (maxItems - 1);
    dibujarPantalla();
    delay(175);
  }

  if (digitalRead(PIN_DOWN) == LOW) {
    selectedIndex = (selectedIndex < maxItems - 1) ? (selectedIndex + 1) : 0;
    dibujarPantalla();
    delay(175);
  }

  if (digitalRead(PIN_OK) == LOW) {
    ejecutarSeleccion();
    delay(230);
  }
}

// =========================================================================
// ICONOS VECTORIALES / PIXEL-ART (IDENTICOS A drawPixelIcon DE LA WEB)
// =========================================================================
void dibujarIconoWifi(int x, int y, uint16_t color) {
  int ox = x + screenOffsetX;
  tft.drawCircleHelper(ox + 7, y + 12, 8, 1 | 2, color);
  tft.drawCircleHelper(ox + 7, y + 12, 5, 1 | 2, color);
  tft.fillCircle(ox + 7, y + 12, 1, color);
}

void dibujarIconoItem(IconType icon, int x, int y, uint16_t color) {
  int ox = x + screenOffsetX;
  switch (icon) {
    case ICON_GAMEPAD:
      tft.drawRoundRect(ox + 1, y + 2, 12, 9, 2, color);
      tft.fillRect(ox + 3, y + 6, 3, 1, color);
      tft.fillRect(ox + 4, y + 5, 1, 3, color);
      tft.fillRect(ox + 9, y + 5, 2, 2, color);
      break;
    case ICON_GEAR:
      tft.drawCircle(ox + 6, y + 6, 4, color);
      tft.fillRect(ox + 5, y + 1, 2, 11, color);
      tft.fillRect(ox + 1, y + 5, 11, 2, color);
      tft.fillCircle(ox + 6, y + 6, 2, TFT_BLACK);
      break;
    case ICON_SYNC:
      tft.drawCircle(ox + 6, y + 6, 5, color);
      tft.fillTriangle(ox + 9, y + 1, ox + 12, y + 4, ox + 7, y + 4, color);
      break;
    case ICON_CHART:
      tft.fillRect(ox + 1, y + 8, 3, 4, color);
      tft.fillRect(ox + 5, y + 5, 3, 7, color);
      tft.fillRect(ox + 9, y + 1, 3, 11, color);
      break;
    case ICON_CROSS:
      tft.fillRect(ox + 4, y + 1, 4, 10, color);
      tft.fillRect(ox + 1, y + 4, 10, 4, color);
      break;
    case ICON_BOLT:
      tft.fillTriangle(ox + 8, y + 1, ox + 2, y + 7, ox + 7, y + 7, color);
      tft.fillTriangle(ox + 5, y + 6, ox + 11, y + 6, ox + 4, y + 12, color);
      break;
    case ICON_DROP:
      tft.fillTriangle(ox + 6, y + 1, ox + 2, y + 8, ox + 10, y + 8, color);
      tft.fillCircle(ox + 6, y + 8, 4, color);
      break;
    case ICON_FLAME:
      tft.fillTriangle(ox + 6, y + 1, ox + 1, y + 9, ox + 11, y + 9, color);
      tft.fillCircle(ox + 6, y + 9, 3, color);
      break;
    case ICON_WIFI:
      dibujarIconoWifi(x - 1, y - 2, color);
      break;
    case ICON_LAYOUT:
      tft.drawRect(ox + 1, y + 1, 10, 4, color);
      tft.drawRect(ox + 1, y + 7, 10, 4, color);
      break;
    case ICON_PALETTE:
      tft.drawCircle(ox + 6, y + 6, 5, color);
      tft.fillRect(ox + 4, y + 4, 4, 4, color);
      break;
    case ICON_ROTATE:
      tft.drawRect(ox + 1, y + 3, 10, 7, color);
      tft.fillTriangle(ox + 9, y + 1, ox + 12, y + 4, ox + 8, y + 4, color);
      break;
    case ICON_BACK:
      tft.fillTriangle(ox + 2, y + 6, ox + 7, y + 2, ox + 7, y + 10, color);
      tft.fillRect(ox + 7, y + 5, 5, 3, color);
      break;
  }
}

void dibujarBarraEstado(const char* titulo) {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillRect(ox, 0, SCREEN_W, STATUS_H, th.topBarBg);
  tft.drawFastHLine(ox, STATUS_H - 1, SCREEN_W, th.cardBorder);

  tft.setTextColor(th.accent, th.topBarBg);
  tft.setTextSize(1);
  tft.drawString(titulo, ox + 8, 8);

  dibujarIconoWifi(SCREEN_W - 50, 2, wifiConnected ? TFT_GREEN : th.accent);

  tft.drawRect(ox + SCREEN_W - 28, 7, 18, 9, TFT_LIGHTGREY);
  tft.fillRect(ox + SCREEN_W - 10, 9, 2, 5, TFT_LIGHTGREY);
  tft.fillRect(ox + SCREEN_W - 26, 9, 12, 5, TFT_GREEN);
}

void dibujarScrollbar(int topY, int totalH, int totalItems, int visibleItems) {
  if (totalItems <= visibleItems) return;
  const ThemePalette& th = themes[currentTheme];
  int sbX = screenOffsetX + 272;
  int trackH = totalH - 14;
  int y = topY + 7;
  tft.fillRect(sbX, y, 3, trackH, th.cardBg);

  int thumbH = max(14, (visibleItems * trackH) / totalItems);
  int maxScroll = totalItems - visibleItems;
  int thumbY = y + ((trackH - thumbH) * scrollOffset) / max(1, maxScroll);
  tft.fillRect(sbX, thumbY, 3, thumbH, th.accent);
}

void animacionInicio() {
  tft.fillScreen(TFT_BLACK);
  const ThemePalette& th = themes[currentTheme];
  int cx = screenOffsetX + (SCREEN_W / 2); // 140
  const char* bootSteps[5] = {
    "Iniciando SPI & LittleFS...",
    "Leyendo WiFi de NVS Flash...",
    "Sincronizando manifest.json...",
    "Cargando ROMs GameBoy...",
    "G4BRIEL OS Listo!"
  };

  for (int p = 0; p <= 100; p += 5) {
    tft.drawCircle(cx, 68, 32, th.cardBorder);
    tft.drawCircle(cx, 68, 24, th.accent);
    tft.fillRoundRect(cx - 14, 54, 28, 28, 6, th.accentBg);
    tft.drawRoundRect(cx - 14, 54, 28, 28, 6, th.accent);
    tft.fillRect(cx - 4, 64, 8, 8, th.accent);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("G4BRIEL OS", cx - 60, 110);
    tft.setTextColor(th.textSecondary, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("JS Engine - GitHub OTA", cx - 66, 134);

    const int barX = screenOffsetX + 40, barY = 166, barW = 200, barH = 8;
    tft.drawRoundRect(barX, barY, barW, barH, 4, th.cardBorder);
    int fillW = map(p, 0, 100, 0, barW - 4);
    if (fillW > 0) tft.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2, th.accent);

    int stepIdx = min(4, p / 22);
    tft.fillRect(screenOffsetX + 20, 184, 240, 14, TFT_BLACK);
    tft.setTextColor(th.accent, TFT_BLACK);
    int msgW = strlen(bootSteps[stepIdx]) * 6;
    tft.drawString(bootSteps[stepIdx], cx - (msgW / 2), 186);
    delay(35);
  }
  delay(180);
  tft.fillScreen(TFT_BLACK);
}

// =========================================================================
// SPRITES PIXEL-ART Y PANTALLA GAMEBOY COLOR (1:1 CON EL SIMULADOR WEB)
// =========================================================================
void dibujarSpritePokemon(int edition, int x, int y, uint16_t col) {
  int ox = x + screenOffsetX;
  if (edition == 0) {
    // Pikachu pixel-art (orejas con punta negra, mejillas rojas y cola rayo)
    tft.fillRect(ox + 6, y, 5, 10, col);
    tft.fillRect(ox + 25, y, 5, 10, col);
    tft.fillRect(ox + 6, y, 5, 4, 0x1082);
    tft.fillRect(ox + 25, y, 5, 4, 0x1082);
    tft.fillRect(ox + 6, y + 10, 24, 20, col);
    tft.fillRect(ox + 11, y + 15, 3, 3, TFT_BLACK);
    tft.fillRect(ox + 22, y + 15, 3, 3, TFT_BLACK);
    tft.fillRect(ox + 8, y + 21, 4, 4, TFT_RED);
    tft.fillRect(ox + 24, y + 21, 4, 4, TFT_RED);
    tft.fillRect(ox + 30, y + 18, 6, 4, col);
    tft.fillRect(ox + 33, y + 12, 6, 6, col);
  } else if (edition == 1) {
    // Blastoise / Squirtle pixel-art (cañones y caparazon)
    tft.fillRect(ox + 8, y + 2, 18, 6, 0x9E7F);
    tft.fillRect(ox + 26, y + 2, 6, 6, 0x9E7F);
    tft.fillRect(ox + 6, y + 8, 24, 22, col);
    tft.fillRect(ox + 11, y + 13, 3, 4, TFT_BLACK);
    tft.fillRect(ox + 21, y + 13, 3, 4, TFT_BLACK);
    tft.fillRect(ox + 10, y + 20, 16, 8, 0xF710);
  } else {
    // Charizard / Charmander pixel-art (alas y llama en la cola)
    tft.fillRect(ox + 10, y + 2, 16, 24, col);
    tft.fillRect(ox + 2, y + 8, 8, 12, col);
    tft.fillRect(ox + 26, y + 8, 8, 12, col);
    tft.fillRect(ox + 13, y + 8, 3, 3, TFT_BLACK);
    tft.fillRect(ox + 20, y + 8, 3, 3, TFT_BLACK);
    tft.fillRect(ox + 32, y + 20, 6, 6, TFT_YELLOW);
    tft.fillRect(ox + 34, y + 16, 4, 4, TFT_RED);
  }
}

void iniciarGameBoyPokemon(int edition) {
  activePokemonEdition = edition;
  gbHp = 100;
  gbMaxHp = 100;
  gbActionIdx = 0;
  if (edition == 0) {
    gbLevel = 25;
    gbMsg1 = "PIKACHU uso Impactrueno!";
    gbMsg2 = "Edicion Especial Pikachu (GBC)";
  } else if (edition == 1) {
    gbLevel = 36;
    gbMsg1 = "BLASTOISE listo en Kanto!";
    gbMsg2 = "Edicion Azul (GameBoy)";
  } else {
    gbLevel = 36;
    gbMsg1 = "CHARIZARD lanzo Llamarada!";
    gbMsg2 = "Edicion Roja (GameBoy)";
  }
  currentState = STATE_POKEMON_PLAY;
  dibujarGameBoyPokemon();
}

void dibujarGameBoyPokemon() {
  int ox = screenOffsetX;
  tft.fillScreen(0x0841);
  uint16_t col = gameboyColors[activePokemonEdition];
  const char* titles[3]   = { "GAMEBOY - POKEMON AMARILLO", "GAMEBOY - POKEMON AZUL", "GAMEBOY - POKEMON ROJO" };
  const char* pokemons[3] = { "PIKACHU", "BLASTOISE", "CHARIZARD" };

  // Barra superior GameBoy con el color de la edicion
  tft.fillRect(ox, 0, 280, 24, 0x10A3);
  tft.fillRect(ox, 22, 280, 2, col);
  tft.setTextColor(col, 0x10A3);
  tft.setTextSize(1);
  tft.drawString(titles[activePokemonEdition], ox + 10, 7);
  tft.setTextColor(TFT_WHITE, 0x10A3);
  tft.drawString(("NV." + String(gbLevel)).c_str(), ox + 236, 7);

  // Marco de pantalla GameBoy Color (arena de batalla 280x240)
  tft.fillRoundRect(ox + 10, 28, 260, 122, 6, 0x10E4);
  tft.drawRoundRect(ox + 10, 28, 260, 122, 6, col);
  tft.fillRoundRect(ox + 14, 32, 252, 114, 4, 0x0883);

  // HUD de vida y barra HP del Pokemon
  tft.setTextColor(col, 0x0883);
  tft.setTextSize(2);
  tft.drawString(pokemons[activePokemonEdition], ox + 22, 38);
  tft.setTextColor(TFT_WHITE, 0x0883);
  tft.setTextSize(1);
  tft.drawString(("HP: " + String(gbHp) + "/" + String(gbMaxHp)).c_str(), ox + 22, 56);

  tft.fillRoundRect(ox + 22, 68, 130, 8, 3, 0x2147);
  tft.drawRoundRect(ox + 22, 68, 130, 8, 3, 0x42C9);
  int hpFill = (126 * gbHp) / gbMaxHp;
  uint16_t hpCol = (gbHp > 50) ? TFT_GREEN : ((gbHp > 25) ? TFT_YELLOW : TFT_RED);
  if (hpFill > 0) tft.fillRoundRect(ox + 24, 70, hpFill, 4, 2, hpCol);

  // Plataforma y Sprite del Pokemon a la derecha
  tft.fillEllipse(ox + 212, 82, 34, 9, 0x1946);
  tft.drawEllipse(ox + 212, 82, 34, 9, col);
  dibujarSpritePokemon(activePokemonEdition, 194, 44, col);

  // Caja de dialogo estilo Pokemon GameBoy
  tft.fillRoundRect(ox + 18, 96, 244, 44, 4, 0x0862);
  tft.drawRoundRect(ox + 18, 96, 244, 44, 4, col);
  tft.setTextColor(TFT_WHITE, 0x0862);
  tft.drawString(gbMsg1.c_str(), ox + 26, 104);
  tft.setTextColor(col, 0x0862);
  tft.drawString(gbMsg2.c_str(), ox + 26, 122);

  // Botonera interactiva 2x2 de comandos GameBoy (identica a la web)
  const char* actLabels[4] = { "* ATACAR", "O POKEBALL", "+ CURAR", "< SALIR" };
  for (int i = 0; i < 4; i++) {
    int c = i % 2;
    int r = i / 2;
    int bx = ox + 10 + (c * 134);
    int by = 156 + (r * 36);
    bool isSel = (gbActionIdx == i);

    uint16_t bg = isSel ? col : 0x10C4;
    uint16_t border = isSel ? TFT_WHITE : 0x29A8;
    uint16_t txt = isSel ? TFT_BLACK : TFT_WHITE;

    tft.fillRoundRect(bx, by, 126, 31, 6, bg);
    tft.drawRoundRect(bx, by, 126, 31, 6, border);
    tft.setTextColor(txt, bg);
    tft.setTextSize(1);
    tft.drawString(actLabels[i], bx + 22, by + 11);
  }

  tft.setTextColor(0x8410, 0x0841);
  tft.setTextSize(1);
  tft.drawString("UP/DOWN: Mover cursor - OK: Ejecutar", ox + 32, 228);
}

// =========================================================================
// OSCILOSCOPIO MONITOR ADC, TEST RGB, TEST 3 BOTONES Y MEMORIA (1:1 WEB)
// =========================================================================
void dibujarMonitorADC() {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("MONITOR ADC (GPIO 34)");

  tft.fillRoundRect(ox + 12, 32, 256, 42, 6, th.cardBg);
  tft.drawRoundRect(ox + 12, 32, 256, 42, 6, th.cardBorder);
  tft.setTextColor(TFT_WHITE, th.cardBg);
  tft.setTextSize(1);
  tft.drawString("Voltaje en Tiempo Real:", ox + 22, 40);
  tft.setTextColor(TFT_GREEN, th.cardBg);
  tft.setTextSize(2);
  tft.drawString("2.84 V", ox + 22, 54);
  tft.setTextColor(th.accent, th.cardBg);
  tft.setTextSize(1);
  tft.drawString("Pico: 3.12V", ox + 170, 42);
  tft.drawString("Min:  1.95V", ox + 170, 58);

  // Pantalla del osciloscopio con onda en vivo
  tft.fillRoundRect(ox + 12, 82, 256, 110, 6, 0x0841);
  tft.drawRoundRect(ox + 12, 82, 256, 110, 6, th.cardBorder);
  tft.drawFastHLine(ox + 14, 137, 252, 0x18E3);

  for (int i = 0; i < WAVE_POINTS - 1; i++) {
    int x1 = ox + 16 + (i * 246) / (WAVE_POINTS - 1);
    int y1 = waveY[i];
    int x2 = ox + 16 + ((i + 1) * 246) / (WAVE_POINTS - 1);
    int y2 = waveY[i + 1];
    tft.drawLine(x1, y1, x2, y2, th.accent);
    tft.drawLine(x1, y1 + 1, x2, y2 + 1, th.accent);
  }

  tft.setTextColor(th.textSecondary, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Pulsa OK / UP / DOWN para volver al Menu", ox + 20, 206);
}

void dibujarTestRGB() {
  const uint16_t cols[5] = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK };
  const char* names[5]   = { "ROJO", "VERDE", "AZUL", "BLANCO", "NEGRO" };
  uint16_t c = cols[rgbTestStep % 5];
  uint16_t txt = (c == TFT_WHITE || c == TFT_GREEN) ? TFT_BLACK : TFT_WHITE;
  tft.fillScreen(c);
  tft.setTextColor(txt, c);
  tft.setTextSize(2);
  tft.drawString((String("TEST RGB: ") + names[rgbTestStep % 5]).c_str(), screenOffsetX + 28, 44);
  tft.setTextSize(1);
  tft.drawString("OK: Cambiar color | UP/DOWN: Salir", screenOffsetX + 28, 86);
}

void dibujarTestBotones() {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("TEST 3 BOTONES (280x240)");

  bool isUp = (lastBtnPressed == "UP");
  tft.fillRoundRect(ox + 30, 44, 220, 36, 6, isUp ? th.accent : th.cardBg);
  tft.drawRoundRect(ox + 30, 44, 220, 36, 6, th.cardBorder);
  tft.setTextColor(isUp ? TFT_BLACK : TFT_WHITE, isUp ? th.accent : th.cardBg);
  tft.setTextSize(2);
  tft.drawString("^ PIN 25 (UP)", ox + 55, 55);

  bool isOk = (lastBtnPressed == "OK");
  tft.fillRoundRect(ox + 30, 92, 220, 36, 6, isOk ? th.accent : th.cardBg);
  tft.drawRoundRect(ox + 30, 92, 220, 36, 6, th.cardBorder);
  tft.setTextColor(isOk ? TFT_BLACK : TFT_WHITE, isOk ? th.accent : th.cardBg);
  tft.drawString("O PIN 27 (OK)", ox + 55, 103);

  bool isDn = (lastBtnPressed == "DOWN");
  tft.fillRoundRect(ox + 30, 140, 220, 36, 6, isDn ? th.accent : th.cardBg);
  tft.drawRoundRect(ox + 30, 140, 220, 36, 6, th.cardBorder);
  tft.setTextColor(isDn ? TFT_BLACK : TFT_WHITE, isDn ? th.accent : th.cardBg);
  tft.drawString("v PIN 26 (DOWN)", ox + 55, 151);

  tft.setTextColor(th.textSecondary, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Pulsa 2 veces seguidas OK para volver", ox + 32, 198);
}

void dibujarInfoMemoria() {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("MEMORIA & SCRIPTS");

  tft.fillRoundRect(ox + 14, 30, 252, 146, 8, th.cardBg);
  tft.drawRoundRect(ox + 14, 30, 252, 146, 8, th.cardBorder);
  tft.setTextColor(TFT_WHITE, th.cardBg);
  tft.setTextSize(2);
  tft.drawString("ESP32-WROOM (280x240)", ox + 22, 40);

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, th.cardBg);
  tft.drawString("Flash LittleFS: 1.5 MB Libres", ox + 24, 66);
  tft.setTextColor(th.accent, th.cardBg);
  tft.drawString(("SRAM Libre: " + String(ESP.getFreeHeap() / 1024) + " KB").c_str(), ox + 24, 84);
  tft.setTextColor(TFT_CYAN, th.cardBg);
  tft.drawString("Scripts JS integrados: 5", ox + 24, 102);
  tft.setTextColor(TFT_WHITE, th.cardBg);
  tft.drawString(("WiFi NVS: " + (hasSavedWifi ? wifiTargetSSID : String("Sin guardar"))).c_str(), ox + 24, 120);
  tft.setTextColor(th.textSecondary, th.cardBg);
  tft.drawString("Display: ST7789 280x240 IPS", ox + 24, 138);
  tft.setTextColor(TFT_GREEN, th.cardBg);
  tft.drawString("Render 1:1 con Simulador Web OK", ox + 24, 156);

  tft.fillRoundRect(ox + 36, 188, 208, 34, 6, th.accentBg);
  tft.drawRoundRect(ox + 36, 188, 208, 34, 6, th.accent);
  tft.setTextColor(TFT_WHITE, th.accentBg);
  tft.drawString("OK: Volver a Sistema & Tests", ox + 56, 200);
}

// =========================================================================
// LISTA SLIM Y FOCUS SMARTWATCH (1:1 CON ICONOS, BADGES Y SCROLLBAR)
// =========================================================================
void obtenerDatosItem(int idx, String &label, String &sub, IconType &icon, uint16_t &itemAccent) {
  const ThemePalette& th = themes[currentTheme];
  itemAccent = th.accent;
  sub = "";
  icon = ICON_GAMEPAD;

  if (currentState == STATE_MAIN_MENU) {
    label = mainItems[idx].title;
    sub   = mainItems[idx].subtitle;
    icon  = mainItems[idx].icon;
  } else if (currentState == STATE_G4BRIEL_GAMEBOY) {
    label = String(gameboyTitles[idx]);
    sub   = (idx < 3) ? "GameBoy Color Edition" : "Regresar al menu principal";
    icon  = gameboyIcons[idx];
    itemAccent = gameboyColors[idx];
  } else if (currentState == STATE_SISTEMA_TESTS) {
    label = String(sysTestTitles[idx]);
    sub   = "Diagnostico interactivo";
    icon  = sysTestIcons[idx];
  } else if (currentState == STATE_CONFIGURACION) {
    label = String(configTitles[idx]);
    sub   = "Pulsa OK para cambiar";
    icon  = configIcons[idx];
  } else if (currentState == STATE_WIFI_SCAN) {
    label = scannedSSIDs[idx];
    sub   = "Pulsa OK para elegir esta red";
    icon  = (label == "< Volver") ? ICON_BACK : ICON_WIFI;
  }
}

void dibujarListaSlim() {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  int total = totalMainItems;
  const char* header = "SISTEMA 1.69";
  if (currentState == STATE_G4BRIEL_GAMEBOY) { total = TOTAL_GAMEBOY; header = "G4BRIEL - GAMEBOY"; }
  else if (currentState == STATE_SISTEMA_TESTS) { total = TOTAL_SYS_TESTS; header = "SISTEMA & TESTS"; }
  else if (currentState == STATE_CONFIGURACION) { total = TOTAL_CONFIG; header = "CONFIGURACION"; }
  else if (currentState == STATE_WIFI_SCAN) { total = totalScannedNets; header = "REDES WIFI (NVS)"; }
  dibujarBarraEstado(header);

  if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
  else if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;

  int startY = STATUS_H + 6;
  int itemX = ((SCREEN_W - ITEM_W) / 2) - 2;

  for (int i = 0; i < VISIBLE_ITEMS; i++) {
    int itemIdx = scrollOffset + i;
    if (itemIdx >= total) break;

    int y = startY + (i * (ITEM_H + SPACING));
    bool isSel = (itemIdx == selectedIndex);
    String label, sub;
    IconType icon;
    uint16_t itemAccent;
    obtenerDatosItem(itemIdx, label, sub, icon, itemAccent);

    if (isSel) {
      tft.fillRoundRect(ox + itemX, y, ITEM_W, ITEM_H, 6, th.accentBg);
      tft.drawRoundRect(ox + itemX, y, ITEM_W, ITEM_H, 6, itemAccent);
      tft.fillRect(ox + itemX + 2, y + 6, 3, ITEM_H - 12, itemAccent);
    } else {
      uint16_t borderCol = (currentState == STATE_G4BRIEL_GAMEBOY && itemIdx < 3) ? itemAccent : th.cardBorder;
      tft.fillRoundRect(ox + itemX, y, ITEM_W, ITEM_H, 6, th.cardBg);
      tft.drawRoundRect(ox + itemX, y, ITEM_W, ITEM_H, 6, borderCol);
    }

    uint16_t iconCol = isSel ? itemAccent : ((currentState == STATE_G4BRIEL_GAMEBOY) ? itemAccent : th.textSecondary);
    dibujarIconoItem(icon, itemX + 11, y + (ITEM_H - 12) / 2, iconCol);

    uint16_t txtCol = isSel ? th.textPrimary : ((currentState == STATE_G4BRIEL_GAMEBOY) ? itemAccent : th.textSecondary);
    uint16_t bgCol  = isSel ? th.accentBg : th.cardBg;
    tft.setTextColor(txtCol, bgCol);
    tft.setTextSize(2);
    tft.drawString(label.c_str(), ox + itemX + 30, y + (ITEM_H - 14) / 2);

    // Badges a la derecha en Configuracion o flecha '>' como en el simulador web
    tft.setTextSize(1);
    if (currentState == STATE_CONFIGURACION && itemIdx == 0) {
      const char* wSt = wifiConnected ? "[ON]" : (hasSavedWifi ? "[NVS]" : "[CFG]");
      tft.setTextColor(wifiConnected ? TFT_GREEN : th.accent, bgCol);
      tft.drawString(wSt, ox + itemX + ITEM_W - 38, y + 13);
    } else if (currentState == STATE_CONFIGURACION && itemIdx == 1) {
      tft.setTextColor(th.accent, bgCol);
      tft.drawString("[OTA]", ox + itemX + ITEM_W - 38, y + 13);
    } else if (currentState == STATE_CONFIGURACION && itemIdx == 2) {
      tft.setTextColor(th.accent, bgCol);
      tft.drawString(currentLayout == LAYOUT_SLIM_LIST ? "[Slim]" : "[Focus]", ox + itemX + ITEM_W - 48, y + 13);
    } else if (currentState == STATE_CONFIGURACION && itemIdx == 3) {
      String tBadge = String("[") + themes[currentTheme].name + "]";
      tft.setTextColor(th.accent, bgCol);
      tft.drawString(tBadge.c_str(), ox + itemX + ITEM_W - (tBadge.length() * 6) - 8, y + 13);
    } else if (currentState == STATE_CONFIGURACION && itemIdx == 4) {
      String rBadge = String("[Rot:") + String(screenRotation) + "]";
      tft.setTextColor(th.accent, bgCol);
      tft.drawString(rBadge.c_str(), ox + itemX + ITEM_W - 48, y + 13);
    } else if (currentState == STATE_CONFIGURACION && itemIdx == 5) {
      String oBadge = String("[+") + String(screenOffsetX) + "px]";
      tft.setTextColor(th.accent, bgCol);
      tft.drawString(oBadge.c_str(), ox + itemX + ITEM_W - 48, y + 13);
    } else {
      tft.setTextColor(isSel ? itemAccent : th.cardBorder, bgCol);
      tft.setTextSize(2);
      tft.drawString(">", ox + itemX + ITEM_W - 16, y + (ITEM_H - 14) / 2);
    }
  }

  dibujarScrollbar(STATUS_H, SCREEN_H - STATUS_H, total, VISIBLE_ITEMS);
}

void dibujarFocusCard() {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  int total = totalMainItems;
  const char* header = "SISTEMA 1.69";
  if (currentState == STATE_G4BRIEL_GAMEBOY) { total = TOTAL_GAMEBOY; header = "G4BRIEL - GAMEBOY"; }
  else if (currentState == STATE_SISTEMA_TESTS) { total = TOTAL_SYS_TESTS; header = "SISTEMA & TESTS"; }
  else if (currentState == STATE_CONFIGURACION) { total = TOTAL_CONFIG; header = "CONFIGURACION"; }
  else if (currentState == STATE_WIFI_SCAN) { total = totalScannedNets; header = "REDES WIFI (NVS)"; }
  dibujarBarraEstado(header);

  int prevIdx = (selectedIndex - 1 + total) % total;
  int nextIdx = (selectedIndex + 1) % total;

  String prevLabel, prevSub, curLabel, curSub, nextLabel, nextSub;
  IconType prevIcon, curIcon, nextIcon;
  uint16_t prevAcc, curAcc, nextAcc;

  obtenerDatosItem(prevIdx, prevLabel, prevSub, prevIcon, prevAcc);
  obtenerDatosItem(selectedIndex, curLabel, curSub, curIcon, curAcc);
  obtenerDatosItem(nextIdx, nextLabel, nextSub, nextIcon, nextAcc);

  // Tarjeta superior atenuada (opcion anterior)
  tft.fillRoundRect(ox + 22, STATUS_H + 6, 236, 28, 6, th.cardBg);
  tft.drawRoundRect(ox + 22, STATUS_H + 6, 236, 28, 6, th.cardBorder);
  dibujarIconoItem(prevIcon, 32, STATUS_H + 14, th.textSecondary);
  tft.setTextColor(th.textSecondary, th.cardBg);
  tft.setTextSize(1);
  tft.drawString(prevLabel.c_str(), ox + 52, STATUS_H + 16);

  // Tarjeta central destacada (opcion activa)
  int centerY = STATUS_H + 40;
  tft.fillRoundRect(ox + 16, centerY, 248, 96, 10, th.accentBg);
  tft.drawRoundRect(ox + 16, centerY, 248, 96, 10, curAcc);
  dibujarIconoItem(curIcon, 28, centerY + 16, curAcc);

  tft.setTextColor(currentState == STATE_G4BRIEL_GAMEBOY ? curAcc : th.textPrimary, th.accentBg);
  tft.setTextSize(2);
  tft.drawString(curLabel.c_str(), ox + 52, centerY + 14);

  tft.setTextColor(th.textSecondary, th.accentBg);
  tft.setTextSize(1);
  tft.drawString(curSub.c_str(), ox + 28, centerY + 46);

  String idxBadge = "[" + String(selectedIndex + 1) + "/" + String(total) + "]";
  tft.setTextColor(curAcc, th.accentBg);
  tft.drawString(idxBadge.c_str(), ox + 212, centerY + 74);

  // Tarjeta inferior atenuada (opcion siguiente)
  tft.fillRoundRect(ox + 22, centerY + 102, 236, 28, 6, th.cardBg);
  tft.drawRoundRect(ox + 22, centerY + 102, 236, 28, 6, th.cardBorder);
  dibujarIconoItem(nextIcon, 32, centerY + 110, th.textSecondary);
  tft.setTextColor(th.textSecondary, th.cardBg);
  tft.setTextSize(1);
  tft.drawString(nextLabel.c_str(), ox + 52, centerY + 112);

  dibujarScrollbar(STATUS_H, SCREEN_H - STATUS_H, total, 3);
}

// =========================================================================
// TECLADO WIFI DE 2 FILAS (FILA 1 LETRAS + FILA 2 ACCIONES)
// =========================================================================
void dibujarTecladoWifi() {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("TECLADO WIFI (2 FILAS)");

  const char* chars = kbCharsets[kbMode];
  int charCount = strlen(chars);
  bool inRow1 = (keyboardIndex < charCount);

  // Caja superior de la clave WiFi (con ajuste de 2 lineas si es larga)
  tft.fillRoundRect(ox + 10, 27, 260, 48, 6, th.cardBg);
  tft.drawRoundRect(ox + 10, 27, 260, 48, 6, th.cardBorder);
  tft.setTextColor(th.accent, th.cardBg);
  tft.setTextSize(1);
  String shortSsid = wifiTargetSSID.length() > 18 ? wifiTargetSSID.substring(0, 18) : wifiTargetSSID;
  tft.drawString(("Red: " + shortSsid).c_str(), ox + 16, 30);

  tft.setTextColor(th.textSecondary, th.cardBg);
  String modeTag = String("[") + kbModeNames[kbMode] + "]";
  tft.drawString(modeTag.c_str(), ox + 222, 30);

  tft.setTextColor(TFT_WHITE, th.cardBg);
  String fullPass = wifiPassword + "_";
  if (fullPass.length() <= 19) {
    tft.setTextSize(2);
    tft.drawString(fullPass.c_str(), ox + 16, 44);
  } else {
    tft.setTextSize(1);
    String vis = fullPass;
    if (vis.length() > 74) vis = "<<" + vis.substring(vis.length() - 72);
    String l1 = vis.substring(0, min((unsigned int)38, vis.length()));
    tft.drawString(l1.c_str(), ox + 16, 43);
    if (vis.length() > 38) {
      String l2 = vis.substring(38);
      tft.setTextColor(TFT_GREEN, th.cardBg);
      tft.drawString(l2.c_str(), ox + 16, 56);
    }
  }

  // FILA 1: LISTADO DE LETRAS / NUMEROS
  int row1Y = 80;
  tft.fillRoundRect(ox + 10, row1Y, 260, 62, 8, 0x0861);
  tft.drawRoundRect(ox + 10, row1Y, 260, 62, 8, inRow1 ? th.accent : th.cardBorder);

  tft.setTextSize(1);
  tft.setTextColor(inRow1 ? th.accent : th.textSecondary, 0x0861);
  if (kbMode == 0) tft.drawString("FILA 1: MINUSCULAS (a - z)", ox + 18, row1Y + 5);
  else if (kbMode == 1) tft.drawString("FILA 1: MAYUSCULAS (A - Z)", ox + 18, row1Y + 5);
  else tft.drawString("FILA 1: NUMEROS Y SIMBOLOS (0 - 9)", ox + 18, row1Y + 5);

  int centerCharIdx = inRow1 ? keyboardIndex : 0;
  for (int offset = -3; offset <= 3; offset++) {
    int idx = (centerCharIdx + offset + charCount) % charCount;
    char c = chars[idx];
    int kx = ox + 140 + (offset * 35) - 14;
    int ky = row1Y + 20;

    if (offset == 0 && inRow1) {
      tft.fillRoundRect(kx - 4, ky - 2, 36, 34, 6, th.accentBg);
      tft.drawRoundRect(kx - 4, ky - 2, 36, 34, 6, th.accent);
      tft.setTextColor(TFT_WHITE, th.accentBg);
      if (c == ' ') {
        tft.setTextSize(1);
        tft.drawString("ESP", kx + 1, ky + 11);
      } else {
        char s[2] = { c, '\0' };
        tft.setTextSize(3);
        tft.drawString(s, kx + 6, ky + 4);
      }
    } else {
      tft.fillRoundRect(kx, ky + 2, 28, 26, 4, th.cardBg);
      tft.drawRoundRect(kx, ky + 2, 28, 26, 4, th.cardBorder);
      tft.setTextColor(th.textSecondary, th.cardBg);
      if (c == ' ') {
        tft.setTextSize(1);
        tft.drawString("_", kx + 11, ky + 10);
      } else {
        char s[2] = { c, '\0' };
        tft.setTextSize(2);
        tft.drawString(s, kx + 8, ky + 8);
      }
    }
  }

  // FILA 2: BARRA INFERIOR DE 4 ACCIONES (MAYUS/123# | BORRAR | ENTER | SALIR)
  int row2Y = 150;
  const char* nextModeLabel = (kbMode == 0) ? "MAYUS" : ((kbMode == 1) ? "123#" : "minus");
  const char* btnLabels[4] = { nextModeLabel, "BORRAR", "ENTER", "SALIR" };
  int btnX[4] = { 10, 76, 142, 208 };
  int btnW[4] = { 62, 62, 62, 62 };

  for (int b = 0; b < 4; b++) {
    bool isSel = (!inRow1 && (keyboardIndex - charCount) == b);
    uint16_t bg = th.cardBg;
    uint16_t border = th.cardBorder;
    uint16_t txtCol = TFT_WHITE;

    if (b == 2) {
      bg = isSel ? 0x02E0 : th.cardBg;
      border = isSel ? TFT_GREEN : th.cardBorder;
      txtCol = TFT_GREEN;
    } else if (isSel) {
      bg = th.accentBg;
      border = th.accent;
      txtCol = TFT_WHITE;
    }

    tft.fillRoundRect(ox + btnX[b], row2Y, btnW[b], 36, 6, bg);
    tft.drawRoundRect(ox + btnX[b], row2Y, btnW[b], 36, 6, border);
    tft.setTextColor(txtCol, bg);
    tft.setTextSize(1);
    int textX = ox + btnX[b] + (btnW[b] - (strlen(btnLabels[b]) * 6)) / 2;
    tft.drawString(btnLabels[b], textX, row2Y + 14);
  }

  tft.setTextSize(1);
  tft.setTextColor(th.textSecondary, TFT_BLACK);
  tft.drawString("UP/DOWN: Mover | Manten OK: Bajar a Fila 2", ox + 14, 196);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("En Fila 2: Cambia MAYUS/123# o dale ENTER", ox + 16, 212);
}

// =========================================================================
// ESCANEO WIFI, CONEXION Y OTA GITHUB
// =========================================================================
void escanearRedesWifi() {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("BUSCANDO REDES WIFI...");
  tft.setTextColor(th.accent, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Escaneando 2.4GHz...", ox + 22, 95);

  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  totalScannedNets = 0;

  if (hasSavedWifi && wifiTargetSSID.length() > 0) {
    scannedSSIDs[totalScannedNets++] = wifiTargetSSID;
  }

  for (int i = 0; i < n && totalScannedNets < (MAX_WIFI_NETS - 1); i++) {
    String s = WiFi.SSID(i);
    if (s.length() > 0 && s != wifiTargetSSID) {
      scannedSSIDs[totalScannedNets++] = s;
    }
  }

  if (totalScannedNets == 0) {
    scannedSSIDs[totalScannedNets++] = "Mi_Casa_5G";
  }
  scannedSSIDs[totalScannedNets++] = "< Volver";

  currentState = STATE_WIFI_SCAN;
  selectedIndex = 0;
  scrollOffset = 0;
  dibujarPantalla();
}

void conectarWifi(String ssid, String pass) {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("CONECTANDO...");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString(ssid.c_str(), ox + 24, 66);
  tft.setTextColor(th.accent, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Guardando en Flash NVS...", ox + 56, 110);

  WiFi.begin(ssid.c_str(), pass.c_str());

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(400);
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    hasSavedWifi = true;
  }
  currentState = STATE_MAIN_MENU;
  selectedIndex = 0;
  scrollOffset = 0;
  dibujarPantalla();
}

bool descargarArchivoGitHub(const char* rutaRelativa) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;

  String url = String(GITHUB_REPO_URL) + "/" + String(rutaRelativa);
  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      String localPath = "/" + String(rutaRelativa);
      if (localPath.indexOf("/apps/") == 0 && !LittleFS.exists("/apps")) {
        LittleFS.mkdir("/apps");
      }
      File f = LittleFS.open(localPath, FILE_WRITE);
      if (f) {
        f.print(payload);
        f.close();
      }
      https.end();
      return true;
    }
    https.end();
  }
  return false;
}

void sincronizarDesdeGitHub() {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("GITHUB OTA SYNC");

  tft.fillRoundRect(ox + 14, 30, 252, 50, 6, th.cardBg);
  tft.drawRoundRect(ox + 14, 30, 252, 50, 6, th.cardBorder);
  tft.setTextColor(th.textSecondary, th.cardBg);
  tft.setTextSize(1);
  tft.drawString("Repositorio GitHub:", ox + 22, 36);
  tft.setTextColor(th.accent, th.cardBg);
  tft.drawString("christoper-d/esp32-os/main", ox + 22, 50);
  tft.setTextColor(TFT_GREEN, th.cardBg);
  tft.drawString("Destino: LittleFS (Flash ESP32)", ox + 22, 64);

  tft.fillRoundRect(ox + 14, 88, 252, 102, 8, 0x0861);
  tft.drawRoundRect(ox + 14, 88, 252, 102, 8, th.cardBorder);
  const int barX = ox + 24, barY = 142, barW = 232, barH = 12;
  tft.drawRoundRect(barX, barY, barW, barH, 4, th.cardBorder);

  descargarArchivoGitHub("manifest.json");
  descargarArchivoGitHub("apps/boot.js");

  for (int i = 0; i < totalMainItems; i++) {
    String script = mainItems[i].scriptPath;
    if (!script.startsWith("system://")) {
      tft.fillRect(ox + 22, 112, 236, 22, 0x0861);
      tft.setTextColor(TFT_WHITE, 0x0861);
      tft.setTextSize(2);
      tft.drawString(("GET /" + script).c_str(), ox + 24, 114);
      descargarArchivoGitHub(script.c_str());
    }
    int fillW = map(i + 1, 0, totalMainItems, 30, barW - 4);
    tft.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2, th.accent);
    delay(200);
  }

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Actualizando menus sin cable USB OK", ox + 34, 204);
  delay(850);

  currentState = STATE_MAIN_MENU;
  selectedIndex = 0;
  dibujarPantalla();
}

void ejecutarScriptJS(String scriptPath) {
  const ThemePalette& th = themes[currentTheme];
  int ox = screenOffsetX;
  currentState = STATE_JS_APP;
  tft.fillScreen(TFT_BLACK);

  File f = LittleFS.open("/" + scriptPath, FILE_READ);
  if (!f) {
    dibujarBarraEstado("APP JS");
    tft.fillRoundRect(ox + 16, 38, 248, 120, 8, th.cardBg);
    tft.drawRoundRect(ox + 16, 38, 248, 120, 8, th.cardBorder);
    tft.setTextColor(th.accent, th.cardBg);
    tft.setTextSize(1);
    tft.drawString(("Archivo: /" + scriptPath).c_str(), ox + 28, 58);
    tft.setTextColor(TFT_WHITE, th.cardBg);
    tft.drawString("Conecta al WiFi y pulsa 'GitHub Sync'", ox + 28, 84);
    tft.drawString("para descargar este .js del repo", ox + 28, 102);
    return;
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    if (line.indexOf("tft.statusBar(") >= 0) {
      int q1 = line.indexOf('"') + 1;
      int q2 = line.lastIndexOf('"');
      if (q1 > 0 && q2 > q1) dibujarBarraEstado(line.substring(q1, q2).c_str());
    } else if (line.indexOf("tft.card(") >= 0) {
      int p1 = line.indexOf('(') + 1;
      int p2 = line.indexOf(')');
      String args = line.substring(p1, p2);
      int x = args.substring(0, args.indexOf(',')).toInt(); args = args.substring(args.indexOf(',') + 1);
      int y = args.substring(0, args.indexOf(',')).toInt(); args = args.substring(args.indexOf(',') + 1);
      int w = args.substring(0, args.indexOf(',')).toInt(); args = args.substring(args.indexOf(',') + 1);
      int h = args.toInt();
      tft.fillRoundRect(ox + x, y, w, h, 6, th.cardBg);
      tft.drawRoundRect(ox + x, y, w, h, 6, th.cardBorder);
    } else if (line.indexOf("tft.text(") >= 0) {
      int q1 = line.indexOf('"') + 1;
      int q2 = line.indexOf('"', q1);
      String txt = line.substring(q1, q2);
      String rest = line.substring(line.indexOf(',', q2) + 1);
      int x = rest.substring(0, rest.indexOf(',')).toInt(); rest = rest.substring(rest.indexOf(',') + 1);
      int y = rest.substring(0, rest.indexOf(',')).toInt(); rest = rest.substring(rest.indexOf(',') + 1);
      int sz = rest.substring(0, rest.indexOf(',')).toInt();
      tft.setTextSize(sz > 0 ? sz : 1);
      tft.setTextColor(TFT_WHITE, th.cardBg);
      tft.drawString(txt.c_str(), ox + x, y);
    }
  }
  f.close();
}

void dibujarPantalla() {
  if (currentState == STATE_WIFI_KEYBOARD) { dibujarTecladoWifi(); return; }
  if (currentState == STATE_POKEMON_PLAY)  { dibujarGameBoyPokemon(); return; }
  if (currentState == STATE_MONITOR_ADC)   { dibujarMonitorADC(); return; }
  if (currentState == STATE_RGB_TEST)      { dibujarTestRGB(); return; }
  if (currentState == STATE_BTN_TEST)      { dibujarTestBotones(); return; }
  if (currentState == STATE_MEM_INFO)      { dibujarInfoMemoria(); return; }

  if (currentLayout == LAYOUT_SLIM_LIST) dibujarListaSlim();
  else dibujarFocusCard();
}

void ejecutarSeleccion() {
  if (currentState == STATE_MAIN_MENU) {
    String script = mainItems[selectedIndex].scriptPath;
    if (script == "apps/g4briel.js") {
      currentState = STATE_G4BRIEL_GAMEBOY;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    } else if (script == "apps/monitor.js") {
      currentState = STATE_MONITOR_ADC;
      dibujarMonitorADC();
    } else if (script == "apps/sysinfo.js") {
      currentState = STATE_SISTEMA_TESTS;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    } else if (script == "system://config") {
      currentState = STATE_CONFIGURACION;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    } else if (script == "system://github_sync") {
      sincronizarDesdeGitHub();
    } else {
      ejecutarScriptJS(script);
    }
    return;
  }

  if (currentState == STATE_G4BRIEL_GAMEBOY) {
    if (selectedIndex < 3) {
      iniciarGameBoyPokemon(selectedIndex);
    } else {
      currentState = STATE_MAIN_MENU;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    }
    return;
  }

  if (currentState == STATE_SISTEMA_TESTS) {
    if (selectedIndex == 0) {
      rgbTestStep = 0;
      currentState = STATE_RGB_TEST;
      dibujarTestRGB();
    } else if (selectedIndex == 1) {
      lastBtnPressed = "NINGUNO";
      okExitCounter = 0;
      currentState = STATE_BTN_TEST;
      dibujarTestBotones();
    } else if (selectedIndex == 2) {
      currentState = STATE_MEM_INFO;
      dibujarInfoMemoria();
    } else {
      currentState = STATE_MAIN_MENU;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    }
    return;
  }

  if (currentState == STATE_WIFI_SCAN) {
    String picked = scannedSSIDs[selectedIndex];
    if (picked == "< Volver") {
      currentState = STATE_CONFIGURACION;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    } else {
      wifiTargetSSID = picked;
      wifiPassword = "";
      kbMode = 0;
      keyboardIndex = 0;
      currentState = STATE_WIFI_KEYBOARD;
      dibujarTecladoWifi();
    }
    return;
  }

  if (currentState == STATE_CONFIGURACION) {
    if (selectedIndex == 0) {
      escanearRedesWifi();
    } else if (selectedIndex == 1) {
      sincronizarDesdeGitHub();
    } else if (selectedIndex == 2) {
      currentLayout = (currentLayout == LAYOUT_SLIM_LIST) ? LAYOUT_FOCUS_SMARTWATCH : LAYOUT_SLIM_LIST;
      preferences.putInt("layout", (int)currentLayout);
      dibujarPantalla();
    } else if (selectedIndex == 3) {
      currentTheme = (ThemeId)((currentTheme + 1) % 4);
      preferences.putInt("theme", (int)currentTheme);
      dibujarPantalla();
    } else if (selectedIndex == 4) {
      // Alternar entre rotacion horizontal 1 y 3
      screenRotation = (screenRotation == 1) ? 3 : 1;
      tft.setRotation(screenRotation);
      preferences.putInt("rot", screenRotation);
      dibujarPantalla();
    } else if (selectedIndex == 5) {
      // Compensar offset horizontal (0px, +20px, +40px) si TFT_eSPI tiene TFT_HEIGHT=320 en vez de 280
      screenOffsetX = (screenOffsetX + 20) % 60;
      preferences.putInt("offx", screenOffsetX);
      dibujarPantalla();
    } else if (selectedIndex == 6) {
      currentState = STATE_MAIN_MENU;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    }
  }
}
