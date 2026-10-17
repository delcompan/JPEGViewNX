#include "assocdlg.h"

#include "i18n.h"
#include "shellassoc.h"

#include <vector>

namespace {

const wchar_t* kDlgClass = L"JPEGViewNX.AssocDialog";

enum : int {
    kIdApply = 1,
    kIdCancel = 2,
    kIdSelectAll = 3,
    kIdSelectNone = 4,
    kIdSettings = 5,
    kIdFirstBox = 1000
};

struct DlgState {
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    HFONT font = nullptr;
    bool ownFont = false;
    bool running = false;
    bool changed = false;
    int changedCount = 0;
    bool anyEnabled = false;
    std::vector<HWND> boxes;

    int width = 0;
    int height = 0;
};

UINT DpiOf(HWND hwnd) {
    using Fn = UINT(WINAPI*)(HWND);
    static Fn fn = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn) {
        const UINT dpi = fn(hwnd ? hwnd : GetDesktopWindow());
        if (dpi) return dpi;
    }
    return 96;
}

int Scaled(int value, UINT dpi) {
    return (int)((value * (double)dpi / 96.0) + 0.5);
}

void CloseDialog(DlgState& st) {
    st.running = false;
    if (st.hwnd) DestroyWindow(st.hwnd);
}

void ApplyAndClose(DlgState& st) {
    const size_t n = AssocFamilyCount();
    for (size_t i = 0; i < n && i < st.boxes.size(); ++i) {
        const bool want = SendMessageW(st.boxes[i], BM_GETCHECK, 0, 0) == BST_CHECKED;
        const bool have = AssocFamilyActive(i);
        if (want == have) continue;
        AssocSetFamily(i, want);
        st.changed = true;
        ++st.changedCount;
        if (want) st.anyEnabled = true;
    }
    if (st.changed) AssocNotifyShell();
    CloseDialog(st);
}

void SetAllBoxes(DlgState& st, bool checked) {
    for (HWND h : st.boxes) {
        SendMessageW(h, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            st = reinterpret_cast<DlgState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            st->hwnd = hwnd;

            RECT rc{};
            GetClientRect(hwnd, &rc);
            st->width = rc.right;
            st->height = rc.bottom;
            const UINT dpi = DpiOf(hwnd);
            auto S = [dpi](int v) { return Scaled(v, dpi); };
            HINSTANCE inst = cs->hInstance;

            LOGFONTW lf{};
            lf.lfHeight = -MulDiv(9, (int)dpi, 72);
            wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
            lf.lfCharSet = DEFAULT_CHARSET;
            st->font = CreateFontIndirectW(&lf);
            st->ownFont = st->font != nullptr;
            if (!st->font) st->font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            auto Mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                          int x, int y, int w, int h, int id) -> HWND {
                HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                         x, y, w, h, hwnd,
                                         id >= 0 ? (HMENU)(INT_PTR)id : nullptr,
                                         inst, nullptr);
                if (c && st->font) SendMessageW(c, WM_SETFONT, (WPARAM)st->font, TRUE);
                return c;
            };

            Mk(L"STATIC", T(S_ASSOC_DLG_INTRO), SS_LEFT,
               S(16), S(12), st->width - S(32), S(22), -1);

            const size_t n = AssocFamilyCount();
            const int top = S(42);
            const int rowH = S(26);
            for (size_t i = 0; i < n; ++i) {
                HWND box = Mk(L"BUTTON", AssocFamilyLabel(i),
                              BS_AUTOCHECKBOX | WS_TABSTOP,
                              S(16), top + (int)i * rowH, st->width - S(32), S(22),
                              kIdFirstBox + (int)i);
                if (box) {
                    SendMessageW(box, BM_SETCHECK,
                                 AssocFamilyActive(i) ? BST_CHECKED : BST_UNCHECKED, 0);
                    st->boxes.push_back(box);
                }
            }

            const int noteY = top + (int)n * rowH + S(6);
            Mk(L"STATIC", T(S_ASSOC_DLG_NOTE), SS_LEFT,
               S(16), noteY, st->width - S(32), S(58), -1);

            const int by = st->height - S(46);
            const int bh = S(30);
            Mk(L"BUTTON", T(S_ASSOC_DLG_SETTINGS), WS_TABSTOP | BS_PUSHBUTTON,
               S(16), by, S(190), bh, kIdSettings);
            const int applyW = S(110);
            const int cancelW = S(100);
            const int selectW = S(90);
            int x = st->width - S(16) - applyW;
            Mk(L"BUTTON", T(S_ASSOC_DLG_APPLY), WS_TABSTOP | BS_DEFPUSHBUTTON,
               x, by, applyW, bh, kIdApply);
            x -= cancelW + S(8);
            Mk(L"BUTTON", T(S_ASSOC_DLG_CANCEL), WS_TABSTOP | BS_PUSHBUTTON,
               x, by, cancelW, bh, kIdCancel);
            x -= selectW + S(8);
            Mk(L"BUTTON", T(S_ASSOC_DLG_SELECT_ALL), WS_TABSTOP | BS_PUSHBUTTON,
               x, by, selectW, bh, kIdSelectAll);
            x -= selectW + S(8);
            Mk(L"BUTTON", T(S_ASSOC_DLG_SELECT_NONE), WS_TABSTOP | BS_PUSHBUTTON,
               x, by, selectW, bh, kIdSelectNone);

            if (!st->boxes.empty()) SetFocus(st->boxes[0]);
            return 0;
        }

        case WM_COMMAND: {
            if (!st) break;
            const int id = LOWORD(wp);
            if (id == kIdApply) ApplyAndClose(*st);
            else if (id == kIdCancel) CloseDialog(*st);
            else if (id == kIdSelectAll) SetAllBoxes(*st, true);
            else if (id == kIdSelectNone) SetAllBoxes(*st, false);
            else if (id == kIdSettings) AssocOpenDefaultApps();
            return 0;
        }

        case WM_CLOSE:
            if (st) CloseDialog(*st);
            return 0;

        case WM_DESTROY:
            if (st) {
                if (st->ownFont && st->font) DeleteObject(st->font);
                st->font = nullptr;
                st->hwnd = nullptr;
            }
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void RegisterDlgClass() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kDlgClass;
    RegisterClassExW(&wc);
    registered = true;
}

} // namespace

AssocDialogResult ShowAssocDialog(HWND owner) {
    AssocDialogResult result;
    RegisterDlgClass();

    DlgState st;
    st.owner = owner;
    st.running = true;

    const UINT dpi = DpiOf(owner);
    const int clientW = Scaled(820, dpi);
    const int clientH = Scaled(600, dpi);

    RECT wr{ 0, 0, clientW, clientH };
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    const DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT;
    AdjustWindowRectEx(&wr, style, FALSE, exStyle);

    int x = 0, y = 0;
    if (owner) {
        RECT orc{};
        GetWindowRect(owner, &orc);
        const int ow = orc.right - orc.left;
        const int oh = orc.bottom - orc.top;
        const int dw = wr.right - wr.left;
        const int dh = wr.bottom - wr.top;
        HMONITOR mon = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi)) {
            if (dw > ow || dh > oh) {
                x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - dw) / 2;
                y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - dh) / 2;
            } else {
                x = orc.left + (ow - dw) / 2;
                y = orc.top + (oh - dh) / 2;
            }
            if (x < mi.rcWork.left) x = mi.rcWork.left;
            if (y < mi.rcWork.top) y = mi.rcWork.top;
            if (x + dw > mi.rcWork.right) x = mi.rcWork.right - dw;
            if (y + dh > mi.rcWork.bottom) y = mi.rcWork.bottom - dh;
        }
    }

    HWND dlg = CreateWindowExW(exStyle, kDlgClass, T(S_ASSOC_DLG_TITLE), style,
                               x, y, wr.right - wr.left, wr.bottom - wr.top,
                               owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (!dlg) {
        st.running = false;
        return result;
    }

    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    if (owner) EnableWindow(owner, FALSE);

    MSG msg;
    while (st.running) {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got <= 0) break;
        if (msg.message == WM_KEYDOWN &&
            (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE) &&
            (msg.hwnd == dlg || IsChild(dlg, msg.hwnd))) {
            if (msg.wParam == VK_RETURN) ApplyAndClose(st);
            else CloseDialog(st);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    result.changed = st.changed;
    result.changedCount = st.changedCount;
    result.anyEnabled = st.anyEnabled;
    return result;
}
