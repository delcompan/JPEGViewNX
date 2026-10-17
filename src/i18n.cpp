#include "i18n.h"

namespace {

const wchar_t* kZh[S_COUNT] = {
    /* S_MENU_OPEN */        L"打开...",
    /* S_MENU_NEXT */        L"下一张",
    /* S_MENU_PREV */        L"上一张",
    /* S_MENU_FIT */         L"适应窗口",
    /* S_MENU_ACTUAL */      L"原始大小",
    /* S_MENU_ZOOM_IN */     L"放大",
    /* S_MENU_ZOOM_OUT */    L"缩小",
    /* S_TRANS_MENU */       L"透明背景",
    /* S_TRANS_CHECKER */    L"棋盘格",
    /* S_TRANS_SOLID */      L"纯色",
    /* S_TRANS_NONE */       L"窗口背景",
    /* S_CELL_LABEL */       L"格子大小 %d px",
    /* S_MENU_SMOOTH */      L"平滑重采样",
    /* S_MENU_SHOWNAME */    L"显示文件名",
    /* S_MENU_STATUS */      L"显示状态信息",
    /* S_MENU_PAUSE */       L"暂停/继续动画",
    /* S_MENU_ROT_CW */      L"顺时针旋转 90°",
    /* S_MENU_ROT_CCW */     L"逆时针旋转 90°",
    /* S_MENU_MIRROR_H */    L"水平镜像",
    /* S_MENU_MIRROR_V */    L"垂直镜像",
    /* S_MENU_RELOAD */      L"重新加载",
    /* S_MENU_FULLSCREEN */  L"全屏",
    /* S_MENU_INFO */        L"图片信息",
    /* S_MENU_HELP */        L"帮助",
    /* S_MENU_ASSOC */       L"文件关联...",
    /* S_MENU_LANGUAGE */    L"语言 / Language",
    /* S_LANG_ZH */          L"中文",
    /* S_LANG_EN */          L"English",
    /* S_MENU_EXIT */        L"退出",
    /* S_ASSOC_ALL */        L"关联全部格式",
    /* S_ASSOC_NONE */       L"取消全部关联",
    /* S_ASSOC_SETTINGS */   L"Windows 默认应用...",
    /* S_OVERLAY_HINT */     L"把图片拖到这里，或按 Ctrl+O 打开",
    /* S_ALPHA_MODE */       L"alpha / %ls",
    /* S_OPAQUE */           L"不透明",
    /* S_FRAME */            L"第 %d/%d 帧",
    /* S_PAUSED */           L"(已暂停)",
    /* S_NOIMAGE */          L"(无图片)",
    /* S_INFO_TITLE */       L"图片信息",
    /* S_INFO_BODY */        L"文件:\n  %ls\n\n尺寸: %d x %d\n格式: %ls\nAlpha 通道: %ls\n\n"
                             L"缩放: %d%%\n透明背景: %ls\n棋盘格: %d px\n当前: %d / %d\n文件夹: %ls",
    /* S_YES */              L"是",
    /* S_NO */               L"否",
    /* S_MODE_CHECKER */     L"棋盘格",
    /* S_MODE_SOLID */       L"纯色",
    /* S_MODE_NONE */        L"无",
    /* S_HELP_TITLE */       L"帮助",
    /* S_HELP_BODY */
        L"JPEGViewNX 键盘与鼠标\n"
        L"\n"
        L"Ctrl+O            打开文件\n"
        L"PgDn / →          下一张\n"
        L"PgUp / ←          上一张\n"
        L"Home / End        第一张 / 最后一张\n"
        L"Enter             适应窗口\n"
        L"Space             适应窗口 ↔ 100%\n"
        L"+ / -             放大 / 缩小\n"
        L"滚轮              缩放（以光标为中心）\n"
        L"左键拖动          平移\n"
        L"Shift+方向键      平移\n"
        L"F11 / 双击        全屏\n"
        L"R / Shift+R       顺时针 / 逆时针旋转 90°\n"
        L"H / V             水平 / 垂直镜像\n"
        L"F3                平滑重采样开关\n"
        L"F5                重新加载\n"
        L"F2                图片信息\n"
        L"I                 显示/隐藏文件名\n"
        L"T                 显示/隐藏状态信息\n"
        L"P                 暂停/继续动画\n"
        L"\n"
        L"透明背景（带 alpha 的图片）：\n"
        L"  C               循环 棋盘格 → 纯色 → 无\n"
        L"  Ctrl+1/2/3      直接选择 棋盘格 / 纯色 / 无\n"
        L"  [ / ]           棋盘格变小 / 变大\n"
        L"\n"
        L"右键菜单：\n"
        L"  文件关联...     弹窗勾选希望本程序打开的格式（可多选）。\n"
        L"                  注册写入 HKCU，无需管理员；Windows 可能要求再确认一次默认程序。\n"
        L"  语言            中文 / English\n"
        L"\n"
        L"Esc 退出全屏，再按退出程序。\n",
    /* S_ASSOC_TITLE */      L"文件关联",
    /* S_ASSOC_HINT_BODY */
        L"已更新 %d 个格式的关联。\n\n"
        L"是否现在打开 Windows 的默认应用设置，把 JPEGViewNX 设为默认？\n"
        L"Windows 要求你手动确认一次，之后双击图片就会直接打开。",
    /* S_ASSOC_DLG_TITLE */  L"文件关联 - JPEGViewNX",
    /* S_ASSOC_DLG_INTRO */  L"勾选希望由 JPEGViewNX 打开的格式（可多选）：",
    /* S_ASSOC_DLG_APPLY */  L"应用",
    /* S_ASSOC_DLG_CANCEL */ L"取消",
    /* S_ASSOC_DLG_SELECT_ALL */   L"全选",
    /* S_ASSOC_DLG_SELECT_NONE */  L"全不选",
    /* S_ASSOC_DLG_SETTINGS */     L"Windows 默认应用...",
    /* S_ASSOC_DLG_NOTE */
        L"提示：勾选表示注册为“打开方式”。Windows 10/11 不允许程序静默抢占默认程序，\n"
        L"需要在系统设置里确认一次（“设为默认”），之后双击图片即可直接打开。",
    /* S_ERR_TITLE */        L"无法显示",
    /* S_ERR_BODY */         L"无法显示：\n  %ls\n\n%ls",
};

const wchar_t* kEn[S_COUNT] = {
    /* S_MENU_OPEN */        L"Open...",
    /* S_MENU_NEXT */        L"Next image",
    /* S_MENU_PREV */        L"Previous image",
    /* S_MENU_FIT */         L"Fit to window",
    /* S_MENU_ACTUAL */      L"Actual size",
    /* S_MENU_ZOOM_IN */     L"Zoom in",
    /* S_MENU_ZOOM_OUT */    L"Zoom out",
    /* S_TRANS_MENU */       L"Transparency background",
    /* S_TRANS_CHECKER */    L"Checkerboard",
    /* S_TRANS_SOLID */      L"Solid colour",
    /* S_TRANS_NONE */       L"Window background",
    /* S_CELL_LABEL */       L"Cell size %d px",
    /* S_MENU_SMOOTH */      L"Smooth resampling",
    /* S_MENU_SHOWNAME */    L"Show file name",
    /* S_MENU_STATUS */      L"Show status line",
    /* S_MENU_PAUSE */       L"Pause animation",
    /* S_MENU_ROT_CW */      L"Rotate 90 CW",
    /* S_MENU_ROT_CCW */     L"Rotate 90 CCW",
    /* S_MENU_MIRROR_H */    L"Mirror horizontally",
    /* S_MENU_MIRROR_V */    L"Mirror vertically",
    /* S_MENU_RELOAD */      L"Reload",
    /* S_MENU_FULLSCREEN */  L"Full screen",
    /* S_MENU_INFO */        L"Image information",
    /* S_MENU_HELP */        L"Help",
    /* S_MENU_ASSOC */       L"File associations...",
    /* S_MENU_LANGUAGE */    L"Language / 语言",
    /* S_LANG_ZH */          L"中文",
    /* S_LANG_EN */          L"English",
    /* S_MENU_EXIT */        L"Exit",
    /* S_ASSOC_ALL */        L"Associate all formats",
    /* S_ASSOC_NONE */       L"Remove all associations",
    /* S_ASSOC_SETTINGS */   L"Windows default apps...",
    /* S_OVERLAY_HINT */     L"Drop an image here, or press Ctrl+O",
    /* S_ALPHA_MODE */       L"alpha / %ls",
    /* S_OPAQUE */           L"opaque",
    /* S_FRAME */            L"frame %d/%d",
    /* S_PAUSED */           L"(paused)",
    /* S_NOIMAGE */          L"(no image)",
    /* S_INFO_TITLE */       L"Image information",
    /* S_INFO_BODY */        L"File:\n  %ls\n\nSize: %d x %d\nFormat: %ls\nAlpha channel: %ls\n\n"
                             L"Zoom: %d%%\nTransparency: %ls\nChecker cell: %d px\nImage: %d / %d\nFolder: %ls",
    /* S_YES */              L"yes",
    /* S_NO */               L"no",
    /* S_MODE_CHECKER */     L"checkerboard",
    /* S_MODE_SOLID */       L"solid",
    /* S_MODE_NONE */        L"none",
    /* S_HELP_TITLE */       L"Help",
    /* S_HELP_BODY */
        L"JPEGViewNX keyboard and mouse\n"
        L"\n"
        L"Ctrl+O            open file\n"
        L"PgDn / Right      next image\n"
        L"PgUp / Left       previous image\n"
        L"Home / End        first / last image\n"
        L"Enter             fit to window\n"
        L"Space             toggle fit <-> 100%\n"
        L"+ / -             zoom in / out\n"
        L"Wheel             zoom around the cursor\n"
        L"Left drag         pan\n"
        L"Shift+Arrows      pan\n"
        L"F11 / dbl-click   full screen\n"
        L"R / Shift+R       rotate 90 CW / CCW\n"
        L"H / V             mirror horizontally / vertically\n"
        L"F3                toggle smooth resampling\n"
        L"F5                reload\n"
        L"F2                image information\n"
        L"I                 toggle file name overlay\n"
        L"T                 toggle status line\n"
        L"P                 pause / resume animation\n"
        L"\n"
        L"Transparency background (alpha images):\n"
        L"  C               cycle checkerboard -> solid -> none\n"
        L"  Ctrl+1/2/3      checkerboard / solid / none\n"
        L"  [ / ]           smaller / larger checker cell\n"
        L"\n"
        L"Right-click menu:\n"
        L"  File associations...  dialog with checkboxes for every supported format\n"
        L"                        (per-user HKCU, no admin; Windows may ask once to\n"
        L"                        confirm the default app).\n"
        L"  Language              中文 / English\n"
        L"\n"
        L"Esc leaves full screen, then quits.\n",
    /* S_ASSOC_TITLE */      L"File associations",
    /* S_ASSOC_HINT_BODY */
        L"Updated associations for %d format(s).\n\n"
        L"Open the Windows Default Apps page now to set JPEGViewNX as the default?\n"
        L"Windows asks you to confirm once; after that, double-click opens images directly.",
    /* S_ASSOC_DLG_TITLE */  L"File associations - JPEGViewNX",
    /* S_ASSOC_DLG_INTRO */  L"Tick the formats that JPEGViewNX should open:",
    /* S_ASSOC_DLG_APPLY */  L"Apply",
    /* S_ASSOC_DLG_CANCEL */ L"Cancel",
    /* S_ASSOC_DLG_SELECT_ALL */   L"Select all",
    /* S_ASSOC_DLG_SELECT_NONE */  L"Select none",
    /* S_ASSOC_DLG_SETTINGS */     L"Windows default apps...",
    /* S_ASSOC_DLG_NOTE */
        L"Note: ticking registers the app under \"Open with\". Windows 10/11 does not\n"
        L"allow an app to silently take over defaults - confirm once in Settings\n"
        L"(\"Set default\"), after which images open directly.",
    /* S_ERR_TITLE */        L"Cannot display",
    /* S_ERR_BODY */         L"Cannot display:\n  %ls\n\n%ls",
};

static_assert(sizeof(kZh) / sizeof(kZh[0]) == S_COUNT, "zh table size");
static_assert(sizeof(kEn) / sizeof(kEn[0]) == S_COUNT, "en table size");

Lang g_lang = LANG_ZH;

} // namespace

void I18nSet(Lang lang) {
    g_lang = (lang == LANG_EN) ? LANG_EN : LANG_ZH;
}

Lang I18nGet() { return g_lang; }

const wchar_t* T(StrId id) {
    if (id < 0 || id >= S_COUNT) return L"";
    return (g_lang == LANG_EN) ? kEn[id] : kZh[id];
}
