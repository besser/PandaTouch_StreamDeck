---
phase: 01-ui-stability-bug-fixes
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - src/ui_settings.cpp
  - src/streamdeck.cpp
autonomous: true
requirements:
  - R1.1
  - R1.2
  - R1.3

must_haves:
  truths:
    - "Botao Disabled no deck principal aparece cinza e nao produz acao BLE ao toque"
    - "O settings menu exibe exatamente 8 itens sem necessitar de scroll"
    - "A lista de botoes abre em tela separada, pode ser rolada livremente e retorna sem crash"
    - "Navegar para settings e voltar multiplas vezes nao aumenta o uso de heap (sem leak)"
    - "Deixar o device na tela de settings ate o sleep timeout nao bloqueia o sono para sempre"
    - "Sliders RGB da edit screen nao propagam gesto vertical para a tela pai"
  artifacts:
    - path: "src/streamdeck.cpp"
      provides: "enter_sleep() corrigida — overlay sempre filho de g_main_screen, guarda lv_scr_act() == g_main_screen"
      contains: "lv_scr_act() == g_main_screen"
    - path: "src/ui_settings.cpp"
      provides: "create_rgb_slider com SCROLL_CHAIN_VER limpo; back_to_main_cb com zeragem do overlay orfao; callbacks de selecao diretos zeram g_sleep_overlay antes de create_main_ui"
      contains: "LV_OBJ_FLAG_SCROLL_CHAIN_VER"
  key_links:
    - from: "src/streamdeck.cpp::enter_sleep"
      to: "g_main_screen"
      via: "guarda lv_scr_act() == g_main_screen antes de criar overlay"
      pattern: "lv_scr_act\\(\\).*g_main_screen"
    - from: "src/ui_settings.cpp::back_to_main_cb"
      to: "g_sleep_overlay"
      via: "detecta e destrói overlay orfao antes de lv_obj_del_async"
      pattern: "g_sleep_overlay.*lv_obj_get_parent"
    - from: "src/ui_settings.cpp::grid_selected,os_selected,pages_selected"
      to: "create_main_ui"
      via: "zera g_sleep_overlay antes de chamar create_main_ui"
      pattern: "g_sleep_overlay = nullptr"
---

<objective>
Fechar os bugs criticos de estabilidade da fase 1 que ainda restam no working tree e
garantir que todas as mudancas ja aplicadas (R1.1, R1.2, R1.3) estejam corretas e
commitadas de forma atomica por requisito.

Purpose: Entregar um firmware crash-free antes de qualquer nova feature. Os tres requisitos
(settings usavel, sem leak de memoria LVGL, botao Disabled completo) estao parcialmente
implementados no working tree. Este plano cobre as duas correcoes pendentes e organiza os
commits finais.

Output:
  - src/streamdeck.cpp: enter_sleep() com guarda de tela + overlay como filho de g_main_screen
  - src/ui_settings.cpp: sliders RGB com SCROLL_CHAIN_VER limpo + zeragem de overlay orfao
    em back_to_main_cb + zeragem de g_sleep_overlay nos callbacks de selecao diretos
  - Tres commits atomicos (um por requisito: R1.3, R1.2, R1.1)
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/phases/01-ui-stability-bug-fixes/RESEARCH.md
@.planning/phases/01-ui-stability-bug-fixes/01-PATTERNS.md
@.planning/codebase/CONVENTIONS.md
</context>

<interfaces>
<!-- Contratos e padroes criticos para este plano. Extraidos dos arquivos-fonte. -->

De src/streamdeck.cpp (estado atual — A CORRIGIR):
```cpp
// BUG: overlay criado como filho de lv_scr_act() — pode ser g_settings_screen
static void enter_sleep() {
    pt_set_backlight(0, false);
    g_sleep_overlay = lv_obj_create(lv_scr_act());   // <-- problema aqui
    // ...
}

// Zeragem correta no path g_pending_ui_update (REFERENCIA — replicar nos outros paths):
if (g_sleep_overlay) {
    g_sleep_overlay = nullptr;
    pt_set_backlight(g_brightness, false);
    pt_last_touch_ms = millis();
}
```

De src/ui_settings.cpp — create_rgb_slider (estado atual — A CORRIGIR):
```cpp
// Linhas 207-215: lambda nao tem LV_OBJ_FLAG_SCROLL_CHAIN_VER
auto create_rgb_slider = [&](lv_obj_t** slider, int y, uint8_t val, lv_color_t color) {
    *slider = lv_slider_create(g_edit_screen);
    lv_obj_set_size(*slider, 200, 15);
    lv_obj_align(*slider, LV_ALIGN_TOP_LEFT, panel_x, y);
    lv_slider_set_range(*slider, 0, 255);
    lv_slider_set_value(*slider, val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(*slider, color, LV_PART_KNOB);
    lv_obj_add_event_cb(*slider, color_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // FALTA: lv_obj_clear_flag(*slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
};
```

De src/ui_settings.cpp — back_to_main_cb (estado atual):
```cpp
static void back_to_main_cb(lv_event_t* e) {
    g_editing_bg = false;
    lv_scr_load(g_main_screen);
    if (g_edit_screen)        { lv_obj_del_async(g_edit_screen);        g_edit_screen        = nullptr; }
    if (g_wifi_screen)        { lv_obj_del_async(g_wifi_screen);        g_wifi_screen        = nullptr; }
    if (g_button_list_screen) { lv_obj_del_async(g_button_list_screen); g_button_list_screen = nullptr; }
    // FALTA: deteccao e limpeza de overlay orfao antes dos del_async
    if (g_settings_needs_rebuild) {
        create_main_ui();
        g_settings_needs_rebuild = false;
    } else {
        refresh_main_ui();
    }
}
```

De src/ui_settings.cpp — callbacks de selecao diretos (estado atual — A CORRIGIR):
```cpp
// grid_selected (linha 307), os_selected (linha 319), pages_selected (linha 366)
// chamam create_main_ui() sem zerar g_sleep_overlay antes.
// Se a Opcao B for adotada (overlay sempre filho de g_main_screen),
// lv_obj_clean dentro de create_main_ui destroi o overlay como filho,
// mas g_sleep_overlay fica dangling — DEVE ser zerado antes.

static void grid_selected(const char* txt) {
    // ...
    save_settings();
    lv_scr_load(g_main_screen);
    // FALTA: if (g_sleep_overlay) { g_sleep_overlay = nullptr; }
    create_main_ui();   // executa lv_obj_clean internamente
}
```

Padrao de referencia — analog existente correto (streamdeck.cpp:101-105):
```cpp
if (g_sleep_overlay) {
    g_sleep_overlay = nullptr;
    pt_set_backlight(g_brightness, false);
    pt_last_touch_ms = millis();
}
lv_scr_load(g_main_screen);
create_main_ui();
```

Analog correto para SCROLL_CHAIN_VER — slider de brilho (ui_main.cpp:174-175):
```cpp
lv_obj_clear_flag(g_slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
```
</interfaces>

<tasks>

<!-- ================================================================
     GRUPO 1 — R1.3: Botao Disabled (completar e commitar)
     Estado: BTN_TYPE_DISABLED guard, default storage, web dashboard e l10n
     ja estao no working tree. Falta apenas o fix do slider RGB.
     ================================================================ -->

<task type="auto" tdd="false">
  <name>Tarefa 1: Corrigir scroll chain dos sliders RGB na edit screen (R1.3)</name>
  <files>src/ui_settings.cpp</files>
  <action>
Dentro do lambda create_rgb_slider (linhas 207-215 de ui_settings.cpp), adicionar a linha
abaixo imediatamente antes de lv_obj_add_event_cb — exatamente como foi feito para o
slider de brilho em ui_main.cpp:175:

    lv_obj_clear_flag(*slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);

O lambda ficara assim (adicionar apenas a linha marcada com +):

    auto create_rgb_slider = [&](lv_obj_t** slider, int y, uint8_t val, lv_color_t color) {
        *slider = lv_slider_create(g_edit_screen);
        lv_obj_set_size(*slider, 200, 15);
        lv_obj_align(*slider, LV_ALIGN_TOP_LEFT, panel_x, y);
        lv_slider_set_range(*slider, 0, 255);
        lv_slider_set_value(*slider, val, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(*slider, color, LV_PART_KNOB);
    +   lv_obj_clear_flag(*slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        lv_obj_add_event_cb(*slider, color_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    };

Nao alterar mais nenhuma outra linha desta tarefa. Sem reformatacao de codigo existente.

Apos editar, verificar que ble_actions.cpp contem o guard BTN_TYPE_DISABLED
(linha 171: "if (cfg.type == BTN_TYPE_DISABLED) return;") e que storage.cpp
inicializa com BTN_TYPE_DISABLED por padrao (linhas 118-122). Esses ja estao presentes
no working tree — apenas confirmar visualmente, nao alterar.
  </action>
  <verify>
Grep confirma a linha adicionada:
  grep -n "SCROLL_CHAIN_VER" src/ui_settings.cpp
Resultado esperado: ao menos uma ocorrencia dentro de create_rgb_slider (linha ~213-215).

Grep confirma que ble_actions.cpp tem o guard:
  grep -n "BTN_TYPE_DISABLED" src/ble_actions.cpp
Resultado esperado: linha com "if (cfg.type == BTN_TYPE_DISABLED) return;"

Grep confirma default em storage.cpp:
  grep -n "BTN_TYPE_DISABLED" src/storage.cpp
Resultado esperado: linha com "g_configs[i].type  = BTN_TYPE_DISABLED;"
  </verify>
  <done>
Os tres sliders RGB (g_slider_r, g_slider_g, g_slider_b) tem SCROLL_CHAIN_VER limpo.
O guard BTN_TYPE_DISABLED esta presente em ble_actions.cpp.
O default de storage esta correto.
R1.3 esta tecnicamente completo e pronto para commit.
  </done>
</task>

<task type="auto" tdd="false">
  <name>Tarefa 2: Commit atomico de R1.3 (BTN_TYPE_DISABLED completo)</name>
  <files>src/ui_settings.cpp, src/ble_actions.cpp, src/storage.cpp, src/webserver_html.h, src/l10n.h, src/l10n.cpp, src/constants.h</files>
  <action>
Fazer stage de todos os arquivos relacionados a BTN_TYPE_DISABLED e commitar com a
mensagem padrao definida na RESEARCH.md:

    git add src/ui_settings.cpp src/ble_actions.cpp src/storage.cpp src/webserver_html.h src/l10n.h src/l10n.cpp src/constants.h
    git commit -m "feat(ui): BTN_TYPE_DISABLED grayed out, BLE guard, web dashboard support"

Nao incluir nenhum arquivo de outra tarefa neste commit.
Antes de commitar, confirmar com "git diff --cached" que apenas mudancas de R1.3 estao
staged: BTN_TYPE_DISABLED guard em ble_actions, default em storage, "Disabled" option em
webserver_html, type_disabled string em l10n, SCROLL_CHAIN_VER dos sliders em
ui_settings.
  </action>
  <verify>
    git log --oneline -1
Resultado esperado: commit com mensagem "feat(ui): BTN_TYPE_DISABLED grayed out, BLE guard, web dashboard support"

    git show --stat HEAD
Resultado esperado: lista dos arquivos R1.3 sem nenhum arquivo de outra categoria.
  </verify>
  <done>
Commit R1.3 existe no historico git com exatamente os arquivos de BTN_TYPE_DISABLED.
Nenhum arquivo de R1.1 ou R1.2 foi incluido neste commit.
  </done>
</task>

<!-- ================================================================
     GRUPO 2 — R1.2: Leaks de memoria LVGL (verificar + commitar)
     Estado: back_to_main_cb, save_edit_cb e save_wifi_cb ja usam
     lv_obj_del_async + nullptr imediato. Verificar e commitar.
     ================================================================ -->

<task type="auto" tdd="false">
  <name>Tarefa 3: Verificar corretude das delecoes de tela em ui_settings.cpp (R1.2)</name>
  <files>src/ui_settings.cpp</files>
  <action>
Verificar que os tres pontos de saida de telas secundarias estao corretos.
Nao ha codigo novo a escrever — esta tarefa e de verificacao estrutural.

Verificar back_to_main_cb (linhas 450-462):
  - lv_scr_load(g_main_screen) chamado ANTES de qualquer lv_obj_del_async
  - g_edit_screen, g_wifi_screen e g_button_list_screen: cada um tem "if (ptr) { lv_obj_del_async(ptr); ptr = nullptr; }"
  - Zeragem do ponteiro ocorre imediatamente na mesma linha, nao depois de async terminar

Verificar save_edit_cb (linhas 490-542):
  - lv_scr_load(g_main_screen) chamado antes das delecoes
  - g_edit_screen e g_button_list_screen: lv_obj_del_async + nullptr imediato
  - create_main_ui() chamado DEPOIS das delecoes

Verificar save_wifi_cb (linhas 464-475):
  - lv_scr_load(g_main_screen) chamado antes das delecoes
  - g_wifi_screen: lv_obj_del_async + nullptr imediato

Verificar back_to_settings_cb (linhas 414-417):
  - g_button_list_screen: lv_obj_del_async + nullptr imediato
  - lv_scr_load(g_settings_screen) chamado para retornar a settings

Se qualquer uma dessas verificacoes falhar (ex.: lv_obj_del sincrono dentro de callback,
ou nullptr nao atribuido imediatamente), corrigir neste momento seguindo o padrao:
    if (g_xxx_screen) { lv_obj_del_async(g_xxx_screen); g_xxx_screen = nullptr; }
  </action>
  <verify>
Grep confirma que lv_obj_del (sincrono) nao aparece dentro de callbacks:
  grep -n "lv_obj_del(" src/ui_settings.cpp | grep -v "lv_obj_del_async" | grep -v "lv_obj_del(g_edit_screen)" | grep -v "lv_obj_del(g_wifi_screen)" | grep -v "lv_obj_del(g_button_list"

Nota: lv_obj_del sincrono e aceitavel FORA de callbacks (ex: create_edit_ui linha 123,
create_wifi_ui linha 254, create_settings_ui linha 65). O que nao pode existir e
lv_obj_del sincrono DENTRO de uma funcao _cb.

Grep confirma del_async + nullptr nos tres callbacks de saida:
  grep -A2 "lv_obj_del_async" src/ui_settings.cpp
Resultado esperado: cada lv_obj_del_async seguido de "= nullptr" na mesma ou proxima linha.
  </verify>
  <done>
Todos os tres pontos de saida (back_to_main_cb, save_edit_cb, save_wifi_cb) usam
lv_obj_del_async + nullptr imediato. Nenhum lv_obj_del sincrono dentro de callback.
R1.2 esta correto e pronto para commit.
  </done>
</task>

<task type="auto" tdd="false">
  <name>Tarefa 4: Commit atomico de R1.2 (sem leak de memoria LVGL)</name>
  <files>src/ui_settings.cpp</files>
  <action>
Se a Tarefa 3 nao produziu nenhuma alteracao (verificacao passou sem correcoes):
  Confirmar com "git diff src/ui_settings.cpp" que nao ha diff novo alem do da Tarefa 1
  (que ja foi commitado). Se nao houver diff, registrar: "R1.2 ja estava correto no working
  tree — sem commit separado necessario para este requisito."

Se a Tarefa 3 gerou alguma correcao em ui_settings.cpp:
  git add src/ui_settings.cpp
  git commit -m "fix(ui): delete g_edit_screen/g_wifi_screen/g_button_list_screen on exit"

Em ambos os casos, documentar o resultado para referencia no SUMMARY.
  </action>
  <verify>
    git log --oneline -5
Resultado esperado: historico reflete o estado correto (com ou sem commit de R1.2
dependendo do resultado da Tarefa 3).
  </verify>
  <done>
R1.2 esta no historico git — seja como commit proprio (se havia correcao) ou documentado
como "ja estava correto" no SUMMARY desta fase.
  </done>
</task>

<!-- ================================================================
     GRUPO 3 — R1.1: Settings usavel + fix do sleep overlay (bug critico)
     Estado: 8 itens no settings list e button list em tela separada
     ja estao no working tree. Dois fixes pendentes:
     (A) enter_sleep() precisa de guarda + overlay como filho de g_main_screen
     (B) callbacks de selecao diretos precisam zerar g_sleep_overlay
     (C) back_to_main_cb precisa detectar e destruir overlay orfao
     ================================================================ -->

<task type="auto" tdd="false">
  <name>Tarefa 5: Corrigir sleep overlay dangling pointer em streamdeck.cpp (R1.1)</name>
  <files>src/streamdeck.cpp</files>
  <action>
Implementar a Opcao B da RESEARCH.md (Q3): enter_sleep() so roda na g_main_screen
e o overlay e sempre filho de g_main_screen.

ALTERACAO em enter_sleep() (linhas 24-37 de streamdeck.cpp):
Adicionar guarda no inicio da funcao:

    static void enter_sleep() {
        // Only enter sleep from the main screen. If a sub-screen (settings, edit,
        // wifi) is active, skip this cycle — the timer will retry next loop.
        if (lv_scr_act() != g_main_screen) return;

        pt_set_backlight(0, false);

        // Always parent the overlay to g_main_screen (never lv_scr_act()) so
        // that navigating away and back cannot orphan the pointer.
        g_sleep_overlay = lv_obj_create(g_main_screen);
        // ... restante igual, sem mudancas nas linhas de configuracao do overlay
    }

APENAS duas mudancas no corpo de enter_sleep:
1. Adicionar "if (lv_scr_act() != g_main_screen) return;" como primeira instrucao do corpo
2. Alterar "lv_obj_create(lv_scr_act())" para "lv_obj_create(g_main_screen)"

Nao alterar sleep_overlay_cb nem check_sleep_status.
  </action>
  <verify>
Grep confirma o guarda de tela:
  grep -n "lv_scr_act.*g_main_screen" src/streamdeck.cpp
Resultado esperado: linha com "if (lv_scr_act() != g_main_screen) return;"

Grep confirma overlay como filho de g_main_screen:
  grep -n "lv_obj_create(g_main_screen)" src/streamdeck.cpp
Resultado esperado: linha dentro de enter_sleep.

Grep confirma que lv_obj_create(lv_scr_act()) foi removido de enter_sleep:
  grep -n "lv_obj_create(lv_scr_act())" src/streamdeck.cpp
Resultado esperado: zero resultados (ou nenhuma ocorrencia dentro de enter_sleep).
  </verify>
  <done>
enter_sleep() retorna imediatamente se a tela ativa nao e g_main_screen.
O overlay e sempre filho de g_main_screen.
O cenario de overlay orfao esta eliminado pela raiz.
  </done>
</task>

<task type="auto" tdd="false">
  <name>Tarefa 6: Zerar g_sleep_overlay nos callbacks de selecao e em back_to_main_cb (R1.1)</name>
  <files>src/ui_settings.cpp</files>
  <action>
Como enter_sleep() agora cria o overlay sempre como filho de g_main_screen (Tarefa 5),
lv_obj_clean() dentro de create_main_ui() ira destruir o overlay como filho. Sem zeragem
do ponteiro antes, g_sleep_overlay fica dangling. Tres pontos precisam de correcao:

PONTO A — grid_selected (linha ~314):
Adicionar zeragem de g_sleep_overlay imediatamente antes de create_main_ui():

    static void grid_selected(const char* txt) {
        // ...parsing da grid...
        save_settings();
        lv_scr_load(g_main_screen);
        if (g_sleep_overlay) { g_sleep_overlay = nullptr; }  // ADICIONAR esta linha
        create_main_ui();
    }

PONTO B — os_selected (linha ~324):
Idem, adicionar antes de create_main_ui():

    static void os_selected(const char* txt) {
        // ...
        save_settings(false);
        load_settings();
        lv_scr_load(g_main_screen);
        if (g_sleep_overlay) { g_sleep_overlay = nullptr; }  // ADICIONAR esta linha
        create_main_ui();
    }

PONTO C — pages_selected (linha ~371):
Idem:

    static void pages_selected(const char* txt) {
        // ...
        save_settings(false);
        lv_scr_load(g_main_screen);
        if (g_sleep_overlay) { g_sleep_overlay = nullptr; }  // ADICIONAR esta linha
        create_main_ui();
    }

PONTO D — back_to_main_cb (linha ~450):
Adicionar deteccao de overlay orfao ANTES das chamadas lv_obj_del_async.
O overlay orfao e aquele cujo parent nao e g_main_screen (cenario de race antes da
Opcao B estar ativa ou durante transicao). Mesmo com a Opcao B, adicionar esta defesa
por consistencia:

    static void back_to_main_cb(lv_event_t* e) {
        g_editing_bg = false;

        // If a sleep overlay somehow survived on a sub-screen (e.g. race before
        // Opt-B guard), clean it up before destroying the parent screen.
        if (g_sleep_overlay && lv_obj_get_parent(g_sleep_overlay) != g_main_screen) {
            lv_obj_del(g_sleep_overlay);  // sync OK: not deleting self
            g_sleep_overlay = nullptr;
            // Do NOT restore backlight here — user is awake (interacted with UI).
        }

        lv_scr_load(g_main_screen);
        if (g_edit_screen)        { lv_obj_del_async(g_edit_screen);        g_edit_screen        = nullptr; }
        if (g_wifi_screen)        { lv_obj_del_async(g_wifi_screen);        g_wifi_screen        = nullptr; }
        if (g_button_list_screen) { lv_obj_del_async(g_button_list_screen); g_button_list_screen = nullptr; }
        if (g_settings_needs_rebuild) {
            if (g_sleep_overlay) { g_sleep_overlay = nullptr; }  // ADICIONAR: lv_obj_clean vai destruir como filho
            create_main_ui();
            g_settings_needs_rebuild = false;
        } else {
            refresh_main_ui();
        }
    }

Nao alterar lang_selected — ela ja possui g_settings_needs_rebuild = true e retorna
para g_main_screen via back_to_main_cb indiretamente (nao chama create_main_ui diretamente).

Verificar ao final que g_sleep_overlay e declarado como extern em algum header acessivel
por ui_settings.cpp. Se nao estiver visivel (erro de compilacao), expor via streamdeck.h
ou criar um header de globals de UI. Consultar a declaracao atual: em streamdeck.cpp e
"static lv_obj_t* g_sleep_overlay = nullptr;" — static significa que nao e acessivel
fora do TU. Para tornar acessivel em ui_settings.cpp, ha duas opcoes:
  - Remover "static" e declarar extern em streamdeck.h
  - Adicionar uma funcao inline "void clear_sleep_overlay_if_orphan()" em streamdeck.h/cpp

Opcao preferida: tornar g_sleep_overlay nao-static em streamdeck.cpp e adicionar
"extern lv_obj_t* g_sleep_overlay;" em streamdeck.h. Verificar se streamdeck.h ja tem
outros externs; se sim, seguir o mesmo padrao.
  </action>
  <verify>
Grep confirma as tres zeragens nos callbacks de selecao:
  grep -n "g_sleep_overlay = nullptr" src/ui_settings.cpp
Resultado esperado: ao menos 4 ocorrencias (3 callbacks + back_to_main_cb).

Grep confirma a checagem de parent em back_to_main_cb:
  grep -n "lv_obj_get_parent" src/ui_settings.cpp
Resultado esperado: linha com "lv_obj_get_parent(g_sleep_overlay) != g_main_screen"

Grep confirma que g_sleep_overlay esta acessivel via extern (se alterado):
  grep -n "extern.*g_sleep_overlay" src/streamdeck.h
Resultado esperado: linha com extern lv_obj_t* g_sleep_overlay.
  </verify>
  <done>
Os quatro pontos de call para create_main_ui() ou telas secundarias que podem carregar
g_sleep_overlay dangling foram corrigidos. O ponteiro e zerado antes de qualquer
lv_obj_clean. A defesa em back_to_main_cb elimina overlays orfaos em qualquer estado.
  </done>
</task>

<task type="auto" tdd="false">
  <name>Tarefa 7: Verificar settings list com 8 itens e button list navegavel (R1.1)</name>
  <files>src/ui_settings.cpp</files>
  <action>
Esta e uma verificacao estrutural — nao ha codigo novo a escrever, apenas confirmar que
as mudancas ja no working tree estao corretas.

VERIFICACAO 1 — Settings list com exatamente 8 itens (create_settings_ui, linhas 77-104):
Contar as chamadas lv_list_add_btn(list, ...) dentro da funcao create_settings_ui.
Resultado esperado: exatamente 8 chamadas (bg, grid, os, wifi, lang, pages, sleep, btns_item).
Se houver mais ou menos, registrar no SUMMARY.

VERIFICACAO 2 — Settings screen sem scroll:
Confirmar lv_obj_clear_flag(g_settings_screen, LV_OBJ_FLAG_SCROLLABLE) na linha ~69.
Confirmar lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN_VER) na linha ~80.

VERIFICACAO 3 — Button list em tela separada:
Confirmar que create_button_list_ui() (linha ~379) cria g_button_list_screen como
lv_obj_create(NULL) — uma tela raiz separada, nao um objeto filho de g_settings_screen.
Confirmar que o botao "Back" desta tela chama back_to_settings_cb (nao back_to_main_cb).
Confirmar que back_to_settings_cb faz lv_obj_del_async(g_button_list_screen) + nullptr
E lv_scr_load(g_settings_screen).

VERIFICACAO 4 — Contagem de itens visiveis sem scroll:
Com height=360 e itens de lista tipicamente 45px cada, 8 itens = 360px — cabem exatamente.
Confirmar "lv_obj_set_size(list, 600, 360)" na linha ~78.
  </action>
  <verify>
Grep conta itens na settings list:
  grep -c "lv_list_add_btn(list," src/ui_settings.cpp
Resultado esperado: 8 (ou valor que contemple exatamente os 8 itens; se a grep pegar itens
de outras funcoes, refinar o grep para o escopo de create_settings_ui).

Grep confirma tela separada para button list:
  grep -n "lv_obj_create(NULL)" src/ui_settings.cpp
Resultado esperado: ao menos uma ocorrencia para g_button_list_screen.

Grep confirma back navigation da button list:
  grep -n "back_to_settings_cb" src/ui_settings.cpp
Resultado esperado: declaracao e uso no botao Back de create_button_list_ui.
  </verify>
  <done>
Settings list tem exatamente 8 itens e scroll bloqueado.
Button list esta em g_button_list_screen (tela LVGL independente).
Navegacao de volta da button list retorna a settings (nao a main diretamente).
R1.1 confirmado estruturalmente.
  </done>
</task>

<task type="auto" tdd="false">
  <name>Tarefa 8: Commit atomico de R1.1 + fix do sleep overlay</name>
  <files>src/ui_settings.cpp, src/streamdeck.cpp, src/streamdeck.h, src/ui_main.cpp</files>
  <action>
Fazer stage de todos os arquivos modificados pelas Tarefas 5, 6 e 7 e commitar:

    git add src/streamdeck.cpp src/streamdeck.h src/ui_settings.cpp
    # Adicionar src/ui_main.cpp apenas se houver mudancas (verificar com git status)
    git commit -m "fix(ui): settings list 8 items, button list on separate screen, sleep overlay safe"

O commit message cobre os tres pontos de R1.1:
  - 8 itens no settings (ja no working tree)
  - button list em tela separada (ja no working tree)
  - sleep overlay nao fica orfao (Opcao B + zeragem nos callbacks)
  </action>
  <verify>
    git log --oneline -3
Resultado esperado: os tres commits da fase presentes (R1.3, R1.2 se houve, R1.1).

    git show --stat HEAD
Resultado esperado: streamdeck.cpp, streamdeck.h e ui_settings.cpp no ultimo commit.

Compilacao final (se PlatformIO disponivel):
    pio run
Resultado esperado: BUILD SUCCESS sem erros ou warnings novos.
  </verify>
  <done>
Tres commits atomicos existem: R1.3, R1.2 (ou documentado como pre-existente), R1.1.
O codigo compila sem erros.
Todos os bugs criticos da RESEARCH.md estao enderecos.
  </done>
</task>

<!-- ================================================================
     GRUPO 4 — Verificacao Final no Device
     ================================================================ -->

<task type="checkpoint:human-verify" gate="blocking">
  <what-built>
Todos os fixes de estabilidade da Fase 1 foram implementados e commitados:
- R1.3: BTN_TYPE_DISABLED grayed out, sem acao BLE, opcao no web dashboard
- R1.2: g_edit_screen, g_wifi_screen, g_button_list_screen destruidos corretamente ao sair
- R1.1: 8 itens no settings sem scroll; button list em tela separada; sleep overlay sem dangling pointer
  </what-built>
  <how-to-verify>
Flash o firmware no PandaTouch e execute o checklist abaixo:

1. BOTAO DISABLED (R1.3):
   a. Abrir Settings > Button Configuration > Button 1
   b. Alterar tipo para "Disabled", salvar
   c. Confirmar que o botao 1 aparece cinza (LV_STATE_DISABLED) na tela principal
   d. Conectar via BLE e pressionar o botao — confirmar que nenhuma tecla e enviada ao host
   e. Abrir web dashboard — confirmar que "Disabled" aparece no seletor de tipo

2. SETTINGS SEM SCROLL (R1.1):
   a. Entrar em Settings
   b. Confirmar que todos os 8 itens (Bg, Grid, OS, WiFi, Lang, Pages, Sleep, Buttons) ficam visiveis sem rolar
   c. Tentar arrastar verticalmente sobre a lista — confirmar que a lista rola mas a TELA nao rola

3. BUTTON LIST SEPARADA (R1.1):
   a. Settings > Button Configuration — confirmar que abre uma NOVA tela (nao um modal)
   b. Rolar a lista de botoes livremente (se grid 5x3: 15 itens)
   c. Selecionar um botao, editar, salvar — confirmar retorno a tela principal (nao a button list)
   d. Repetir: Settings > Button Configuration > Back — confirmar retorno a Settings sem crash

4. SEM LEAK DE MEMORIA (R1.2):
   a. Abrir monitor serial (baud 115200)
   b. Executar ciclo: Main > Settings > BgEdit > Back > Settings > WifiEdit > Back > Settings > ButtonList > Back > Main
   c. Repetir 5 vezes rapidamente
   d. Confirmar no serial: sem panic, sem stack smashing, sem restart involuntario
   e. Verificar heap com: ESP.getFreeHeap() via serial (nao deve cair mais de ~5KB por ciclo)

5. SLEEP OVERLAY (R1.1 — bug critico):
   a. Configurar Sleep para "30 sec" via Settings
   b. Ir para Settings e aguardar 30 segundos SEM tocar na tela
   c. Confirmar que o display NAO apaga (o enter_sleep deve retornar pois a tela ativa nao e g_main_screen)
   d. Pressionar Back — retornar a tela principal
   e. Aguardar 30 segundos — confirmar que o display APAGA (sleep funcionando na main)
   f. Tocar na tela — confirmar que acorda normalmente
   g. Aguardar mais 30 segundos — confirmar que dorme de novo (sem travar)
  </how-to-verify>
  <resume-signal>
Digite "aprovado" se todos os 5 cenarios passaram, ou descreva o cenario que falhou
com o comportamento observado para rastrear o problema.
  </resume-signal>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Touch input -> LVGL event | Qualquer toque pode disparar callbacks; overlays e buttons nao devem ter estado inconsistente |
| BLE host -> handle_button_action | Host pode pressionar qualquer idx; guard de range e tipo necessario |
| Web dashboard (WiFi) -> webserver | Campos do form podem ter valores invalidos; NVS keys tem limite de 15 chars |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-01-01 | Tampering | g_sleep_overlay (dangling) | mitigate | Opcao B: overlay sempre filho de g_main_screen; zeragem antes de create_main_ui em todos os call sites |
| T-01-02 | Denial | lv_obj_del_async fora de ordem | mitigate | Padrao: lv_scr_load antes de del_async; ponteiro zerado imediatamente apos agendamento |
| T-01-03 | Elevation | BTN_TYPE_DISABLED disparando BLE | mitigate | Guard "if (cfg.type == BTN_TYPE_DISABLED) return;" como primeira checagem de tipo em handle_button_action |
| T-01-04 | Denial | Heap leak por telas LVGL nao deletadas | mitigate | lv_obj_del_async + nullptr nos tres pontos de saida (back_to_main_cb, save_edit_cb, save_wifi_cb) |
| T-01-05 | Tampering | Scroll em tela de settings disparando item errado | mitigate | LV_OBJ_FLAG_SCROLL_CHAIN_VER removido da list + LV_OBJ_FLAG_SCROLLABLE removido da tela raiz |
| T-01-06 | Information | NVS keys ultrapassando 15 chars | accept | Keys atuais estao dentro do limite (maior: "num_pages" = 9 chars); nenhuma nova key adicionada nesta fase |
</threat_model>

<verification>
Verificacao geral da fase apos todos os commits:

1. Compilacao limpa:
     pio run
   Sem erros de compilacao. Warnings pre-existentes sao aceitaveis.

2. Tres commits atomicos presentes:
     git log --oneline -5
   - "feat(ui): BTN_TYPE_DISABLED grayed out, BLE guard, web dashboard support"
   - "fix(ui): delete g_edit_screen/g_wifi_screen/g_button_list_screen on exit" (se havia diff)
   - "fix(ui): settings list 8 items, button list on separate screen, sleep overlay safe"

3. Sem lv_obj_del sincrono dentro de callbacks:
     grep -n "lv_obj_del(" src/ui_settings.cpp | grep -v "_async"
   Ocorrencias esperadas: apenas nos create_* fora de callbacks (linhas 65, 123, 254, 381).

4. Overlay sempre em g_main_screen:
     grep -n "lv_obj_create(g_main_screen)" src/streamdeck.cpp
   Resultado esperado: 1 ocorrencia dentro de enter_sleep.

5. Sliders RGB com SCROLL_CHAIN_VER:
     grep -c "SCROLL_CHAIN_VER" src/ui_settings.cpp
   Resultado esperado: ao menos 1 (dentro de create_rgb_slider).
</verification>

<success_criteria>
A fase esta completa quando:

1. Botao com tipo Disabled aparece cinza na tela principal e nao envia nenhuma tecla via BLE
2. O tipo Disabled aparece no seletor do web dashboard
3. O settings menu exibe todos os 8 itens sem precisar rolar a tela
4. A lista de botoes abre em tela LVGL separada, pode ser rolada livremente e tem Back para settings
5. Navegar para settings e voltar 10 vezes nao causa reinicio ou erro de heap no serial
6. Deixar o device em Settings por mais tempo que o sleep timeout nao apaga o display
7. Voltar para a tela principal e aguardar o timeout APAGA o display; tocar ACORDA; o ciclo se repete
8. Todos os tres commits atomicos (R1.3, R1.2, R1.1) existem no historico git
9. pio run compila sem novos erros
</success_criteria>

<output>
Apos a conclusao (incluindo checkpoint de verificacao no device aprovado), criar:

.planning/phases/01-ui-stability-bug-fixes/01-SUMMARY.md

O SUMMARY deve conter:
- Decisoes tomadas (ex.: Opcao B para sleep overlay, extern para g_sleep_overlay)
- Arquivos modificados e natureza da mudanca
- Hashes dos commits atomicos
- Resultado do checklist de verificacao no device
- Qualquer desvio do plano e justificativa
</output>
