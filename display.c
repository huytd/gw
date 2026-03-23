#include "display.h"
#include "git.h"
#include <stdio.h>
#include <string.h>

#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_RESET  "\033[0m"

void display_worktree_list(const WorktreeList *wl) {
    if (wl->count == 0) return;

    const char *main_branch = wl->items[0].branch;

    for (int i = 0; i < wl->count; i++) {
        const Worktree *wt = &wl->items[i];
        const char *label  = git_basename(wt->path);
        const char *branch = wt->branch[0] ? wt->branch : "(detached)";

        if (i == 0 || strcmp(branch, main_branch) == 0) {
            printf("%s [%s]\n", label, branch);
        } else {
            int commits = git_commits_ahead(wt->path, main_branch, branch);
            int added = 0, deleted = 0;
            git_diff_stat(wt->path, main_branch, branch, &added, &deleted);

            printf("%s [%s] (%d commits, %s+%d%s, %s-%d%s)\n",
                label, branch, commits,
                COLOR_GREEN, added,   COLOR_RESET,
                COLOR_RED,   deleted, COLOR_RESET);
        }
    }
}
