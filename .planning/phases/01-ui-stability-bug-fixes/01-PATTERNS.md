# Phase 1: UI Stability & Bug Fixes — Pattern Map

**Mapped:** 2026-05-13
**Files analyzed:** 7 (ui_settings.cpp, ui_main.cpp, ui_helpers.cpp, streamdeck.cpp, ble_actions.cpp, storage.cpp, constants.h)
**Analogs found:** 7 / 7 (todos os arquivos-alvo são os próprios analogs desta fase)

---

## File Classification

| Arquivo | Role | Data Flow | Analog principal | Qualidade |
|---------|------|-----------|-----------------|-----------|
| `src/ui_settings.cpp` | UI screen manager | event-driven | `src/ui_main.cpp` | exact |
| `src/ui_main.cpp` | UI screen manager | event-driven | `src/ui_settings.cpp` | exact |
| `src/ui_helpers.cpp` | UI utility | event-driven | `src/ui_settings.cpp` | role-match |
| `src/streamdeck.cpp` | app controller | event-driven | — | primary |
| `src/ble_actions.cpp` | service | request-response | — | primary |
| `src/storage.cpp` | service | CRUD / file-I/O | — | primary |
| `src/constants.h` | config | — | — | primary |

---

## Pattern Assignments

---

### PADRÃO 1 — Ciclo de Vida de Tela LVGL (Screen Lifecycle)

**Analogs:** `src/ui_settings.cpp`, `src/ui_main.cpp`, `src/ui_helpers.cpp`

#### Variante A — Tela com rebuild condicional (`create_settings_ui`)

Aplicar quando a tela precisa ser cacheada entre visitas (ex.: settings list).

**Declaração de estado** (`ui_settings.cpp` linhas 11-17):
```cpp
static lv_obj_t* g_settings_screen = nullptr;
static bool g_settings_needs_rebuild = true;
```

**Lógica de criação** (`ui_settings.cpp` linhas 61-116):
```cpp
void create_settings_ui() {
    if (g_settings_screen == nullptr || g_settings_needs_rebuild) {
        if (g_settings_screen != nullptr) {
            lv_obj_del(g_settings_screen);
        }
        g_settings_screen = lv_obj_create(NULL);
        lv_obj_clear_flag(g_settings_screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(g_settings_screen, lv_color_hex(g_bg_color), LV_PART_MAIN);
        // ... widgets ...
        g_settings_needs_rebuild = false;
    }
    lv_scr_load(g_settings_screen);        // <-- sempre ao final
}
```

**Regra:** `lv_scr_load()` é chamado **fora** do bloco `if`, sempre ao final da função.

---

#### Variante B — Tela descartável (sempre recria, ex.: `create_edit_ui`, `create_wifi_ui`, `create_button_list_ui`)

Aplicar quando o conteúdo depende de estado mutável (índice do botão, WiFi atual, etc.).

**Padrão de deleção + criação** (`ui_settings.cpp` linhas 123-126):
```cpp
if (g_edit_screen) { lv_obj_del(g_edit_screen); g_edit_screen = nullptr; }
g_edit_screen = lv_obj_create(NULL);
lv_obj_clear_flag(g_edit_screen, LV_OBJ_FLAG_SCROLLABLE);
lv_scr_load(g_edit_screen);   // <-- imediatamente após criação
```

**Regra:** `lv_scr_load()` é chamado **imediatamente** após `lv_obj_create(NULL)`, antes de adicionar widgets.

---

#### Variante C — Tela reciclada com `lv_obj_clean` (ex.: `create_main_ui`)

Aplicar quando a tela raiz nunca é deletada (`g_main_screen = lv_scr_act()` em setup).

**Padrão** (`ui_main.cpp` linhas 57-67):
```cpp
void create_main_ui() {
    lv_obj_clean(g_main_screen);           // limpa filhos sem deletar a tela
    lv_obj_set_style_bg_color(g_main_screen, lv_color_hex(g_bg_color), LV_PART_MAIN);
    lv_obj_clear_flag(g_main_screen, LV_OBJ_FLAG_SCROLLABLE);
    // ponteiros estáticos resetados manualmente:
    memset(g_btns, 0, sizeof(g_btns));
    g_grid = nullptr;
    // ... rebuilds todos os widgets ...
}
```

**Regra:** Após `lv_obj_clean`, todos os ponteiros estáticos de widgets filhos DEVEM ser zerados (memset + nullptr) antes de recriar.

---

#### Destruição assíncrona ao navegar para trás

**Padrão** (`ui_settings.cpp` linhas 453-455):
```cpp
if (g_edit_screen)        { lv_obj_del_async(g_edit_screen);        g_edit_screen        = nullptr; }
if (g_wifi_screen)        { lv_obj_del_async(g_wifi_screen);        g_wifi_screen        = nullptr; }
if (g_button_list_screen) { lv_obj_del_async(g_button_list_screen); g_button_list_screen = nullptr; }
```

**Regra:** Nunca use `lv_obj_del` síncrono dentro de um callback de evento — sempre use `lv_obj_del_async`. O ponteiro deve ser zerado para `nullptr` imediatamente (antes da chamada async terminar) para evitar double-free.

---

### PADRÃO 2 — Flags de Scroll LVGL

**Analogs:** `src/ui_settings.cpp`, `src/ui_main.cpp`, `src/ui_helpers.cpp`

#### Regra geral: toda tela raiz é não-scrollável

Aplicado em **todas** as telas sem exceção (`ui_settings.cpp` linha 69, `ui_main.cpp` linha 60, `ui_helpers.cpp` linha 39):
```cpp
lv_obj_clear_flag(<screen>, LV_OBJ_FLAG_SCROLLABLE);
```

#### Regra para listas (`lv_list`): bloquear propagação vertical de scroll

Aplicado em **todo** `lv_list` do projeto (`ui_settings.cpp` linhas 80 e 394, `ui_helpers.cpp` linha 51):
```cpp
lv_obj_t* list = lv_list_create(<parent>);
lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
```

**Por quê:** Sem `LV_OBJ_FLAG_SCROLL_CHAIN_VER` removido, o scroll do `lv_list` vaza para o container pai (tela), causando scroll indesejado da tela inteira.

#### Regra para containers internos não-scrolláveis

Containers de layout (grid, nav_cont, page_box, right_box) também recebem o clear:
```cpp
lv_obj_clear_flag(g_grid,    LV_OBJ_FLAG_SCROLLABLE);  // ui_main.cpp linha 96
lv_obj_clear_flag(nav_cont,  LV_OBJ_FLAG_SCROLLABLE);  // ui_main.cpp linha 164
lv_obj_clear_flag(page_box,  LV_OBJ_FLAG_SCROLLABLE);  // ui_main.cpp linha 185
lv_obj_clear_flag(right_box, LV_OBJ_FLAG_SCROLLABLE);  // ui_main.cpp linha 214
```

#### Regra para slider no nav bar: bloquear propagação vertical

(`ui_main.cpp` linhas 174-175):
```cpp
// Prevent vertical drag on the slider from propagating as a scroll gesture.
lv_obj_clear_flag(g_slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
```

**Checklist para qualquer novo widget em container scrollável:**
1. Se for tela raiz → `lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE)`
2. Se for `lv_list` → `lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN_VER)`
3. Se for slider dentro de layout → `lv_obj_clear_flag(slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER)`
4. Se for container de layout interno → `lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE)`

---

### PADRÃO 3 — Sleep Overlay (`g_sleep_overlay`)

**Analog:** `src/streamdeck.cpp` linhas 13-53

#### Estrutura completa do overlay

**Declaração** (`streamdeck.cpp` linha 13):
```cpp
static lv_obj_t* g_sleep_overlay = nullptr;
```

**Criação** (`streamdeck.cpp` linhas 24-37):
```cpp
static void enter_sleep() {
    pt_set_backlight(0, false);              // apaga display

    g_sleep_overlay = lv_obj_create(lv_scr_act());   // pai = tela ativa ATUAL
    lv_obj_set_size(g_sleep_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(g_sleep_overlay, 0, 0);
    lv_obj_set_style_bg_opa(g_sleep_overlay,    LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_sleep_overlay, 0,          LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_sleep_overlay,     0,           LV_PART_MAIN);
    lv_obj_add_flag(g_sleep_overlay,   LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_sleep_overlay,   LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(g_sleep_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_sleep_overlay, sleep_overlay_cb, LV_EVENT_PRESSED, nullptr);
}
```

**Wake callback** (`streamdeck.cpp` linhas 15-22):
```cpp
static void sleep_overlay_cb(lv_event_t* e) {
    (void)e;
    lv_obj_t* overlay = g_sleep_overlay;
    g_sleep_overlay    = nullptr;            // zera ANTES do del_async
    pt_set_backlight(g_brightness, false);
    pt_last_touch_ms = millis();
    lv_obj_del_async(overlay);              // destrói referência local, não g_sleep_overlay
}
```

**Regra crítica:** O ponteiro global `g_sleep_overlay` é zerado ANTES de chamar `lv_obj_del_async`. A variável local `overlay` guarda a referência para o del_async. Isso evita que `check_sleep_status` detecte o overlay como presente durante o ciclo de deleção.

**Verificação de presença** (`streamdeck.cpp` linha 45):
```cpp
if (g_sleep_overlay != nullptr) return;   // já dormindo, não fazer nada
```

**Limpeza quando a UI é reconstruída** (`streamdeck.cpp` linhas 101-105):
```cpp
if (g_sleep_overlay) {
    g_sleep_overlay = nullptr;
    pt_set_backlight(g_brightness, false);
    pt_last_touch_ms = millis();
}
// lv_obj_clean dentro de create_main_ui() destrói o overlay automaticamente
```

**Regra:** Quando `create_main_ui()` é chamado (que executa `lv_obj_clean`), o overlay é destruído como filho da tela. O ponteiro global DEVE ser zerado antes para evitar dangling pointer.

---

### PADRÃO 4 — BTN_TYPE_DISABLED

**Analogs:** `src/constants.h` linha 18, `src/ble_actions.cpp` linhas 171 e 168-177, `src/ui_main.cpp` linhas 151-155, `src/storage.cpp` linhas 83-84 e 121-122

#### Definição canônica (`constants.h` linha 18):
```cpp
enum ButtonType {
    BTN_TYPE_APP = 0,
    BTN_TYPE_MEDIA,
    BTN_TYPE_BASIC_COMBO,
    BTN_TYPE_ADV_COMBO,
    BTN_TYPE_DISABLED      // sempre o último valor do enum
};
```

#### Default de inicialização — storage (`storage.cpp` linhas 118-122):
```cpp
for (int i = 0; i < MAX_TOTAL_BUTTONS; i++) {
    memset(&g_configs[i], 0, sizeof(ButtonConfig));
    g_configs[i].color = 0x333333;
    g_configs[i].type  = BTN_TYPE_DISABLED;   // todo botão nasce disabled
}
```

#### Guard na execução — BLE (`ble_actions.cpp` linhas 162-177):
```cpp
void handle_button_action(uint8_t idx) {
    if (!bleKeyboard.isConnected()) return;
    if (idx >= MAX_TOTAL_BUTTONS) return;
    ButtonConfig& cfg = g_configs[idx];

    if (cfg.type == BTN_TYPE_DISABLED) return;   // sai ANTES de qualquer ação
    // ... guard de value vazio ...
    const char* v = cfg.value;
    while (*v == ' ' || *v == '\t') v++;
    if (*v == '\0') return;
```

#### Render visual — UI (`ui_main.cpp` linhas 151-155):
```cpp
if (g_configs[global_idx].type == BTN_TYPE_DISABLED) {
    lv_obj_add_state(btn, LV_STATE_DISABLED);
    // NOTA: evento NÃO é registrado — btn_event_cb não é adicionado
} else {
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
}
```

**Regra:** Botões `BTN_TYPE_DISABLED` recebem `LV_STATE_DISABLED` via `lv_obj_add_state` e NUNCA têm `lv_obj_add_event_cb` registrado. A defesa em `handle_button_action` é redundante mas mantida como segunda linha de defesa.

---

### PADRÃO 5 — Persistência de Settings (NVS + LittleFS)

**Analog:** `src/storage.cpp`

#### Escopo das responsabilidades

| Dado | Onde salvo | Função |
|------|-----------|--------|
| Configurações escalares (rows, cols, os, lang, bg, brightness, etc.) | NVS via `Preferences` (namespace `"deck"`) | `save_settings()` / `load_settings()` |
| Configurações de botões (label, value, icon, color, type, imgPath) | LittleFS binário (`/win_btns_v2.bin`, etc.) | `save_settings(bool saveButtons=true)` |

#### Assinatura canônica (`storage.cpp` linha 134):
```cpp
void save_settings(bool saveButtons = true);
```

**Padrão de uso:**
- `save_settings()` — salva NVS + botões (padrão, ex.: save_edit_cb, save_wifi_cb)
- `save_settings(false)` — salva apenas NVS (ex.: page nav, grid change, sleep change)

#### Bloco NVS (`storage.cpp` linhas 135-147):
```cpp
void save_settings(bool saveButtons) {
    preferences.begin("deck", false);
    preferences.putUInt("bg",        g_bg_color);
    preferences.putUChar("bright",   g_brightness);
    preferences.putUChar("rows",     g_rows);
    preferences.putUChar("cols",     g_cols);
    preferences.putUChar("os",       g_target_os);
    preferences.putUChar("lang",     g_kb_lang);
    preferences.putUChar("num_pages",g_num_pages);
    preferences.putUChar("sleep_to", g_sleep_timeout);
    preferences.putUChar("page",     g_current_page);
    preferences.putString("wssid",   g_wifi_ssid);
    preferences.putString("wpass",   g_wifi_pass);
    preferences.end();
    // ...
}
```

**Regra:** `preferences.begin()` e `preferences.end()` SEMPRE em par dentro do mesmo escopo. Nunca deixar aberto entre chamadas.

#### Validação pós-load (`storage.cpp` linhas 52-58):
```cpp
if (g_bg_color == 0x000000) g_bg_color = 0x121212;
if (g_brightness < 1) g_brightness = 50;
if (g_brightness > 100) g_brightness = 100;
if (g_rows < 1 || g_rows > 5) g_rows = 3;
if (g_cols < 1 || g_cols > 5) g_cols = 3;
if (g_num_pages < 1 || g_num_pages > MAX_PAGES) g_num_pages = MAX_PAGES;
if (g_current_page >= g_num_pages) g_current_page = 0;
```

**Regra:** Todo valor lido do NVS DEVE ter validação de range imediata em `load_settings`. Nunca confiar no NVS sem validar. Qualquer nova configuração adicionada deve seguir este padrão.

#### Sequência após mudança de configuração

Padrão completo quando uma setting muda e a UI precisa ser reconstruída (`ui_settings.cpp` linhas 307-317):
```cpp
static void grid_selected(const char* txt) {
    // 1. atualiza a global
    g_cols = 3; g_rows = 3;  // (exemplo)
    // 2. persiste
    save_settings();
    // 3. volta para tela principal
    lv_scr_load(g_main_screen);
    // 4. reconstrói UI
    create_main_ui();
}
```

Quando apenas NVS muda (sem rebuild de UI obrigatório):
```cpp
save_settings(false);   // ex.: sleep_selected, page_nav_cb
```

---

## Shared Patterns (Padrões Transversais)

### A — Background Color Global

**Fonte:** `src/storage.cpp` linha 16, aplicado em toda tela
```cpp
uint32_t g_bg_color = 0x121212;  // definido em storage.cpp, extern em storage.h
```

**Aplicação em toda tela nova:**
```cpp
lv_obj_set_style_bg_color(<screen>, lv_color_hex(g_bg_color), LV_PART_MAIN);
```

**Regra:** Nenhuma tela usa cor hardcoded. Sempre `lv_color_hex(g_bg_color)`.

---

### B — Callback de Evento: Assinatura Padrão

**Padrão uniforme em todo o projeto:**
```cpp
static void my_cb(lv_event_t* e) {
    // Para dados simples (índice inteiro):
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    // Para dados complexos (struct):
    MyData* data = (MyData*)lv_event_get_user_data(e);
    // Para o objeto que disparou:
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
}
```

**Registro:**
```cpp
lv_obj_add_event_cb(obj, my_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)integer_value);
lv_obj_add_event_cb(obj, my_cb, LV_EVENT_CLICKED, &struct_data);
```

---

### C — Localização (L10n)

**Padrão de acesso a strings localizadas (`ui_settings.cpp` linha 62):**
```cpp
void create_settings_ui() {
    const L10n* l = get_l10n();   // primeira linha de toda função create_*
    // uso: l->settings_title, l->back_btn, etc.
}
```

**Regra:** `get_l10n()` é chamado uma vez por função, resultado guardado em `const L10n* l`. Nunca chamar `get_l10n()` diretamente em parâmetros de função.

---

### D — Ícones FontAwesome como UTF-8 Literal

**Padrão uniforme:**
```cpp
lv_label_set_text_fmt(lbl, "\xEF\x81\x93 %s", l->back_btn);   // ícone + espaço + texto
lv_label_set_text(label, "\xEF\x87\xAB");                       // ícone standalone
```

**Regra:** Ícones são sempre sequências `\xEF\x8x\xNN` inline. Não usar defines ou constantes separadas.

---

### E — Fontes Disponíveis

Fontes usadas ativamente no projeto (aplicar por contexto):
```cpp
&lv_font_montserrat_12   // texto muito pequeno (grid 5x3)
&lv_font_montserrat_14   // labels de botão padrão
&lv_font_montserrat_18   // títulos de tela
&lv_font_montserrat_24   // ícones de botão padrão
```

---

## No Analog Found

Não há arquivos sem analog nesta fase. Todos os arquivos a modificar já existem e são os próprios analogs.

---

## Metadata

**Scope de busca:** `/home/besser/Dev/PandaTouch_streamDeck/src/`
**Arquivos lidos:** 7
**Data de extração:** 2026-05-13
