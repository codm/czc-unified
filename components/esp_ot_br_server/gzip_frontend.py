#!/usr/bin/env python3
"""Compress all frontend assets with gzip level 9 into an output directory."""

import gzip
import os
import shutil
import sys


def gzip_directory(src: str, dst: str) -> None:
    for root, _dirs, files in os.walk(src):
        for filename in files:
            src_path = os.path.join(root, filename)
            rel_path = os.path.relpath(src_path, src)
            dst_path = os.path.join(dst, rel_path)
            os.makedirs(os.path.dirname(dst_path), exist_ok=True)
            with open(src_path, "rb") as fi, gzip.open(dst_path, "wb", compresslevel=9) as fo:
                shutil.copyfileobj(fi, fo)
            src_kb = os.path.getsize(src_path) / 1024
            dst_kb = os.path.getsize(dst_path) / 1024
            print(f"  {rel_path}: {src_kb:.1f} KB -> {dst_kb:.1f} KB ({100 * dst_kb / src_kb:.0f}%)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <src_dir> <dst_dir>")
        sys.exit(1)

    src_dir = sys.argv[1]
    dst_dir = sys.argv[2]

    print(f"Compressing frontend: {src_dir} -> {dst_dir}")
    gzip_directory(src_dir, dst_dir)
    print("Done.")
