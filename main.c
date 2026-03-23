#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "git.h"
#include "display.h"
#include "worktree.h"
#include "init.h"

static void print_usage(void) {
    fprintf(stderr,
        "Usage: gw [COMMAND] [ARGS...]\n"
        "\n"
        "Commands:\n"
        "  l, list           List all worktrees\n"
        "  j, jump BRANCH    Jump to specified worktree\n"
        "  n, new  BRANCH    Create new worktree with branch\n"
        "  r, remove BRANCH  Remove specified worktree\n"
        "  d, done           Merge current branch into main and clean up\n"
        "  init SHELL        Print shell integration (fish|bash|zsh)\n"
        "  BRANCH [CMD...]   Jump to worktree, optionally run a command\n"
        "\n"
        "Without arguments: prints list (use shell integration for fzf picker)\n"
        "\n"
        "Shell setup (add to your rc file):\n"
        "  fish:  gw init fish | source\n"
        "  bash:  eval \"$(gw init bash)\"\n"
        "  zsh:   eval \"$(gw init zsh)\"\n"
    );
}

int main(int argc, char **argv) {
    /* gw init <shell> — no git check needed */
    if (argc >= 3 && strcmp(argv[1], "init") == 0) {
        const char *shell = argv[2];
        if (strcmp(shell, "fish") == 0) { print_init_fish(); return 0; }
        if (strcmp(shell, "bash") == 0) { print_init_bash(); return 0; }
        if (strcmp(shell, "zsh")  == 0) { print_init_zsh();  return 0; }
        fprintf(stderr, "✗ Unknown shell '%s'. Use: fish, bash, or zsh\n", shell);
        return 1;
    }

    if (!git_is_inside_work_tree()) {
        fprintf(stderr, "✗ Not in a git worktree!\n");
        return 1;
    }

    WorktreeList wl = {0};
    if (git_list_worktrees(&wl) <= 0) {
        fprintf(stderr, "✗ Could not read worktree list\n");
        return 1;
    }

    /* No arguments: print list (shell wrapper pipes this into fzf) */
    if (argc < 2) {
        display_worktree_list(&wl);
        return 0;
    }

    const char *opt   = argv[1];
    const char *param = argc >= 3 ? argv[2] : NULL;

    /* list */
    if (strcmp(opt, "l") == 0 || strcmp(opt, "list") == 0) {
        display_worktree_list(&wl);
        return 0;
    }

    /* jump */
    if (strcmp(opt, "j") == 0 || strcmp(opt, "jump") == 0) {
        if (!param) { fprintf(stderr, "✗ Please specify a worktree to jump to\n"); return 1; }
        return cmd_jump(&wl, param);
    }

    /* new */
    if (strcmp(opt, "n") == 0 || strcmp(opt, "new") == 0) {
        if (!param) { fprintf(stderr, "✗ Please specify a branch name for the new worktree\n"); return 1; }
        /* extra args start at argv[3] */
        return cmd_new(&wl, param, argc - 3, argv + 3);
    }

    /* remove */
    if (strcmp(opt, "r") == 0 || strcmp(opt, "remove") == 0) {
        if (!param) { fprintf(stderr, "✗ Please specify a worktree to remove\n"); return 1; }
        return cmd_remove(&wl, param);
    }

    /* done */
    if (strcmp(opt, "d") == 0 || strcmp(opt, "done") == 0) {
        return cmd_done(&wl);
    }

    /* Bare branch name: gw <branch> [cmd...] */
    int rc = cmd_branch_jump(&wl, opt, argc - 2, argv + 2);
    if (rc == -1) {
        fprintf(stderr, "✗ Unknown worktree or command: %s\n\n", opt);
        print_usage();
        return 1;
    }
    return rc;
}
