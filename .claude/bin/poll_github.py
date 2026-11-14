"""Watch the Lyra repo for human activity the coordinator should react to.

Prints one line per new event, so it can run under Claude Code's Monitor tool
(each stdout line becomes a notification). Events:
  - comments on issues and PR conversations
  - inline review comments on PR diffs
  - submitted reviews on open PRs
  - newly opened issues and PRs

Anything authored by a bot account (including our own App) is ignored.

Progress is kept in a state file, so a monitor that expires and is re-armed
picks up exactly where the last one stopped. On the very first run there's no
state, and it starts from "now" rather than replaying history.

Before each poll it runs bot_token.py, because a long-running poller makes no
tool calls of its own and would otherwise outlive the hourly App token.

Configuration from the environment:
  GH_CONFIG_DIR     gh config dir holding the bot token (see bot_token.py)
  LYRA_REPO         owner/repo to watch (default: au-to-pi-lot/lyra)
  LYRA_POLL_STATE   state file path (default: poll_state.json next to GH_CONFIG_DIR)

Usage: poll_github.py [--interval SECONDS] [--once]
"""

import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timedelta, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
OVERLAP = timedelta(seconds=30)  # re-query a little history; `seen` dedupes it
SEEN_LIMIT = 2000
SNIPPET = 160


def utcnow():
    return datetime.now(timezone.utc)


def parse_ts(s):
    return datetime.fromisoformat(s.replace("Z", "+00:00"))


def fmt_ts(dt):
    return dt.strftime("%Y-%m-%dT%H:%M:%SZ")


def gh_api(path):
    out = subprocess.run(
        ["gh", "api", "-H", "Accept: application/vnd.github+json", path],
        capture_output=True, text=True, encoding="utf-8",
    )
    if out.returncode != 0:
        raise RuntimeError(f"gh api {path}: {out.stderr.strip()}")
    return json.loads(out.stdout)


def is_human(obj):
    user = obj.get("user") or {}
    return user.get("type") != "Bot"


def snippet(body):
    text = " ".join((body or "").split())
    return text if len(text) <= SNIPPET else text[:SNIPPET - 1] + "…"


def number_from_url(url):
    return url.rstrip("/").rsplit("/", 1)[-1] if url else "?"


def collect(repo, since):
    """Yield (key, timestamp, line) for every human event updated since `since`."""
    s = fmt_ts(since)

    for c in gh_api(f"repos/{repo}/issues/comments?since={s}&sort=updated&per_page=100"):
        if not is_human(c):
            continue
        edited = " (edited)" if c["updated_at"] != c["created_at"] else ""
        n = number_from_url(c.get("issue_url"))
        yield (f"ic:{c['id']}:{c['updated_at']}", c["updated_at"],
               f"[#{n} comment{edited}] {c['user']['login']}: {snippet(c['body'])} {c['html_url']}")

    for c in gh_api(f"repos/{repo}/pulls/comments?since={s}&sort=updated&per_page=100"):
        if not is_human(c):
            continue
        edited = " (edited)" if c["updated_at"] != c["created_at"] else ""
        n = number_from_url(c.get("pull_request_url"))
        yield (f"rc:{c['id']}:{c['updated_at']}", c["updated_at"],
               f"[#{n} review comment{edited} on {c.get('path')}] {c['user']['login']}: "
               f"{snippet(c['body'])} {c['html_url']}")

    # `since` on this endpoint filters by updated_at; only report genuinely new ones.
    for i in gh_api(f"repos/{repo}/issues?state=all&since={s}&sort=updated&per_page=100"):
        if not is_human(i) or parse_ts(i["created_at"]) < since:
            continue
        kind = "PR" if "pull_request" in i else "issue"
        yield (f"new:{i['id']}", i["created_at"],
               f"[#{i['number']} new {kind}] {i['user']['login']}: {i['title']} {i['html_url']}")

    # Reviews have no repo-wide endpoint, so check each open PR.
    for pr in gh_api(f"repos/{repo}/pulls?state=open&per_page=100"):
        for r in gh_api(f"repos/{repo}/pulls/{pr['number']}/reviews?per_page=100"):
            submitted = r.get("submitted_at")
            if not submitted or not is_human(r) or parse_ts(submitted) < since:
                continue
            body = f": {snippet(r['body'])}" if r.get("body") else ""
            yield (f"rv:{r['id']}", submitted,
                   f"[#{pr['number']} review {r['state']}] {r['user']['login']}{body} {r['html_url']}")


def load_state(path):
    try:
        with open(path, encoding="utf-8") as f:
            st = json.load(f)
        return parse_ts(st["checked_at"]), st.get("seen", [])
    except (OSError, ValueError, KeyError):
        return None, []


def save_state(path, checked_at, seen):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump({"checked_at": fmt_ts(checked_at), "seen": seen[-SEEN_LIMIT:]}, f)
    os.replace(tmp, path)


def poll_once(repo, state_path):
    checked_at, seen = load_state(state_path)
    started = utcnow()
    if checked_at is None:
        save_state(state_path, started, [])
        print(f"[poller] first run; watching {repo} from {fmt_ts(started)}", flush=True)
        return

    subprocess.run([sys.executable, os.path.join(HERE, "bot_token.py")], check=True)
    seen_set = set(seen)
    events = sorted(
        (e for e in collect(repo, checked_at - OVERLAP) if e[0] not in seen_set),
        key=lambda e: e[1],
    )
    for key, _, line in events:
        print(line, flush=True)
        seen.append(key)
    save_state(state_path, started, seen)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--interval", type=int, default=60)
    ap.add_argument("--once", action="store_true")
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")  # Windows consoles default to cp1252

    repo = os.environ.get("LYRA_REPO", "au-to-pi-lot/lyra")
    state_path = os.environ.get("LYRA_POLL_STATE") or os.path.join(
        os.path.dirname(os.environ["GH_CONFIG_DIR"].rstrip("\\/")), "poll_state.json")

    while True:
        try:
            poll_once(repo, state_path)
        except Exception as e:  # one failed poll shouldn't kill the monitor
            print(f"poll_github: {e}", file=sys.stderr, flush=True)
        if args.once:
            return
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
