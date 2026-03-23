#pragma once
#include <stdbool.h>

#define MAX_WORKTREES 64
#define MAX_PATH_LEN  512
#define MAX_BRANCH_LEN 128

typedef struct {
    char path[MAX_PATH_LEN];
    char branch[MAX_BRANCH_LEN];
    bool is_main;
} Worktree;

typedef struct {
    Worktree items[MAX_WORKTREES];
    int count;
} WorktreeList;

bool        git_is_inside_work_tree(void);
int         git_list_worktrees(WorktreeList *out);
int         git_commits_ahead(const char *repo_path, const char *base, const char *branch);
void        git_diff_stat(const char *repo_path, const char *base, const char *branch, int *added, int *deleted);
bool        git_is_clean(void);
bool        git_current_branch(char *out, int size);
bool        git_worktree_add(const char *target_path, const char *branch);
bool        git_worktree_remove(const char *path_or_name);
bool        git_merge(const char *repo_path, const char *branch);
bool        git_delete_branch(const char *branch);
const char *git_basename(const char *path);
