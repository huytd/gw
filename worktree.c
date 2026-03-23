#include "worktree.h"
#include "git.h"
#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Find the first worktree whose path or branch contains `needle`. */
static const Worktree *find_worktree(const WorktreeList *wl, const char *needle) {
    for (int i = 0; i < wl->count; i++) {
        if (strstr(wl->items[i].path,   needle) ||
            strcmp(wl->items[i].branch, needle) == 0)
            return &wl->items[i];
    }
    return NULL;
}

/* Emit the cd directive the shell wrapper will act on. */
static void emit_cd(const char *path) {
    printf("cd:%s\n", path);
}

/* Emit a command for the shell wrapper to eval in the new directory. */
static void emit_exec(int argc, char **argv) {
    printf("exec:");
    for (int i = 0; i < argc; i++) {
        if (i) printf(" ");
        printf("%s", argv[i]);
    }
    printf("\n");
}

/* ── jump ──────────────────────────────────────────────────────────────── */

int cmd_jump(const WorktreeList *wl, const char *branch) {
    const Worktree *wt = find_worktree(wl, branch);
    if (!wt) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Worktree '%s' not found\n", branch);
        return 1;
    }
    emit_cd(wt->path);
    fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " You are now working in %s\n", branch);
    return 0;
}

/* ── new ───────────────────────────────────────────────────────────────── */

int cmd_new(const WorktreeList *wl, const char *branch, int argc, char **extra_argv) {
    const char *main_folder = git_basename(wl->items[0].path);

    char target_path[MAX_PATH_LEN];
    snprintf(target_path, sizeof(target_path), "../%s-%s", main_folder, branch);

    /* If it already exists, reuse it (idempotent behavior). */
    WorktreeList fresh = {0};
    int listed = git_list_worktrees(&fresh);
    const Worktree *wt = (listed > 0) ? find_worktree(&fresh, branch) : NULL;
    if (wt) {
        emit_cd(wt->path);
        fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Work tree %s already exists, reusing it\n", branch);
        if (argc > 0) {
            fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Starting command...\n");
            emit_exec(argc, extra_argv);
        }
        return 0;
    }

    bool created_new = false;
    bool attached_existing_branch = false;

    if (git_branch_exists(branch)) {
        attached_existing_branch = git_worktree_add_existing(target_path, branch);
    } else {
        created_new = git_worktree_add(target_path, branch);
        if (!created_new && git_branch_exists(branch)) {
            /* Branch might have been created concurrently; retry as existing. */
            attached_existing_branch = git_worktree_add_existing(target_path, branch);
        }
    }

    if (!created_new && !attached_existing_branch) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Failed to create worktree '%s'\n", branch);
        return 1;
    }

    /* Re-read to get absolute path */
    WorktreeList latest = {0};
    if (git_list_worktrees(&latest) > 0) {
        wt = find_worktree(&latest, branch);
    } else {
        wt = NULL;
    }

    if (wt) emit_cd(wt->path);
    else emit_cd(target_path); /* Fallback if listing fails unexpectedly */

    if (created_new) fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Work tree %s created!\n", branch);
    else fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Work tree %s created from existing branch\n", branch);

    if (argc > 0) {
        fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Starting command...\n");
        emit_exec(argc, extra_argv);
    }
    return 0;
}

/* ── remove ────────────────────────────────────────────────────────────── */

int cmd_remove(const WorktreeList *wl, const char *branch) {
    const Worktree *wt = find_worktree(wl, branch);
    if (!wt) {
        fprintf(stderr,
            COLOR_RED "✗" COLOR_RESET " Worktree '%s' not found\n", branch);
        return 1;
    }

    /* If we're inside the worktree being removed, move to main first */
    char cwd[MAX_PATH_LEN] = {0};
    if (getcwd(cwd, sizeof(cwd))) {
        if (strcmp(cwd, wt->path) == 0) {
            emit_cd(wl->items[0].path);
            fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Moved to main worktree\n");
        }
    }

    if (!git_worktree_remove(wt->path)) {
        fprintf(stderr,
            COLOR_RED "✗" COLOR_RESET " Failed to remove worktree '%s' (may have uncommitted changes)\n",
            branch);
        return 1;
    }
    fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Work tree %s removed!\n", branch);
    return 0;
}

/* ── done ──────────────────────────────────────────────────────────────── */

int cmd_done(const WorktreeList *wl) {
    const char *main_path   = wl->items[0].path;
    const char *main_branch = wl->items[0].branch;

    char current_branch[MAX_BRANCH_LEN] = {0};
    if (!git_current_branch(current_branch, sizeof(current_branch))) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Could not determine current branch\n");
        return 1;
    }

    if (strcmp(current_branch, "main") == 0 || strcmp(current_branch, "master") == 0) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Already on %s, nothing to do\n", current_branch);
        return 1;
    }

    if (!git_is_clean()) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Uncommitted changes detected, please commit or stash first\n");
        return 1;
    }

    if (!git_merge(main_path, current_branch)) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Merge failed, please resolve conflicts manually\n");
        return 1;
    }
    fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Merged %s into %s\n", current_branch, main_branch);

    char cwd[MAX_PATH_LEN] = {0};
    if (!getcwd(cwd, sizeof(cwd))) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Could not determine current working directory\n");
        return 1;
    }

    /* Move parent shell and this process to main before removing this worktree.
       The shell wrapper applies cd: after this binary exits, so we must chdir
       here as well to avoid running later git commands from a deleted cwd. */
    emit_cd(main_path);
    if (chdir(main_path) != 0) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Failed to switch to main worktree\n");
        return 1;
    }

    if (!git_worktree_remove(cwd)) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Failed to remove worktree\n");
        return 1;
    }
    fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Worktree removed\n");

    if (!git_delete_branch(current_branch)) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Failed to delete branch %s\n", current_branch);
        return 1;
    }
    fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Branch %s deleted\n", current_branch);
    fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Done! Now on %s\n", main_branch);
    return 0;
}

/* ── pull ──────────────────────────────────────────────────────────────── */

int cmd_pull(const WorktreeList *wl, const char *branch) {
    const char *main_path   = wl->items[0].path;
    const char *main_branch = wl->items[0].branch;

    const Worktree *wt = find_worktree(wl, branch);
    if (!wt) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Worktree '%s' not found\n", branch);
        return 1;
    }

    if (strcmp(wt->branch, main_branch) == 0) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Cannot pull from the main worktree into itself\n");
        return 1;
    }

    if (!git_merge_no_commit(main_path, wt->branch)) {
        fprintf(stderr, COLOR_RED "✗" COLOR_RESET " Merge failed, please resolve conflicts manually in %s\n", main_path);
        return 1;
    }

    fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Pulled changes from %s into %s (staged, not committed)\n",
            wt->branch, main_branch);
    return 0;
}

/* ── bare branch jump (gw <branch> [cmd...]) ───────────────────────────── */

int cmd_branch_jump(const WorktreeList *wl, const char *name, int argc, char **extra_argv) {
    const Worktree *wt = find_worktree(wl, name);
    if (!wt) return -1; /* signal: not found, caller prints usage */

    emit_cd(wt->path);
    fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Jumping to work tree %s\n", name);

    if (argc > 0) {
        fprintf(stderr, COLOR_GREEN "✓" COLOR_RESET " Starting command...\n");
        emit_exec(argc, extra_argv);
    }
    return 0;
}
