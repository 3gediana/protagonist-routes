#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["httpx", "tqdm"]
# ///
"""Fetch all CC0 Quaternius assets from poly.pizza into third_party/assets/quaternius/.

poly.pizza is a Google Poly successor that hosts CC0 3D models on a public CDN
(static.poly.pizza). It's reachable from China without a proxy. Each model is a
single .glb file; bundles group related models (e.g. "Ultimate Animated Animal
Pack").

Strategy:
  1. Crawl https://poly.pizza/u/Quaternius/bundles to enumerate all bundles
     (parsing window.__SERVER_APP_STATE__ JSON embedded in the SSR HTML).
  2. For each bundle, list its models (PublicID + S3ID).
  3. Download each model's .glb directly from static.poly.pizza/<S3ID>.glb.
  4. Layout on disk: third_party/assets/quaternius/<bundle-slug>/<model-name>.glb
"""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path
from urllib.parse import unquote

import httpx
from tqdm import tqdm


ROOT = Path(__file__).resolve().parent.parent
ASSET_ROOT = ROOT / "third_party" / "assets" / "quaternius"
USER_PAGE = "https://poly.pizza/u/Quaternius"
BUNDLES_PAGE = "https://poly.pizza/u/Quaternius/bundles"
CDN = "https://static.poly.pizza"

# Match the JSON blob that React hydrates from in poly.pizza's SSR HTML.
SSR_MARKER = "window.__SERVER_APP_STATE__"


def extract_ssr_json(html: str) -> dict:
    """Find `window.__SERVER_APP_STATE__ = {...};` in HTML by scanning
    balanced braces. Regex is unreliable here because the JSON is huge and
    contains escape sequences that confuse non-greedy patterns."""
    idx = html.find(SSR_MARKER)
    if idx < 0:
        raise RuntimeError("SSR marker not found")
    eq = html.find("=", idx)
    start = html.find("{", eq)
    if start < 0:
        raise RuntimeError("SSR opening brace not found")
    depth = 0
    in_str = False
    escape = False
    for i in range(start, len(html)):
        c = html[i]
        if in_str:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return json.loads(html[start:i + 1])
    raise RuntimeError("SSR closing brace not found")


def slugify(s: str) -> str:
    s = re.sub(r"[^A-Za-z0-9._ -]", "", s).strip()
    s = re.sub(r"\s+", "_", s)
    return s[:60] or "unnamed"


def fetch_ssr_state(client: httpx.Client, url: str) -> dict:
    r = client.get(url, follow_redirects=True, timeout=30.0)
    r.raise_for_status()
    return extract_ssr_json(r.text)


def find_bundles(state: dict) -> list[dict]:
    """Walk the SSR state and pull out every dict that looks like a bundle."""
    found: list[dict] = []

    def walk(node):
        if isinstance(node, dict):
            # Heuristic: a bundle has Name + PublicID + Models[].
            if (
                "Models" in node
                and "Name" in node
                and "PublicID" in node
                and isinstance(node["Models"], list)
            ):
                found.append(node)
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    walk(state)
    # De-duplicate by PublicID
    seen, unique = set(), []
    for b in found:
        if b["PublicID"] not in seen:
            seen.add(b["PublicID"])
            unique.append(b)
    return unique


def fetch_model_meta(client: httpx.Client, public_id: str) -> dict:
    """Fetch /m/<id> and pull this single model's metadata + S3ID."""
    url = f"https://poly.pizza/m/{public_id}"
    state = fetch_ssr_state(client, url)

    def walk(node):
        if isinstance(node, dict):
            if (
                "S3ID" in node
                and "Name" in node
                and "PublicID" in node
                and node.get("PublicID") == public_id
            ):
                return node
            for v in node.values():
                r = walk(v)
                if r is not None:
                    return r
        elif isinstance(node, list):
            for item in node:
                r = walk(item)
                if r is not None:
                    return r
        return None

    meta = walk(state)
    if meta is None:
        raise RuntimeError(f"meta not found for /m/{public_id}")
    return meta


def download_glb(client: httpx.Client, s3id: str, dest: Path) -> int:
    if dest.exists() and dest.stat().st_size > 0:
        return 0  # already there
    url = f"{CDN}/{s3id}.glb"
    with client.stream("GET", url, timeout=60.0) as r:
        r.raise_for_status()
        dest.parent.mkdir(parents=True, exist_ok=True)
        tmp = dest.with_suffix(dest.suffix + ".part")
        n = 0
        with tmp.open("wb") as f:
            for chunk in r.iter_bytes(chunk_size=64 * 1024):
                f.write(chunk)
                n += len(chunk)
        tmp.rename(dest)
    return n


def main() -> int:
    ASSET_ROOT.mkdir(parents=True, exist_ok=True)
    headers = {"User-Agent": "Mozilla/5.0 (deep_protagonist asset fetcher)"}

    with httpx.Client(headers=headers) as client:
        # 1) Enumerate bundles
        print(f"Fetching {BUNDLES_PAGE} ...")
        bundles_state = fetch_ssr_state(client, BUNDLES_PAGE)
        bundles = find_bundles(bundles_state)

        # Also check user page for bundles surfaced there
        print(f"Fetching {USER_PAGE} ...")
        user_state = fetch_ssr_state(client, USER_PAGE)
        for b in find_bundles(user_state):
            if not any(x["PublicID"] == b["PublicID"] for x in bundles):
                bundles.append(b)

        if not bundles:
            print("No bundles found - poly.pizza layout may have changed.", file=sys.stderr)
            return 1
        print(f"Found {len(bundles)} bundles.")

        # 2) For every bundle, list models. The bundle's `Models` field already
        #    has PublicID + S3ID + (sometimes) Name. If Name is missing for a
        #    model we fetch its individual page.
        for b in bundles:
            bundle_dir = ASSET_ROOT / slugify(b["Name"])
            bundle_dir.mkdir(parents=True, exist_ok=True)
            print(f"\n[{b['Name']}] -> {bundle_dir}  ({len(b['Models'])} models)")

            with (bundle_dir / "_bundle.json").open("w", encoding="utf-8") as f:
                json.dump(b, f, indent=2)

            for model in tqdm(b["Models"], unit="model", leave=False):
                pid = model.get("PublicID")
                s3id = model.get("S3ID")
                name = model.get("Name")
                if not s3id:
                    # Need to fetch full meta
                    try:
                        meta = fetch_model_meta(client, pid)
                        s3id = meta["S3ID"]
                        name = meta.get("Name") or pid
                    except Exception as e:
                        tqdm.write(f"  ! skipping {pid}: {e}")
                        continue
                if not name:
                    name = pid
                dest = bundle_dir / f"{slugify(name)}_{pid}.glb"
                try:
                    n = download_glb(client, s3id, dest)
                    if n:
                        tqdm.write(f"  + {dest.name} ({n/1024:.0f} KB)")
                except httpx.HTTPError as e:
                    tqdm.write(f"  ! failed {dest.name}: {e}")

    print("\nDone.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
