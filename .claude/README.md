# Claude Code setup for Lyra

How agents working on Lyra act on GitHub, and how to set that up on a new machine. Background and the decisions behind it: issue #10.

## The bot identity

Agents act as **`lyra-agent-app[bot]`**, a GitHub App installed on this repo, so their issues, comments, PRs, pushes and commits are distinguishable from Ysanne's. Installation tokens expire after an hour. `bin/bot_token.py` mints them from the App's private key and keeps a dedicated `gh` config dir holding a fresh one. A PreToolUse hook runs it before every shell command (a no-op unless the token has under 10 minutes left).

Git follows the same identity, via environment variables that apply only inside Claude sessions:
- pushes to `git@github.com:` are rewritten to HTTPS and authenticated with `gh auth git-credential`, i.e. the App token (Git Credential Manager is bypassed);
- commits are authored and committed as `lyra-agent-app[bot] <355583250+lyra-agent-app[bot]@users.noreply.github.com>`.

`main` is protected by a ruleset (PRs required, no force-push or deletion), and the App is not on the bypass list.

## Setting up a machine

1. Put the App's private key outside the repo, e.g. `~/.config/lyra-agent-app/<name>.private-key.pem`.
2. Create `.claude/settings.local.json` (gitignored), adjusting paths:

```json
{
  "env": {
    "LYRA_APP_ID": "5720800",
    "LYRA_APP_INSTALLATION_ID": "181072172",
    "LYRA_APP_KEY": "C:\\Users\\<you>\\.config\\lyra-agent-app\\<name>.private-key.pem",
    "GH_CONFIG_DIR": "C:\\Users\\<you>\\.config\\lyra-agent-app\\gh",
    "GIT_AUTHOR_NAME": "lyra-agent-app[bot]",
    "GIT_AUTHOR_EMAIL": "355583250+lyra-agent-app[bot]@users.noreply.github.com",
    "GIT_COMMITTER_NAME": "lyra-agent-app[bot]",
    "GIT_COMMITTER_EMAIL": "355583250+lyra-agent-app[bot]@users.noreply.github.com",
    "GIT_CONFIG_COUNT": "3",
    "GIT_CONFIG_KEY_0": "url.https://github.com/.pushInsteadOf",
    "GIT_CONFIG_VALUE_0": "git@github.com:",
    "GIT_CONFIG_KEY_1": "credential.https://github.com.helper",
    "GIT_CONFIG_VALUE_1": "",
    "GIT_CONFIG_KEY_2": "credential.https://github.com.helper",
    "GIT_CONFIG_VALUE_2": "!gh auth git-credential"
  },
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Bash|PowerShell",
        "hooks": [
          {
            "type": "command",
            "command": "python",
            "args": ["<absolute path to this repo>\\.claude\\bin\\bot_token.py"],
            "timeout": 30
          }
        ]
      }
    ]
  }
}
```

   The hook path is absolute so it still resolves from subagent worktrees.
3. Restart Claude Code, then check: `gh api graphql -f query='{ viewer { login } }'` should print `lyra-agent-app[bot]`, and `git remote get-url --push origin` should print an `https://` URL.

Requirements: Python 3 and an `openssl` binary (Git for Windows ships one). No pip packages.

## Watching for replies

`bin/poll_github.py` prints one line per new human event: comments on issues and PRs, inline review comments, reviews on open PRs, and new issues or PRs. The coordinator runs it as a **background shell command** (not under Monitor):

```
python .claude/bin/poll_github.py --interval 60 --exit-on-event
```

It polls silently and exits as soon as it reports something, which wakes the coordinator exactly once. The coordinator handles the events and starts it again. A quiet stretch therefore costs no model tokens at all, so it can run all day while Ysanne is away. (Monitor would also work, but Monitor expires every 30 minutes, and each re-arm is a model turn.) Single failed polls go only to stderr. Five failures in a row produce a `[poller] failing` line and an exit, so a persistent problem still surfaces.

Progress is saved in `poll_state.json` next to `GH_CONFIG_DIR`, so restarts neither drop nor repeat events. The poller runs only while the Claude Code session is open. After a restart it reports everything since its last check.
