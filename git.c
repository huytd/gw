#include "git.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Run a command and capture its stdout into buf (up to buf_size-1 bytes).
   Returns true on success (exit code 0). */
static bool run_capture(const char *cmd, char *buf, int buf_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;

    int len = 0;
    int c;
    while ((c = fgetc(fp)) != EOF && len < buf_size - 1)
        buf[len++] = (char)c;
    buf[len] = '\0';

    /* Trim trailing newline */
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';

    int status = pclose(fp);
    return status == 0;
}

/* Run a command, return its exit code (0 = success). */
static bool run_silent(const char *cmd) {
    char devnull[256];
    snprintf(devnull, sizeof(devnull), "%s >/dev/null 2>&1", cmd);
    return system(devnull) == 0;
}

bool git_is_inside_work_tree(void) {
    return run_silent("git rev-parse --is-inside-work-tree");
}

int git_list_worktrees(WorktreeList *out) {
    FILE *fp = popen("git worktree list", "r");
    if (!fp) return -1;

    out->count = 0;
    char line[MAX_PATH_LEN + MAX_BRANCH_LEN + 32];

    while (fgets(line, sizeof(line), fp) && out->count < MAX_WORKTREES) {
        /* Format: /path/to/dir  abcdef12  [branch-name] */
        char path[MAX_PATH_LEN] = {0};
        char hash[64]           = {0};
        char branch_raw[MAX_BRANCH_LEN + 2] = {0}; /* includes [] */

        if (sscanf(line, "%511s %63s %129s", path, hash, branch_raw) < 2)
            continue;

        Worktree *wt = &out->items[out->count];
        strncpy(wt->path, path, MAX_PATH_LEN - 1);

        /* Strip surrounding brackets from branch name */
        int blen = (int)strlen(branch_raw);
        if (blen >= 2 && branch_raw[0] == '[' && branch_raw[blen-1] == ']') {
            strncpy(wt->branch, branch_raw + 1, (size_t)(blen - 2));
            wt->branch[blen - 2] = '\0';
        } else {
            strncpy(wt->branch, branch_raw, MAX_BRANCH_LEN - 1);
        }

        wt->is_main = (out->count == 0);
        out->count++;
    }

    pclose(fp);
    return out->count;
}

int git_commits_ahead(const char *repo_path, const char *base, const char *branch) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "git -C '%s' log '%s..%s' --oneline 2>/dev/null | wc -l",
        repo_path, base, branch);

    char buf[32] = {0};
    run_capture(cmd, buf, sizeof(buf));
    return atoi(buf);
}

void git_diff_stat(const char *repo_path, const char *base, const char *branch,
                   int *added, int *deleted) {
    *added = 0;
    *deleted = 0;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "git -C '%s' diff '%s...%s' --shortstat 2>/dev/null",
        repo_path, base, branch);

    char buf[512] = {0};
    if (!run_capture(cmd, buf, sizeof(buf))) return;

    /* Parse "N files changed, A insertions(+), D deletions(-)" */
    char *p;

    p = strstr(buf, "insertion");
    if (p) {
        while (p > buf && (*p < '0' || *p > '9')) p--;
        while (p > buf && (*(p-1) >= '0' && *(p-1) <= '9')) p--;
        *added = atoi(p);
    }

    p = strstr(buf, "deletion");
    if (p) {
        while (p > buf && (*p < '0' || *p > '9')) p--;
        while (p > buf && (*(p-1) >= '0' && *(p-1) <= '9')) p--;
        *deleted = atoi(p);
    }
}

bool git_is_clean(void) {
    return run_silent("git diff --quiet") &&
           run_silent("git diff --cached --quiet");
}

bool git_current_branch(char *out, int size) {
    char cmd[] = "git branch --show-current";
    return run_capture(cmd, out, size);
}

bool git_worktree_add(const char *target_path, const char *branch) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "git worktree add '%s' -b '%s' >/dev/null 2>&1",
        target_path, branch);
    return system(cmd) == 0;
}

bool git_worktree_remove(const char *path_or_name) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "git worktree remove '%s' >/dev/null 2>&1", path_or_name);
    return system(cmd) == 0;
}

bool git_merge(const char *repo_path, const char *branch) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "git -C '%s' merge '%s' >/dev/null 2>&1", repo_path, branch);
    return system(cmd) == 0;
}

bool git_delete_branch(const char *branch) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "git branch -d '%s' >/dev/null 2>&1", branch);
    return system(cmd) == 0;
}

const char *git_basename(const char *path) {
    const char *last = strrchr(path, '/');
    return last ? last + 1 : path;
}
