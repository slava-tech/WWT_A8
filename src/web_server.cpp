// =================================================================================

#include "sensors.h" 
#include "hardware.h" 
#include "web_server.h"
#include "utils.h"

// --- Секция 12: HTML, CSS, JavaScript для веб-интерфейса ---
// Здесь находится полный код вашей оригинальной веб-страницы.
const char index_html[] PROGMEM = R"====(
// ... HTML код ...
)====";


// --- Вспомогательные функции (реализация тех, что объявлены в utils.h) ---

int tileIndexById(const String& id){
  for (uint8_t i=0; i < 6; i++) {
    if (id == TILES[i].id) return (int)i;
  }
  return -1;
}

const TileDef& getTile(uint8_t idx){
  return TILES[(idx < 6) ? idx : 0];
}

static bool tileIdExists(const String& id) {
  for (uint8_t i=0; i < 6; i++) {
    if (id == TILES[i].id) return true;
  }
  return false;
}

String getProfileId(uint8_t cont) {
  prefsProfiles.begin("profiles", true);
  String key = "c" + String(cont) + ".profile";
  String id = prefsProfiles.getString(key.c_str(), "CUSTOM_6");
  prefsProfiles.end();
  if (!tileIdExists(id)) {
    id = "CUSTOM_6";
  }
  return id;
}


// --- Обработчики HTTP-запросов ---

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleNotFound(){
  server.send(404, "text/plain", "Not found");
}

// ... (остальные обработчики handleOwScan, handleOwStatus и т.д.)


// --- Функция настройки сервера ---

void setupWebServer() {
    server.on("/", handleRoot);
    // ... (все остальные server.on(...))
    server.onNotFound(handleNotFound);
    server.begin();
}

void dumpNvsToSerial(){
  Serial.println(F("\n========== NVS DUMP =========="));
  Serial.println(F("[NVS/Profiles]"));
  for (uint8_t c=1; c<=2; c++){
    String id = getProfileId(c);
    int idx = tileIndexById(id);
    const TileDef& td = getTile((idx>=0) ? (uint8_t)idx : 0);
    Serial.print(F("  c")); Serial.print(c);
    Serial.print(F(".profile = \""));
    Serial.print(id); Serial.print(F("\"  ("));
    Serial.print(td.displayName); Serial.println(F(")"));
  }

  Serial.println(F("[NVS/OWMAP]"));
  prefs.begin("owmap", true);
  for (size_t i=0; i < OW_VAR_COUNT; i++){
    String rom = prefs.getString(OW_VARS[i], String());
    if (!rom.length()) continue;
    Serial.print(F("  ")); Serial.print(OW_VARS[i]);
    Serial.print(F(" -> ")); Serial.println(rom);
  }
  prefs.end();

  Serial.println(F("[NVS/Params]"));
  prefsParams.begin("params", true);
  for (int i=0; i<6; i++) {
      const char* tzad = TILES[i].TZAD;
      if (tzad && tzad[0]) {
          if (prefsParams.isKey(tzad)) {
              float val = prefsParams.getFloat(tzad, 0.0f);
              Serial.print(F("  ")); Serial.print(tzad);
              Serial.print(F(" = ")); Serial.println(val);
          }
      }
  }
  prefsParams.end();
  
  Serial.println(F("[NVS/General]"));
  prefsGeneral.begin("general", true);
  Serial.print(F("  pumpEnableMask = ")); Serial.println(prefsGeneral.getUChar("pumpEnableMask", 0b1111), BIN);
  // ... (добавьте сюда вывод остальных параметров из prefsGeneral по аналогии, если нужно)
  prefsGeneral.end();

  Serial.println(F("================================\n"));
}