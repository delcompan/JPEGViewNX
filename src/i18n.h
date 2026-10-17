// Minimal two-language UI strings (Chinese default, English optional).
#pragma once

#include <windows.h>

enum Lang {
    LANG_ZH = 0,
    LANG_EN = 1
};

enum StrId {
    S_MENU_OPEN,
    S_MENU_NEXT,
    S_MENU_PREV,
    S_MENU_FIT,
    S_MENU_ACTUAL,
    S_MENU_ZOOM_IN,
    S_MENU_ZOOM_OUT,
    S_TRANS_MENU,
    S_TRANS_CHECKER,
    S_TRANS_SOLID,
    S_TRANS_NONE,
    S_CELL_LABEL,
    S_MENU_SMOOTH,
    S_MENU_SHOWNAME,
    S_MENU_STATUS,
    S_MENU_PAUSE,
    S_MENU_ROT_CW,
    S_MENU_ROT_CCW,
    S_MENU_MIRROR_H,
    S_MENU_MIRROR_V,
    S_MENU_RELOAD,
    S_MENU_FULLSCREEN,
    S_MENU_INFO,
    S_MENU_HELP,
    S_MENU_ASSOC,
    S_MENU_LANGUAGE,
    S_LANG_ZH,
    S_LANG_EN,
    S_MENU_EXIT,
    S_ASSOC_ALL,
    S_ASSOC_NONE,
    S_ASSOC_SETTINGS,
    S_OVERLAY_HINT,
    S_ALPHA_MODE,
    S_OPAQUE,
    S_FRAME,
    S_PAUSED,
    S_NOIMAGE,
    S_INFO_TITLE,
    S_INFO_BODY,
    S_YES,
    S_NO,
    S_MODE_CHECKER,
    S_MODE_SOLID,
    S_MODE_NONE,
    S_HELP_TITLE,
    S_HELP_BODY,
    S_ASSOC_TITLE,
    S_ASSOC_HINT_BODY,
    S_ASSOC_DLG_TITLE,
    S_ASSOC_DLG_INTRO,
    S_ASSOC_DLG_APPLY,
    S_ASSOC_DLG_CANCEL,
    S_ASSOC_DLG_SELECT_ALL,
    S_ASSOC_DLG_SELECT_NONE,
    S_ASSOC_DLG_SETTINGS,
    S_ASSOC_DLG_NOTE,
    S_ERR_TITLE,
    S_ERR_BODY,
    S_COUNT
};

void I18nSet(Lang lang);
Lang I18nGet();
const wchar_t* T(StrId id);
