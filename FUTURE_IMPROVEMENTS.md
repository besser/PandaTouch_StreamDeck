# PandaTouch StreamDeck - Future Improvements

This file tracks planned features and architectural enhancements for future development.

## 📋 Planned Features

### 1. Múltiplas Páginas (Perfis) - [DONE]
- Allow users to have more than one grid of buttons.
- Support navigation between pages (swipe or dedicated buttons).
- Target: At least 5 pages (100 buttons total).
- Implemented: configurable 1–5 pages via Settings screen; pagination controls hidden when only 1 page is set.

### 2. Ícones Dinâmicos
- Enable custom icon uploads via the Web Dashboard.
- Efficient storage and rendering of small bitmaps on LVGL.

### 3. Sensor de Proximidade / Sleep Mode - [DONE]
- Auto-dim or turn off display after inactivity.
- "Wake on touch" functionality.
- Implemented: configurable timeout (Disabled / 30s / 1min / 2min / 5min / 10min) via Settings screen; uses lv_display_get_inactive_time() for zero-overhead detection; first touch restores brightness automatically.

### 4. Macros de Texto
- Type whole strings of text (useful for terminal commands or boilerplate code).

### 5. Integração com Home Assistant
- Send HTTP/MQTT commands directly from the device.
- Act as a hybrid HID and IoT controller.

### 6. Atualização do LVGL (v9.3 -> v9.5) - [PLANNED]
- **Análise de Viabilidade**: Altamente viável e recomendado.
- **Benefícios**:
    - **Native Software Blur**: Melhoraria a estética de menus e overlays.
    - **Property Interfaces**: Simplifica a vinculação de dados nos widgets (código mais limpo).
    - **Performance**: Melhorias no motor de renderização e suporte a drivers unificados.
- **Impacto**: Baixo. Como o projeto não utiliza Wayland ou Fragments (módulos removidos/depreciados na v9.5), a migração deve ser suave.
- **Ação**: Atualizar `lib_deps` no `platformio.ini` e revisar `lv_conf.h` para novos parâmetros de performance.
