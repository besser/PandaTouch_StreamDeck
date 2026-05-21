#pragma once
#ifndef PT_CONSTANTS_H
#define PT_CONSTANTS_H

#include <stdint.h>

#define MAX_PAGES 5
#define BUTTONS_PER_PAGE 20
#define MAX_TOTAL_BUTTONS (MAX_PAGES * BUTTONS_PER_PAGE)
#define MAX_BUTTONS BUTTONS_PER_PAGE // Keep this for backward compatibility in some places or refactor
#define PANDA_VERSION "1.9.0"

enum ButtonType {
    BTN_TYPE_APP = 0,
    BTN_TYPE_MEDIA,
    BTN_TYPE_BASIC_COMBO,
    BTN_TYPE_ADV_COMBO,
    BTN_TYPE_DISABLED
};

enum TargetOS {
    OS_WINDOWS = 0,
    OS_MACOS,
    OS_LINUX
};

enum KbLang {
    LANG_US = 0,
    LANG_ES
};

#endif
