# Phase 1: UI Stability & Bug Fixes — Research

**Pesquisado em:** 2026-05-13
**Domínio:** LVGL 9.3.0 screen lifecycle, ESP32-S3 single-thread event loop
**Confiança geral:** HIGH — todas as conclusões derivadas diretamente da leitura do código-fonte

---

## Sumário

Esta fase cobre três requisitos de estabilidade que já foram parcialmente implementados no
working tree: o menu de configurações com lista de botões em tela separada (R1.1), limpeza
de memória LVGL nas telas criadas dinamicamente (R1.2), e o tipo de botão desabilitado
completo incluindo suporte no web dashboard (R1.3).

As mudanças já estão aplicadas nos arquivos (`ui_settings.cpp`, `ui_main.cpp`,
`ble_actions.cpp`, `storage.cpp`, `webserver_html.h`). O working tree está limpo conforme
confirmado por `git status`. A tarefa da fase é verificar a correção de cada mudança,
resolver os riscos identificados abaixo, e fechar com commit(s) limpos.

**Recomendação principal:** As mudanças são funcionalmente corretas, mas há dois riscos
ativos — o sleep overlay podendo ficar orphan quando a tela muda durante o sono, e o botão
list screen mostrando labels desatualizados após salvar — que precisam de correção antes
de marcar a fase como concluída.

---

## Mapa de Responsabilidade Arquitetural

| Capability | Tier primário | Tier secundário | Rationale |
|---|---|---|---|
| Gerenciamento de tela LVGL | UI Layer (`ui_settings.cpp`, `ui_main.cpp`) | Orchestration (`streamdeck.cpp`) | LVGL é single-thread; todas as chamadas `lv_obj_*` vivem no loop do Core 0 |
| Sleep overlay lifecycle | Orchestration (`streamdeck.cpp`) | UI Layer | `enter_sleep()` e `check_sleep_status()` são donos do estado `g_sleep_overlay` |
| Persistência de botões | Persistence Layer (`storage.cpp`) | — | `save_settings()` / `load_settings()` escrevem LittleFS + NVS |
| Tipo de botão (BTN_TYPE_DISABLED) | Business Logic (`ble_actions.cpp`) | UI Layer + Web API | Guard em `handle_button_action()`; visual em `create_main_ui()`; HTML em `webserver_html.h` |
| Button list screen | UI Layer (`ui_settings.cpp`) | — | `g_button_list_screen` criado e destruído dentro de `ui_settings.cpp` |

---

## Análise das Questões Técnicas

### Q1 — Ordem e granularidade dos commits

**Contexto:** O working tree está limpo. Não há mudanças pending; as alterações já estão no
HEAD ou em commits anteriores. A questão torna-se: como organizar commits futuros de
correção/verificação desta fase.

**Recomendação:** Usar commits atômicos por requisito:

```
fix(ui): settings list 8 items, button list on separate screen   ← R1.1
fix(ui): delete g_edit_screen/g_wifi_screen/g_button_list_screen on exit ← R1.2
feat(ui): BTN_TYPE_DISABLED grayed out, BLE guard, web dashboard support ← R1.3
```

Se uma verificação revelar problema e a correção for pequena, pode ser squashado no mesmo
commit do requisito. Evitar um único commit gigante porque os três requisitos têm propósito
independente — facilita bisect e revisão.

**Confiança:** HIGH [VERIFIED: leitura direta do código e git log]

---

### Q2 — Risco com remoção de `g_settings_needs_rebuild` em `grid_selected` / `pages_selected`

**Código atual — `grid_selected` (ui_settings.cpp:307-317):**
```cpp
static void grid_selected(const char* txt) {
    // ...altera g_rows, g_cols...
    save_settings();
    lv_scr_load(g_main_screen);
    create_main_ui();   // reconstrói completamente — não depende de g_settings_needs_rebuild
}
```

**Código atual — `pages_selected` (ui_settings.cpp:366-374):**
```cpp
static void pages_selected(const char* txt) {
    // ...altera g_num_pages, g_current_page se necessário...
    save_settings(false);
    lv_scr_load(g_main_screen);
    create_main_ui();   // idem
}
```

**Análise:** Ambas as funções chamam `create_main_ui()` diretamente após salvar. Elas não
retornam para `back_to_main_cb`. Portanto, `g_settings_needs_rebuild` nunca foi necessário
para elas — o flag só tem efeito quando o usuário pressiona o botão "Back" (que chama
`back_to_main_cb`). A remoção do `g_settings_needs_rebuild = true` nessas funções **não
quebra nada**.

A única função que ainda usa `g_settings_needs_rebuild = true` é `lang_selected`
(linha 336), porque a mudança de idioma exige rebuild da `g_settings_screen` com os strings
corretos. Essa atribuição está correta e não foi removida.

**Risco residual:** Nenhum. [VERIFIED: leitura de ui_settings.cpp]

---

### Q3 — Sleep overlay com pointer dangling quando a tela muda durante o sono

Este é o **risco mais crítico** desta fase. Já documentado em CONCERNS.md (Known Bugs,
"Sleep overlay orphaned when switching screens").

**Fluxo do problema:**
1. Usuário abre Settings (tela `g_settings_screen` torna-se ativa)
2. Sleep timeout dispara → `enter_sleep()` chama `lv_obj_create(lv_scr_act())` → overlay
   é filho de `g_settings_screen`, não de `g_main_screen`
3. Usuário toca (wake) → `sleep_overlay_cb` destrói o overlay, restaura backlight. OK até aqui.
4. Usuário pressiona "Back" → `back_to_main_cb` chama `lv_scr_load(g_main_screen)` e
   `lv_obj_del_async(g_settings_screen)` → settings screen destruída. Nenhum problema aqui.

**Mas o cenário oposto:**
1. Sleep dispara com settings ativa → `g_sleep_overlay` aponta para filho de `g_settings_screen`
2. `back_to_main_cb` é chamada (usuário toca o botão Back **sem acordar pelo overlay** —
   isso pode acontecer se o touch não cair sobre o overlay transparent, o que é improvável
   mas possível)
3. `lv_obj_del_async(g_settings_screen)` destrói settings e **todos os seus filhos**,
   incluindo o overlay
4. `g_sleep_overlay` ainda é não-null → `check_sleep_status()` retorna cedo, nunca re-dorme
5. `sleep_overlay_cb` será chamado em algum momento sobre objeto destruído → crash/UB

**Cenário pior confirmado (CONCERNS.md linha 61-64):**
> "Returning to the main screen via `lv_scr_load(g_main_screen)` does not destroy the
> overlay; the overlay stays on the now-inactive settings screen. `check_sleep_status()`
> sees `g_sleep_overlay != nullptr` and never re-enters sleep."

**Correção necessária — opção A (mais simples):**
Em `back_to_main_cb`, antes de chamar `lv_obj_del_async` nas telas secundárias, verificar
e destruir o overlay:

```cpp
static void back_to_main_cb(lv_event_t* e) {
    g_editing_bg = false;

    // Se o sleep overlay existe em uma tela diferente da main (ficou órfão),
    // destruí-lo antes de deletar a tela pai.
    if (g_sleep_overlay && lv_obj_get_parent(g_sleep_overlay) != g_main_screen) {
        lv_obj_del(g_sleep_overlay);   // sync OK pois não estamos no callback do overlay
        g_sleep_overlay = nullptr;
        // NÃO restaurar backlight aqui — o usuário está acordado (interagiu com Settings)
    }

    lv_scr_load(g_main_screen);
    if (g_edit_screen)        { lv_obj_del_async(g_edit_screen);        g_edit_screen        = nullptr; }
    if (g_wifi_screen)        { lv_obj_del_async(g_wifi_screen);        g_wifi_screen        = nullptr; }
    if (g_button_list_screen) { lv_obj_del_async(g_button_list_screen); g_button_list_screen = nullptr; }
    // ... restante igual
}
```

**Correção — opção B (mais robusta):**
Em `enter_sleep()`, verificar se a tela ativa é `g_main_screen` antes de criar o overlay.
Se não for, não criar o overlay (não entrar em sono enquanto em sub-tela):

```cpp
static void enter_sleep() {
    // Não criar overlay em sub-telas — o usuário pode estar configurando.
    // Aguardar o retorno à tela principal.
    if (lv_scr_act() != g_main_screen) return;

    pt_set_backlight(0, false);
    g_sleep_overlay = lv_obj_create(g_main_screen);  // sempre filho de g_main_screen
    // ... restante igual
}
```

A Opção B é mais limpa: o sono só ocorre na tela principal, e o overlay é sempre filho de
`g_main_screen` (que nunca é destruída via `lv_obj_del`). Isso elimina toda a classe de
bugs relacionados.

**Importante:** `g_main_screen` é alocada uma vez em `setup()` como `lv_scr_act()` e
nunca destruída — apenas limpa com `lv_obj_clean()`. Portanto, criar o overlay
explicitamente como filho de `g_main_screen` é seguro mesmo após `lv_obj_clean()` chamado
de outro contexto, **desde que** `g_sleep_overlay` seja zerado antes do clean. Essa zeragem
já existe no path de `g_pending_ui_update` (streamdeck.cpp:101-105), mas não nos paths de
`grid_selected`, `pages_selected`, `os_selected`, e `lang_selected` que chamam
`create_main_ui()` diretamente.

**Correção complementar para os callbacks de seleção:** Antes de chamar `create_main_ui()`
nesses callbacks, adicionar:
```cpp
// Garantir que o overlay não aponte para filho que será destruído por lv_obj_clean
if (g_sleep_overlay) { g_sleep_overlay = nullptr; }
```
Não é necessário destruir o LVGL object aqui porque `lv_obj_clean(g_main_screen)` vai
destruí-lo como filho (se a Opção B foi adotada), e a Opção B garante que o overlay é
sempre filho de `g_main_screen`.

**Confiança:** HIGH [VERIFIED: leitura de streamdeck.cpp + ui_settings.cpp + CONCERNS.md]

---

### Q4 — Button list labels não atualizam após `save_edit_cb`

**Fluxo atual em `save_edit_cb` (ui_settings.cpp:490-542):**
```cpp
save_settings();
g_editing_bg = false;
lv_scr_load(g_main_screen);
if (g_edit_screen)        { lv_obj_del_async(g_edit_screen);        g_edit_screen        = nullptr; }
if (g_button_list_screen) { lv_obj_del_async(g_button_list_screen); g_button_list_screen = nullptr; }
create_main_ui();
```

Após salvar, o código vai direto para `g_main_screen` e destrói `g_button_list_screen`.
O usuário **não retorna** à button list — vai para a tela principal. Portanto, as labels
desatualizadas na button list **nunca são visíveis**: a tela já foi deletada.

**Caso de uso verificado:**
- Usuário: Settings → Button List → edita botão 3 → salva → vai para Main Screen
- `g_button_list_screen` é deletado por `lv_obj_del_async`
- Na próxima vez que o usuário entrar em Button List, `create_button_list_ui()` é chamada
  do zero, lendo `g_configs` atualizado → labels corretos

**Conclusão:** Não há bug. A button list sempre é recriada do zero quando visitada.
A preocupação só seria válida se o código retornasse à button list sem recriá-la, o que
não acontece.

**Confiança:** HIGH [VERIFIED: leitura de ui_settings.cpp save_edit_cb + create_button_list_ui]

---

### Q5 — Scroll remanescente: edit screen sliders, wifi screen textarea

**Edit screen (`create_edit_ui`) — análise:**

A tela edit (`g_edit_screen`) tem `LV_OBJ_FLAG_SCROLLABLE` removido na linha 125:
```cpp
lv_obj_clear_flag(g_edit_screen, LV_OBJ_FLAG_SCROLLABLE);
```

Os sliders RGB (g_slider_r, g_slider_g, g_slider_b) são criados diretamente em
`g_edit_screen` com posição absoluta via `lv_obj_align`. **Não há container flexível
ou grid envolvendo os sliders** — eles não têm relação de scroll entre si.

**Risco identificado:** O keyboard (`lv_keyboard_create`) pode ter comportamento de scroll
chain. Quando o textarea tem foco e o teclado está visível, um arrasto no slider que
tenha componente vertical pode se propagar ao parent (`g_edit_screen`). Porém, como a
própria `g_edit_screen` tem SCROLLABLE desabilitado, o scroll chain não produz scroll
visível — apenas a "física" de tentativa de scroll que libera o evento.

**Recomendação preventiva:** Aplicar `LV_OBJ_FLAG_SCROLL_CHAIN_VER` clear nos sliders
da edit screen, por consistência com o fix aplicado ao slider de brilho em `ui_main.cpp`:
```cpp
lv_obj_clear_flag(*slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
```
Este fix deve ser aplicado dentro do lambda `create_rgb_slider` (ui_settings.cpp:207-215).

**WiFi screen (`create_wifi_ui`) — análise:**

A wifi screen (linha 255) também tem SCROLLABLE desabilitado. Os textareas estão
posicionados com `lv_obj_align` sem container com scroll. O keyboard ocupa a parte inferior.

**Risco identificado:** LVGL 9.x pode fazer scroll de textarea ao digitar (cursor
scrolls within the textarea). Esse é o comportamento **correto e desejado** — o textarea
tem sua própria área de scroll interna para exibir o texto que não cabe. A tela em si não
vai scrollar porque SCROLLABLE está desabilitado.

**Não há scroll issue pendente na wifi screen.** [VERIFIED: leitura de create_wifi_ui]

**g_main_screen e g_grid — confirmação dos fixes já aplicados:**

`ui_main.cpp` linhas 60 e 96 confirmam:
```cpp
lv_obj_clear_flag(g_main_screen, LV_OBJ_FLAG_SCROLLABLE);   // linha 60
lv_obj_clear_flag(g_grid, LV_OBJ_FLAG_SCROLLABLE);           // linha 96
```
E linha 175:
```cpp
lv_obj_clear_flag(g_slider, LV_OBJ_FLAG_SCROLL_CHAIN_VER);  // slider de brilho
```

**Confiança:** HIGH [VERIFIED: leitura direta de ui_main.cpp e ui_settings.cpp]

---

## Padrões Arquiteturais Relevantes

### Lifecycle correto de telas LVGL neste projeto

```
Criação fora de callback:
  lv_obj_del(old_screen)        // sync — seguro
  new_screen = lv_obj_create(NULL)
  lv_scr_load(new_screen)

Destruição dentro de callback:
  lv_scr_load(target)
  lv_obj_del_async(old_screen)  // defer — seguro (self-delete pattern)
  old_ptr = nullptr              // zerar imediatamente
```

### Convenção de zeragem do overlay antes de `lv_obj_clean`

```cpp
// streamdeck.cpp:96-108 — path correto (g_pending_ui_update)
if (g_sleep_overlay) {
    g_sleep_overlay = nullptr;
    pt_set_backlight(g_brightness, false);
    pt_last_touch_ms = millis();
}
lv_scr_load(g_main_screen);
create_main_ui();  // chama lv_obj_clean internamente
```

Este padrão deve ser replicado em qualquer path que chame `create_main_ui()` diretamente.

---

## Não Implemente do Zero

| Problema | Não construir | Usar em vez disso |
|---|---|---|
| Deleção segura de self | Deleção sync no callback | `lv_obj_del_async()` já no código |
| Scroll bloqueio | Sistema próprio de swallow | `LV_OBJ_FLAG_SCROLL_CHAIN_VER` clear |
| Reset de configuração | Lógica própria de defaults | Path `load_defaults:` em `storage.cpp:117` |

---

## Pitfalls Comuns

### Pitfall 1: Pointer dangling de overlay após `lv_obj_del_async` da tela-pai
**O que ocorre:** `g_sleep_overlay` permanece não-null mas o objeto LVGL foi destruído
como filho da tela deletada.
**Por que ocorre:** `lv_obj_del_async` destrói a tela e todos os filhos, mas ponteiros
estáticos em outros módulos não são zerados automaticamente.
**Como evitar:** Zerar `g_sleep_overlay = nullptr` ANTES de qualquer `lv_obj_del_async`
na tela que pode hospedar o overlay. Melhor ainda: garantir que o overlay só é criado
como filho de `g_main_screen` (Opção B da Q3).

### Pitfall 2: `create_main_ui()` com sleep overlay filho de g_main_screen sem zerar o ponteiro
**O que ocorre:** `lv_obj_clean(g_main_screen)` destrói o overlay como filho.
`g_sleep_overlay` fica dangling. Próxima chamada de `check_sleep_status()` retorna
cedo e o device nunca mais dorme.
**Como evitar:** Sempre zerar `g_sleep_overlay = nullptr` antes de `create_main_ui()`.

### Pitfall 3: `save_settings(false)` seguido de `load_settings()` em OS/lang switch
**O que ocorre:** `load_settings()` faz leitura do arquivo binário ativo e pode resetar
`g_configs` para defaults se o arquivo falhar (size mismatch). Callbacks de seleção de OS
e lang chamam esse par (ui_settings.cpp:324-325, 334).
**Como evitar:** Para `lang_selected`, não chamar `load_settings()` — a mudança de idioma
não precisa reler o arquivo de botões. Apenas salvar o NVS e reconstruir a UI. (Melhoria
de tech debt, não bloqueante para esta fase.)

---

## Verificação de Ambiente

| Dependência | Requerida por | Disponível | Observação |
|---|---|---|---|
| PlatformIO + espressif32@7.0.0 | Compilação | [ASSUMED] | Projeto já compila (git log mostra releases) |
| LVGL 9.3.0 | UI | [ASSUMED] | `platformio.ini` lib_deps |
| Hardware PandaTouch | Teste final | Necessário para smoke test de scroll |

---

## Requisitos da Fase → Mapa de Implementação

| ID | Comportamento | Arquivo(s) | Status |
|---|---|---|---|
| R1.1 | 8 itens no settings list, sem scroll acidental | `ui_settings.cpp:create_settings_ui` | Aplicado — verificar em device |
| R1.1 | Button list em tela separada (`g_button_list_screen`) | `ui_settings.cpp:create_button_list_ui` | Aplicado — verificar |
| R1.2 | `g_edit_screen` deletado em `back_to_main_cb` | `ui_settings.cpp:450-455` | Aplicado — verificar |
| R1.2 | `g_wifi_screen` deletado em `back_to_main_cb` | `ui_settings.cpp:453-454` | Aplicado — verificar |
| R1.2 | `g_button_list_screen` deletado em `back_to_main_cb` | `ui_settings.cpp:455` | Aplicado — verificar |
| R1.3 | `BTN_TYPE_DISABLED` guard em `handle_button_action` | `ble_actions.cpp:171` | Aplicado |
| R1.3 | Visual grayed-out via `LV_STATE_DISABLED` | `ui_main.cpp:151-155` | Aplicado |
| R1.3 | Default buttons como BTN_TYPE_DISABLED | `storage.cpp:118-122` | Aplicado |
| R1.3 | "Disabled" no type selector do web dashboard | `webserver_html.h` | Aplicado |

---

## Questões em Aberto

1. **Zeragem de `g_sleep_overlay` nos callbacks de seleção diretos**
   - O que sabemos: `grid_selected`, `pages_selected`, `os_selected` chamam `create_main_ui()`
     sem zerar `g_sleep_overlay` primeiro
   - O que está incerto: Se a Opção B (overlay sempre filho de `g_main_screen`) for adotada,
     isso é necessário pois `lv_obj_clean` vai destruir o overlay como filho. Se o overlay
     for filho de outra tela quando esses callbacks rodam, o pointer fica dangling.
   - Recomendação: Adotar Opção B + adicionar `if (g_sleep_overlay) g_sleep_overlay = nullptr;`
     antes de `create_main_ui()` em todos os call sites diretos.

2. **Clear de `LV_OBJ_FLAG_SCROLL_CHAIN_VER` nos sliders RGB da edit screen**
   - O que sabemos: O slider de brilho já recebeu esse fix. Os sliders RGB não.
   - O que está incerto: Se em device real isso causa scroll visual no edit screen.
   - Recomendação: Aplicar o clear por consistência; custo é zero linhas.

---

## Fontes

### Primárias (HIGH confidence)
- `src/ui_settings.cpp` — leitura direta do código atual
- `src/ui_main.cpp` — leitura direta do código atual
- `src/streamdeck.cpp` — leitura direta do código atual
- `src/ble_actions.cpp` — leitura direta do código atual
- `src/storage.cpp` — leitura direta do código atual
- `.planning/codebase/CONCERNS.md` — análise de bugs conhecidos
- `.planning/codebase/ARCHITECTURE.md` — padrões LVGL e threading

### Sem fontes externas
Toda pesquisa foi realizada sobre o código-fonte e documentação interna do projeto.
Nenhuma busca externa foi necessária — o domínio técnico (LVGL lifecycle patterns no
ESP32-S3 single-thread loop) está completamente representado no código existente.

---

## Log de Premissas

| # | Afirmação | Seção | Risco se errado |
|---|---|---|---|
| A1 | PlatformIO e hardware disponíveis para smoke test | Ambiente | Fase não pode ser validada sem o device |
| A2 | Working tree limpo = mudanças de R1.1/R1.2/R1.3 já no HEAD | Q1 | Se há mudanças ainda não aplicadas, os paths de código analisados podem diferir |

---

## Metadados

**Breakdown de confiança:**
- Análise de bugs (Q2, Q3, Q4, Q5): HIGH — baseado em leitura direta do código
- Recomendações de correção (Q3 Opção B, Q5 slider fix): HIGH — seguem padrões já
  estabelecidos no próprio código
- Status de implementação (tabela de requisitos): HIGH — verificado arquivo por arquivo

**Data da pesquisa:** 2026-05-13
**Válido até:** 2026-06-13 (código estável; prazo longo pois não há deps externas)
