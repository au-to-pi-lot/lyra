"""Mint a GitHub App installation token so agents act on GitHub as the Lyra bot.

Installation tokens expire after an hour, so this keeps a gh config dir
(GH_CONFIG_DIR) whose hosts.yml always holds a fresh one. It's run as a
Claude Code PreToolUse hook: when the cached token has more than
REFRESH_MARGIN left it exits immediately, otherwise it mints a new one.

Uses only the stdlib plus the `openssl` CLI (ships with Git for Windows) for
the RS256 signature, so there's nothing to pip install.

Configuration comes from the environment:
  LYRA_APP_ID               GitHub App ID
  LYRA_APP_INSTALLATION_ID  installation ID on the lyra repo
  LYRA_APP_KEY              path to the App's private key (.pem)
  GH_CONFIG_DIR             gh config dir to write hosts.yml into

Usage: bot_token.py [--force]
"""

import base64
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
from datetime import datetime, timezone

API = "https://api.github.com"
REFRESH_MARGIN = 10 * 60  # seconds of validity left before we mint a new token


def b64url(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def find_openssl():
    found = shutil.which("openssl")
    if found:
        return found
    git_openssl = r"C:\Program Files\Git\mingw64\bin\openssl.exe"
    if os.path.exists(git_openssl):
        return git_openssl
    sys.exit("bot_token: openssl not found")


def make_jwt(app_id, key_path):
    now = int(time.time())
    header = b64url(json.dumps({"alg": "RS256", "typ": "JWT"}).encode())
    # iat is backdated a minute to tolerate clock drift; GitHub caps exp at 10 min.
    payload = b64url(json.dumps({"iat": now - 60, "exp": now + 540, "iss": app_id}).encode())
    signing_input = f"{header}.{payload}".encode()
    sig = subprocess.run(
        [find_openssl(), "dgst", "-sha256", "-sign", key_path],
        input=signing_input, capture_output=True, check=True,
    ).stdout
    return f"{header}.{payload}.{b64url(sig)}"


def github(method, path, bearer):
    req = urllib.request.Request(
        API + path, method=method,
        headers={
            "Authorization": f"Bearer {bearer}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "lyra-bot-token",
        },
    )
    with urllib.request.urlopen(req, timeout=20) as resp:
        return json.load(resp)


def atomic_write(path, text):
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path))
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(text)
    os.replace(tmp, path)


def main():
    try:
        app_id = os.environ["LYRA_APP_ID"]
        installation_id = os.environ["LYRA_APP_INSTALLATION_ID"]
        key_path = os.environ["LYRA_APP_KEY"]
        config_dir = os.environ["GH_CONFIG_DIR"]
    except KeyError as e:
        sys.exit(f"bot_token: missing environment variable {e}")

    os.makedirs(config_dir, exist_ok=True)
    meta_path = os.path.join(config_dir, "bot_token_meta.json")

    if "--force" not in sys.argv:
        try:
            with open(meta_path, encoding="utf-8") as f:
                meta = json.load(f)
            if meta["expires_at"] - time.time() > REFRESH_MARGIN:
                return
        except (OSError, ValueError, KeyError):
            pass

    jwt = make_jwt(app_id, key_path)
    slug = github("GET", "/app", jwt)["slug"]
    result = github("POST", f"/app/installations/{installation_id}/access_tokens", jwt)
    token = result["token"]
    expires_at = datetime.fromisoformat(result["expires_at"].replace("Z", "+00:00"))
    login = f"{slug}[bot]"

    # Both the legacy top-level token and the multi-account `users` map, so any
    # gh version reads it. git_protocol https lets `gh auth setup-git` push as the bot.
    atomic_write(os.path.join(config_dir, "hosts.yml"), (
        "github.com:\n"
        f"    oauth_token: {token}\n"
        f"    user: {login}\n"
        "    git_protocol: https\n"
        "    users:\n"
        f"        {login}:\n"
        f"            oauth_token: {token}\n"
    ))
    atomic_write(meta_path, json.dumps({
        "login": login,
        "expires_at": expires_at.astimezone(timezone.utc).timestamp(),
    }))


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        sys.exit(f"bot_token: openssl failed: {e.stderr.decode(errors='replace').strip()}")
    except urllib.error.HTTPError as e:
        sys.exit(f"bot_token: GitHub API {e.code}: {e.read().decode(errors='replace').strip()}")
