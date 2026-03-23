# gw — Git Worktree Manager

A lightweight C CLI for managing git worktrees, with built-in shell integration
for fish, bash, and zsh. No dependencies beyond a C compiler and git.

## Commands

| Command | Description |
|---|---|
| `gw` | Interactive fzf picker (requires shell integration) |
| `gw l` / `gw list` | List all worktrees with diff stats |
| `gw j <branch>` / `gw jump <branch>` | Jump to a worktree |
| `gw n <branch>` / `gw new <branch>` | Create new worktree + branch |
| `gw r <branch>` / `gw remove <branch>` | Remove a worktree |
| `gw d` / `gw done` | Merge → remove worktree → delete branch |
| `gw p <branch>` / `gw pull <branch>` | Soft-merge branch changes into main (no commit) |
| `gw <branch> [cmd...]` | Jump to worktree, optionally run a command |
| `gw init fish\|bash\|zsh` | Print shell integration code |

## Build & Install

```sh
make
sudo make install        # installs to /usr/local/bin
# or
make install PREFIX=~/.local
```

## Shell Setup

The binary cannot change the parent shell's directory itself (a fundamental Unix
constraint — `cd` must run inside the shell process). `gw init` emits a tiny shell
function that handles the `cd` on the binary's behalf. Add one line to your config:

**fish** — `~/.config/fish/config.fish`
```fish
gw init fish | source
```

**bash** — `~/.bashrc`
```bash
eval "$(gw init bash)"
```

**zsh** — `~/.zshrc`
```zsh
eval "$(gw init zsh)"
```

All logic (git operations, display, fzf wiring) lives in the C binary. The shell
function is purely a 10-line `cd` relay — the same pattern used by `zoxide`,
`direnv`, and `nvm`.

## Architecture

```
src/
├── main.c      — Argument parsing and dispatch
├── git.c/h     — Git subprocess wrappers (worktree list, log, diff, merge, …)
├── display.c/h — Colored worktree list output
├── worktree.c/h— Command implementations (jump, new, remove, done, pull)
└── init.c/h    — Shell integration code generator (fish / bash / zsh)
```

### cd/exec protocol

Since the binary runs as a child process, it communicates directory changes via
prefixed stdout lines that the shell function intercepts:

- `cd:<path>` — shell should `cd` to this path
- `exec:<cmd>` — shell should `eval` this command in the new directory

All user-facing status messages (`✓`, `✗`) go to **stderr** so they are always
visible and never interfere with the protocol.
