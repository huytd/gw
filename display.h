#pragma once
#include "git.h"

#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_RESET  "\033[0m"

void display_worktree_list(const WorktreeList *wl);
