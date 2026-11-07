// =========================================================================
// G4BRIEL OS - MOTOR BASE ESP32 + TFT_eSPI (240x280) + GITHUB JS OTA
// Sube este archivo (g4briel_os_esp32.ino) UNA SOLA VEZ a tu ESP32 en Arduino IDE.
// - Guarda y auto-conecta al WiFi usando memoria Flash NVS (Preferences.h)
// - Al iniciar ejecuta la animacion de arranque (boot.js)
// - Descarga manifest.json y tus scripts .js desde tu repositorio de GitHub
// - Incluye Menu GameBoy (Pokemon Amarillo, Azul y Rojo) en g4briel
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
};

const ThemePalette themes[4] = {
  { "Naranja", TFT_ORANGE, 0x3A00, 0x18E3, 0x2965, TFT_WHITE, 0xA510 },
  { "Cian",    TFT_CYAN,   0x01E6, 0x08A6, 0x11AB, TFT_WHITE, 0x54FB },
  { "Verde",   TFT_GREEN,  0x02E0, 0x08A2, 0x11A4, TFT_WHITE, 0x56E5 },
  { "Blanco",  TFT_WHITE,  0x31A6, 0x10A2, 0x4208, TFT_WHITE, 0x8410 }
};

enum ScreenState {
  STATE_MAIN_MENU,
  STATE_G4BRIEL_GAMEBOY,
  STATE_POKEMON_PLAY,
  STATE_SISTEMA_TESTS,
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
};

const int MAX_MENU_ITEMS = 12;
int totalMainItems = 5;
MenuItem mainItems[MAX_MENU_ITEMS] = {
  {"g4briel", "GameBoy: Amarillo, Azul, Rojo", "apps/g4briel.js"},
  {"Configuracion", "WiFi NVS, UI y Color", "system://config"},
  {"GitHub Sync", "Descargar .js del repo", "system://github_sync"},
  {"Monitor ADC", "Osciloscopio GPIO34", "apps/monitor.js"},
  {"Sistema & Tests", "Tests RGB, Botones y Flash", "apps/sysinfo.js"}
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
  TFT_YELLOW,
  TFT_CYAN,
  TFT_RED,
  TFT_WHITE
};
int activePokemonEdition = 0; // 0: Amarillo, 1: Azul, 2: Rojo

// Submenu Sistema & Tests (pruebas movidas aqui)
const int TOTAL_SYS_TESTS = 4;
const char* sysTestTitles[TOTAL_SYS_TESTS] = {
  "Test Pantalla RGB",
  "Test 3 Botones",
  "Memoria & Scripts",
  "< Volver"
};

const int TOTAL_CONFIG = 5;
const char* configTitles[TOTAL_CONFIG] = {
  "Conectar WiFi",
  "Sync GitHub JS",
  "Estilo UI",
  "Tema Color",
  "< Volver"
};

int selectedIndex = 0;
int scrollOffset  = 0;

const char keyboardKeys[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ._-@#$%&*!? ";
const int TOTAL_KEYS = sizeof(keyboardKeys) - 1;
int keyboardIndex = 0;

String wifiTargetSSID = "Mi_Casa_5G";
String wifiPassword   = "";
bool wifiConnected    = false;
bool hasSavedWifi     = false;
String activeJsFile   = "";

const int SCREEN_W = 240;
const int SCREEN_H = 280;
const int STATUS_H = 24;
const int ITEM_H   = 35;
const int ITEM_W   = 206;
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

void setup() {
  Serial.begin(115200);

  pinMode(PIN_UP,   INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_OK,   INPUT_PULLUP);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.invertDisplay(true);

  // 1. Montar sistema de archivos LittleFS en la Flash del ESP32
  LittleFS.begin(true);

  // 2. Cargar WiFi guardado en memoria NVS (Preferences.h)
  preferences.begin("g4briel_wifi", false);
  String savedSSID = preferences.getString("ssid", "");
  String savedPass = preferences.getString("pass", "");
  currentLayout = (MenuLayout)preferences.getInt("layout", (int)currentLayout);
  currentTheme  = (ThemeId)preferences.getInt("theme", (int)currentTheme);

  if (savedSSID.length() > 0) {
    hasSavedWifi = true;
    wifiTargetSSID = savedSSID;
    wifiPassword   = savedPass;
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
  }

  // 3. Ejecutar animacion de arranque apenas inicia
  animacionInicio();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
  }

  dibujarPantalla();
}

void loop() {
  if (currentState == STATE_JS_APP) {
    if (digitalRead(PIN_OK) == LOW || digitalRead(PIN_UP) == LOW || digitalRead(PIN_DOWN) == LOW) {
      currentState = STATE_MAIN_MENU;
      dibujarPantalla();
      delay(250);
    }
    return;
  }

  if (currentState == STATE_WIFI_KEYBOARD) {
    if (digitalRead(PIN_UP) == LOW) {
      keyboardIndex = (keyboardIndex - 1 + TOTAL_KEYS + 2) % (TOTAL_KEYS + 2);
      dibujarTecladoWifi();
      delay(150);
    }
    if (digitalRead(PIN_DOWN) == LOW) {
      keyboardIndex = (keyboardIndex + 1) % (TOTAL_KEYS + 2);
      dibujarTecladoWifi();
      delay(150);
    }
    if (digitalRead(PIN_OK) == LOW) {
      // Si mantiene presionado PIN_OK (Pin 27) por 600ms, conecta directo al WiFi desde cualquier letra
      unsigned long tPress = millis();
      while (digitalRead(PIN_OK) == LOW && (millis() - tPress) < 600) {
        delay(10);
      }
      if ((millis() - tPress) >= 600) {
        conectarWifi(wifiTargetSSID, wifiPassword);
        return;
      }

      if (keyboardIndex == TOTAL_KEYS) {
        if (wifiPassword.length() > 0) wifiPassword.remove(wifiPassword.length() - 1);
      } else if (keyboardIndex == TOTAL_KEYS + 1) {
        conectarWifi(wifiTargetSSID, wifiPassword);
        return;
      } else {
        wifiPassword += keyboardKeys[keyboardIndex];
      }
      dibujarTecladoWifi();
      delay(180);
    }
    return;
  }

  if (currentState == STATE_POKEMON_PLAY) {
    if (digitalRead(PIN_OK) == LOW || digitalRead(PIN_UP) == LOW || digitalRead(PIN_DOWN) == LOW) {
      currentState = STATE_G4BRIEL_GAMEBOY;
      dibujarPantalla();
      delay(250);
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
    delay(180);
  }

  if (digitalRead(PIN_DOWN) == LOW) {
    selectedIndex = (selectedIndex < maxItems - 1) ? (selectedIndex + 1) : 0;
    dibujarPantalla();
    delay(180);
  }

  if (digitalRead(PIN_OK) == LOW) {
    ejecutarSeleccion();
    delay(250);
  }
}

void dibujarIconoWifi(int x, int y, uint16_t color) {
  tft.drawCircleHelper(x + 7, y + 12, 8, 1 | 2, color);
  tft.drawCircleHelper(x + 7, y + 12, 5, 1 | 2, color);
  tft.fillCircle(x + 7, y + 12, 1, color);
}

void dibujarBarraEstado(const char* titulo) {
  const ThemePalette& th = themes[currentTheme];
  tft.fillRect(0, 0, SCREEN_W, STATUS_H, 0x10A2);
  tft.drawFastHLine(0, STATUS_H - 1, SCREEN_W, th.cardBorder);

  tft.setTextColor(th.accent, 0x10A2);
  tft.setTextSize(1);
  tft.drawString(titulo, 8, 8);

  dibujarIconoWifi(SCREEN_W - 50, 2, wifiConnected ? TFT_GREEN : th.accent);

  tft.drawRect(SCREEN_W - 28, 7, 18, 9, TFT_LIGHTGREY);
  tft.fillRect(SCREEN_W - 10, 9, 2, 5, TFT_LIGHTGREY);
  tft.fillRect(SCREEN_W - 26, 9, 12, 5, TFT_GREEN);
}

void animacionInicio() {
  tft.fillScreen(TFT_BLACK);
  const ThemePalette& th = themes[currentTheme];
  for (int r = 10; r <= 32; r += 5) {
    tft.drawCircle(120, 85, r, th.accent);
    delay(28);
  }
  tft.fillRoundRect(106, 71, 28, 28, 6, th.accentBg);
  tft.drawRoundRect(106, 71, 28, 28, 6, th.accent);
  tft.fillRect(116, 81, 8, 8, th.accent);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("G4BRIEL OS", 56, 130);
  tft.setTextColor(th.textSecondary, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("JS Engine - GitHub OTA", 44, 154);

  const int barX = 30, barY = 195, barW = 180, barH = 8;
  tft.drawRoundRect(barX, barY, barW, barH, 4, th.cardBorder);

  for (int p = 0; p <= 100; p += 5) {
    int fillW = map(p, 0, 100, 0, barW - 4);
    tft.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2, th.accent);
    delay(25);
  }
  delay(200);
  tft.fillScreen(TFT_BLACK);
}

// =========================================================================
// DESCARGA OTA DE ARCHIVOS .JS DESDE TU REPOSITORIO GITHUB
// =========================================================================
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
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("GITHUB OTA SYNC");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Repositorio GitHub:", 16, 40);
  tft.setTextColor(th.accent, TFT_BLACK);
  tft.drawString(GITHUB_REPO_URL, 16, 56);

  const int barX = 20, barY = 140, barW = 200, barH = 12;
  tft.drawRoundRect(barX, barY, barW, barH, 4, th.cardBorder);

  descargarArchivoGitHub("manifest.json");
  descargarArchivoGitHub("apps/boot.js");

  for (int i = 0; i < totalMainItems; i++) {
    String script = mainItems[i].scriptPath;
    if (!script.startsWith("system://")) {
      tft.fillRect(20, 110, 200, 20, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(("GET /" + script).c_str(), 20, 110);
      descargarArchivoGitHub(script.c_str());
    }
    int fillW = map(i + 1, 0, totalMainItems, 30, barW - 4);
    tft.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2, th.accent);
    delay(200);
  }

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Scripts .js actualizados!", 40, 180);
  delay(900);

  currentState = STATE_MAIN_MENU;
  selectedIndex = 0;
  dibujarPantalla();
}

// =========================================================================
// INTERPRETE LIGERO DE COMANDOS TFT EN ARCHIVOS .JS DESCARGADOS
// =========================================================================
void ejecutarScriptJS(String scriptPath) {
  const ThemePalette& th = themes[currentTheme];
  currentState = STATE_JS_APP;
  tft.fillScreen(TFT_BLACK);

  File f = LittleFS.open("/" + scriptPath, FILE_READ);
  if (!f) {
    dibujarBarraEstado("APP JS");
    tft.fillRoundRect(12, 40, 216, 120, 8, th.cardBg);
    tft.drawRoundRect(12, 40, 216, 120, 8, th.cardBorder);
    tft.setTextColor(th.accent, th.cardBg);
    tft.setTextSize(1);
    tft.drawString(("Archivo: /" + scriptPath).c_str(), 22, 60);
    tft.setTextColor(TFT_WHITE, th.cardBg);
    tft.drawString("Pulsa 'GitHub Sync' para", 22, 85);
    tft.drawString("descargar este .js del repo", 22, 102);
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
      tft.fillRoundRect(x, y, w, h, 6, th.cardBg);
      tft.drawRoundRect(x, y, w, h, 6, th.cardBorder);
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
      tft.drawString(txt.c_str(), x, y);
    }
  }
  f.close();
}

void dibujarGameBoyPokemon() {
  tft.fillScreen(TFT_BLACK);
  uint16_t col = gameboyColors[activePokemonEdition];
  const char* titles[3]   = { "POKEMON AMARILLO", "POKEMON AZUL", "POKEMON ROJO" };
  const char* pokemons[3] = { "PIKACHU  NV.25",   "BLASTOISE NV.36", "CHARIZARD NV.36" };
  const char* moves[3]    = { "Impactrueno!",     "Hidrobomba!",     "Llamarada!" };

  dibujarBarraEstado(titles[activePokemonEdition]);
  tft.drawRoundRect(10, 32, 220, 140, 6, col);
  tft.setTextColor(col, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString(pokemons[activePokemonEdition], 22, 48);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("HP: 100/100 [============]", 22, 76);

  tft.fillRoundRect(18, 108, 204, 52, 4, 0x08A2);
  tft.drawRoundRect(18, 108, 204, 52, 4, col);
  tft.setTextColor(TFT_WHITE, 0x08A2);
  tft.drawString("Batalla GameBoy Color:", 26, 118);
  tft.setTextColor(col, 0x08A2);
  tft.drawString(moves[activePokemonEdition], 26, 136);

  tft.fillRoundRect(20, 215, 200, 36, 6, 0x18E3);
  tft.drawRoundRect(20, 215, 200, 36, 6, col);
  tft.setTextColor(TFT_WHITE, 0x18E3);
  tft.drawString("OK / UP / DOWN: Volver", 44, 228);
}

void dibujarListaSlim() {
  const ThemePalette& th = themes[currentTheme];
  tft.fillScreen(TFT_BLACK);
  int total = totalMainItems;
  const char* header = "SISTEMA 1.69";
  if (currentState == STATE_G4BRIEL_GAMEBOY) { total = TOTAL_GAMEBOY; header = "G4BRIEL - GAMEBOY"; }
  else if (currentState == STATE_SISTEMA_TESTS) { total = TOTAL_SYS_TESTS; header = "SISTEMA & TESTS"; }
  else if (currentState == STATE_CONFIGURACION) { total = TOTAL_CONFIG; header = "CONFIGURACION"; }
  else if (currentState == STATE_WIFI_SCAN) { total = totalScannedNets; header = "SELECCIONAR WIFI"; }
  dibujarBarraEstado(header);

  if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
  else if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;

  int startY = STATUS_H + 7;
  int itemX = (SCREEN_W - ITEM_W) / 2 - 2;

  for (int i = 0; i < VISIBLE_ITEMS; i++) {
    int itemIdx = scrollOffset + i;
    if (itemIdx >= total) break;

    int y = startY + (i * (ITEM_H + SPACING));
    bool isSel = (itemIdx == selectedIndex);
    String label = mainItems[itemIdx].title;
    uint16_t itemAccent = th.accent;

    if (currentState == STATE_G4BRIEL_GAMEBOY) {
      label = String(gameboyTitles[itemIdx]);
      itemAccent = gameboyColors[itemIdx];
    } else if (currentState == STATE_SISTEMA_TESTS) {
      label = String(sysTestTitles[itemIdx]);
    } else if (currentState == STATE_CONFIGURACION) {
      label = String(configTitles[itemIdx]);
    } else if (currentState == STATE_WIFI_SCAN) {
      label = scannedSSIDs[itemIdx];
    }

    if (isSel) {
      tft.fillRoundRect(itemX, y, ITEM_W, ITEM_H, 6, th.accentBg);
      tft.drawRoundRect(itemX, y, ITEM_W, ITEM_H, 6, itemAccent);
      tft.fillRect(itemX + 2, y + 6, 3, ITEM_H - 12, itemAccent);
      tft.setTextColor(TFT_WHITE, th.accentBg);
    } else {
      tft.fillRoundRect(itemX, y, ITEM_W, ITEM_H, 6, th.cardBg);
      tft.drawRoundRect(itemX, y, ITEM_W, ITEM_H, 6, th.cardBorder);
      tft.setTextColor(currentState == STATE_G4BRIEL_GAMEBOY ? itemAccent : TFT_LIGHTGREY, th.cardBg);
    }
    tft.setTextSize(2);
    tft.drawString(label.c_str(), itemX + 24, y + (ITEM_H - 14) / 2);
  }
}

void dibujarFocusCard() {
  const ThemePalette& th = themes[currentTheme];
  tft.fillScreen(TFT_BLACK);
  const char* header = "SISTEMA 1.69";
  String curTitle = mainItems[selectedIndex].title;
  String curSub   = mainItems[selectedIndex].subtitle;
  uint16_t accent = th.accent;

  if (currentState == STATE_G4BRIEL_GAMEBOY) {
    header = "G4BRIEL - GAMEBOY";
    curTitle = String(gameboyTitles[selectedIndex]);
    curSub = "GameBoy Color Edition";
    accent = gameboyColors[selectedIndex];
  } else if (currentState == STATE_SISTEMA_TESTS) {
    header = "SISTEMA & TESTS";
    curTitle = String(sysTestTitles[selectedIndex]);
    curSub = "Diagnostico de Hardware";
  } else if (currentState == STATE_CONFIGURACION) {
    header = "CONFIGURACION";
    curTitle = String(configTitles[selectedIndex]);
    curSub = "OK para cambiar";
  } else if (currentState == STATE_WIFI_SCAN) {
    header = "SELECCIONAR WIFI";
    curTitle = scannedSSIDs[selectedIndex];
    curSub = "OK para elegir esta red";
  }
  dibujarBarraEstado(header);

  int centerY = STATUS_H + 48;
  tft.fillRoundRect(14, centerY, 212, 100, 10, th.accentBg);
  tft.drawRoundRect(14, centerY, 212, 100, 10, accent);

  tft.setTextColor(accent, th.accentBg);
  tft.setTextSize(2);
  tft.drawString(curTitle.c_str(), 28, centerY + 20);

  tft.setTextColor(th.textSecondary, th.accentBg);
  tft.setTextSize(1);
  tft.drawString(curSub.c_str(), 28, centerY + 52);
}

void dibujarTecladoWifi() {
  const ThemePalette& th = themes[currentTheme];
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("TECLADO WIFI");

  tft.fillRoundRect(8, 28, 224, 54, 6, th.cardBg);
  tft.drawRoundRect(8, 28, 224, 54, 6, th.cardBorder);
  tft.setTextColor(th.accent, th.cardBg);
  tft.setTextSize(1);
  tft.drawString(("Red: " + wifiTargetSSID + " (" + String(wifiPassword.length()) + ")").c_str(), 14, 32);

  tft.setTextColor(TFT_WHITE, th.cardBg);
  String fullPass = wifiPassword + "_";
  if (fullPass.length() <= 16) {
    tft.setTextSize(2);
    tft.drawString(fullPass.c_str(), 14, 48);
  } else {
    // Si la contrasena es muy larga, usar 2 lineas de tamano 1 (hasta 64 caracteres sin salirse de pantalla)
    tft.setTextSize(1);
    String vis = fullPass;
    if (vis.length() > 62) vis = "<<" + vis.substring(vis.length() - 60);
    String l1 = vis.substring(0, min((unsigned int)32, vis.length()));
    tft.drawString(l1.c_str(), 14, 48);
    if (vis.length() > 32) {
      String l2 = vis.substring(32);
      tft.setTextColor(TFT_GREEN, th.cardBg);
      tft.drawString(l2.c_str(), 14, 62);
    }
  }

  int drumY = 90;
  tft.fillRoundRect(8, drumY, 224, 85, 8, 0x08A2);
  tft.drawRoundRect(8, drumY, 224, 85, 8, th.cardBorder);

  int kx = 104, ky = drumY + 24;
  tft.fillRoundRect(kx - 8, ky, 48, 34, 6, th.accentBg);
  tft.drawRoundRect(kx - 8, ky, 48, 34, 6, th.accent);
  tft.setTextColor(TFT_WHITE, th.accentBg);

  if (keyboardIndex == TOTAL_KEYS) {
    tft.setTextSize(2);
    tft.drawString("BORR", kx - 4, ky + 10);
  } else if (keyboardIndex == TOTAL_KEYS + 1) {
    tft.setTextColor(TFT_GREEN, th.accentBg);
    tft.setTextSize(2);
    tft.drawString("OK!", kx, ky + 10);
  } else {
    char curChar[2] = { keyboardKeys[keyboardIndex], '\0' };
    tft.setTextSize(3);
    tft.drawString(curChar, kx + 8, ky + 6);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("1 clic UP desde 'a' = [OK! Conectar]", 12, 195);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("O manten presionado OK (0.6s)", 24, 214);
}

void escanearRedesWifi() {
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("BUSCANDO WIFI...");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Escaneando...", 40, 110);

  int n = WiFi.scanNetworks();
  totalScannedNets = 0;
  for (int i = 0; i < n && totalScannedNets < (MAX_WIFI_NETS - 1); i++) {
    scannedSSIDs[totalScannedNets++] = WiFi.SSID(i).substring(0, 16);
  }
  scannedSSIDs[totalScannedNets++] = "< Volver";

  currentState = STATE_WIFI_SCAN;
  selectedIndex = 0;
  scrollOffset = 0;
  dibujarPantalla();
}

void conectarWifi(String ssid, String pass) {
  tft.fillScreen(TFT_BLACK);
  dibujarBarraEstado("CONECTANDO...");
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
  dibujarPantalla();
}

void dibujarPantalla() {
  if (currentState == STATE_WIFI_KEYBOARD) {
    dibujarTecladoWifi();
    return;
  }
  if (currentState == STATE_POKEMON_PLAY) {
    dibujarGameBoyPokemon();
    return;
  }
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
      activePokemonEdition = selectedIndex;
      currentState = STATE_POKEMON_PLAY;
      dibujarPantalla();
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
      tft.fillScreen(TFT_RED); delay(350);
      tft.fillScreen(TFT_GREEN); delay(350);
      tft.fillScreen(TFT_BLUE); delay(350);
      dibujarPantalla();
    } else if (selectedIndex == 1 || selectedIndex == 2) {
      ejecutarScriptJS("apps/sysinfo.js");
    } else {
      currentState = STATE_MAIN_MENU;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    }
    return;
  }

  if (currentState == STATE_WIFI_SCAN) {
    if (selectedIndex == totalScannedNets - 1) {
      currentState = STATE_CONFIGURACION;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    } else {
      wifiTargetSSID = scannedSSIDs[selectedIndex];
      wifiPassword = "";
      currentState = STATE_WIFI_KEYBOARD;
      keyboardIndex = 0;
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
      currentState = STATE_MAIN_MENU;
      selectedIndex = 0;
      scrollOffset = 0;
      dibujarPantalla();
    }
  }
}
