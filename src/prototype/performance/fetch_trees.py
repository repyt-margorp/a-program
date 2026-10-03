#!/usr/bin/env python3
"""Fetch the unchanged official sources at the historical Bend comparison pin."""

import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

REVISION = "7d24b8d0235cb9781140512c0f163c48ea84a719"
HASHES = {
	"bend": "71ec84456fa54b860ddaf0f6bd1614581bdb801a9d256b7c77365c0711111fcb",
	"lean": "51156f26e4d9e47ea3a8a9e35cce6c86afb33a954b7cde9bb618b6315f1ebf38",
	"agda": "d0e0eedee48ab7c8748d9a5c82fb2e8564cd19518f9fc8425dd952abd2fbb2a1",
	"v": "792a9f61df4dd3357d01f1d5061561f2c86872d3cab6aaf34604f6b572f1d3e5",
}


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("output", type=Path)
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=False)
	metadata = {"revision": REVISION, "sources": {}}
	for extension, expected in HASHES.items():
		url = f"https://raw.githubusercontent.com/bendlang/bend/{REVISION}/bench/checker/trees_400/main.{extension}"
		data = urllib.request.urlopen(url, timeout=60).read()
		assert hashlib.sha256(data).hexdigest() == expected, url
		(args.output / f"main.{extension}").write_bytes(data)
		metadata["sources"][extension] = {"url": url, "sha256": expected}
	(args.output / "sources.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
	main()
