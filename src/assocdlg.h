// Modal dialog for picking the file formats to associate with JPEGViewNX.
#pragma once

#include <windows.h>

struct AssocDialogResult {
    bool changed = false;       // anything was applied
    int  changedCount = 0;      // number of format families changed
    bool anyEnabled = false;    // at least one family was turned on
};

AssocDialogResult ShowAssocDialog(HWND owner);
