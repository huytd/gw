#pragma once
#include "git.h"

int  cmd_jump(const WorktreeList *wl, const char *branch);
int  cmd_new(const WorktreeList *wl, const char *branch, int argc, char **extra_argv);
int  cmd_remove(const WorktreeList *wl, const char *branch);
int  cmd_done(const WorktreeList *wl);
int  cmd_branch_jump(const WorktreeList *wl, const char *name, int argc, char **extra_argv);
