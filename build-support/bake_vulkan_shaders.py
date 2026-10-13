#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0+
"""Repack explicit PCSX2 Vulkan shader-cache files as a baked SPIR-V pack.

Example::

    python build-support/bake_vulkan_shaders.py --input cache/vulkan_shaders.idx cache/vulkan_shaders.bin \
        --input another/vulkan_shaders.idx another/vulkan_shaders.bin --output baked

The emitted baked_shaders.idx/.bin format is consumed by VKBakedShaderPack.
Only portable shader SPIR-V is copied; GPU-specific Vulkan pipeline-cache data
(the cache index's vendor/device/UUID header) is validated as metadata and omitted.
"""

from __future__ import annotations

import argparse
import os
import re
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

CACHE_HEADER = struct.Struct("<IIII16s")
CACHE_ENTRY = struct.Struct("<QQIIII")
PACK_HEADER = struct.Struct("<IIIIQII")
PACK_ENTRY = struct.Struct("<QQIIII")
PACK_MAGIC = 0x4B425845  # 'EXBK' as written by VKBakedShaderPack.cpp
PACK_VERSION = 1
SPIRV_MAGIC = 0x07230203
VALID_SHADER_TYPES = frozenset((0, 1, 2))  # shaderc vertex, fragment, compute
MAX_U32 = 0xFFFFFFFF


class BakeError(ValueError):
    """An input cache or output pack is malformed or incompatible."""


@dataclass(frozen=True, order=True)
class ShaderKey:
    source_hash_low: int
    source_hash_high: int
    source_length: int
    shader_type: int


@dataclass(frozen=True)
class BakeResult:
    entry_count: int
    blob_bytes: int


def _current_cache_version() -> int:
    version_header = (
        Path(__file__).resolve().parents[1]
        / "app/src/main/cpp/pcsx2/pcsx2/ShaderCacheVersion.h"
    )
    try:
        content = version_header.read_text(encoding="utf-8")
    except OSError as exc:
        raise BakeError(f"cannot read shader cache version from {version_header}: {exc}") from exc
    match = re.search(r"\bSHADER_CACHE_VERSION\s*=\s*(\d+)\s*;", content)
    if match is None:
        raise BakeError(f"cannot find SHADER_CACHE_VERSION in {version_header}")
    return int(match.group(1))


def _validate_spirv(data: bytes, context: str) -> None:
    if len(data) < 20 or len(data) % 4:
        raise BakeError(f"{context} is truncated or not word-aligned SPIR-V")
    magic, version, _generator, bound, schema = struct.unpack_from("<5I", data)
    if magic != SPIRV_MAGIC:
        raise BakeError(f"{context} does not start with the SPIR-V magic")
    # SPIR-V's standard header reserves the schema word as zero, requires a
    # nonzero ID bound, and currently encodes versions as 1.0 through 1.6.
    major = (version >> 16) & 0xFF
    minor = (version >> 8) & 0xFF
    if version & 0xFF0000FF or major != 1 or minor > 6:
        raise BakeError(f"{context} has an invalid or unsupported SPIR-V version word")
    if bound == 0 or schema != 0:
        raise BakeError(f"{context} has an invalid SPIR-V header")

    # Check framing only. Full SPIR-V instruction validation requires the
    # external SPIRV-Tools validator, which this standalone script does not use.
    word_count = len(data) // 4
    cursor = 5
    while cursor < word_count:
        instruction_word = struct.unpack_from("<I", data, cursor * 4)[0]
        instruction_words = instruction_word >> 16
        if instruction_words == 0 or cursor + instruction_words > word_count:
            raise BakeError(f"{context} contains a zero-sized or truncated SPIR-V instruction")
        cursor += instruction_words


def _read_cache(index_path: Path, blob_path: Path, expected_version: int) -> dict[ShaderKey, bytes]:
    try:
        index_data = index_path.read_bytes()
        blob_data = blob_path.read_bytes()
    except OSError as exc:
        raise BakeError(f"cannot read cache pair '{index_path}' and '{blob_path}': {exc}") from exc

    first_entry = 4 + CACHE_HEADER.size
    if len(index_data) < first_entry:
        raise BakeError(f"'{index_path}' is truncated before the cache header is complete")
    (cache_version,) = struct.unpack_from("<I", index_data)
    if cache_version != expected_version:
        raise BakeError(
            f"'{index_path}' uses shader cache version {cache_version}; "
            f"this source expects {expected_version}"
        )

    header_length, header_version, _vendor_id, _device_id, _uuid = CACHE_HEADER.unpack_from(index_data, 4)
    if header_length != CACHE_HEADER.size or header_version != 1:
        raise BakeError(f"'{index_path}' has an invalid Vulkan pipeline-cache metadata header")
    if (len(index_data) - first_entry) % CACHE_ENTRY.size:
        raise BakeError(f"'{index_path}' ends with an incomplete shader-cache entry")
    if len(blob_data) % 4:
        raise BakeError(f"'{blob_path}' is not word-aligned")

    entries: dict[ShaderKey, bytes] = {}
    for entry_number, offset in enumerate(range(first_entry, len(index_data), CACHE_ENTRY.size), 1):
        hash_low, hash_high, source_length, shader_type, file_offset, blob_words = CACHE_ENTRY.unpack_from(
            index_data, offset
        )
        if shader_type not in VALID_SHADER_TYPES:
            raise BakeError(f"'{index_path}' entry {entry_number} has unsupported shader type {shader_type}")
        if file_offset % 4:
            raise BakeError(f"'{index_path}' entry {entry_number} has a misaligned byte offset")
        if blob_words == 0:
            raise BakeError(f"'{index_path}' entry {entry_number} has an empty SPIR-V blob")
        blob_end = file_offset + blob_words * 4
        if blob_end > len(blob_data):
            raise BakeError(f"'{index_path}' entry {entry_number} points beyond '{blob_path}'")
        spirv = blob_data[file_offset:blob_end]
        _validate_spirv(spirv, f"'{index_path}' entry {entry_number}")

        key = ShaderKey(hash_low, hash_high, source_length, shader_type)
        previous = entries.get(key)
        if previous is not None and previous != spirv:
            raise BakeError(f"'{index_path}' contains conflicting data for shader key {key}")
        entries[key] = spirv
    return entries


def _build_pack(entries: dict[ShaderKey, bytes], cache_version: int) -> tuple[bytes, bytes]:
    ordered = sorted(entries.items())
    if len(ordered) > MAX_U32:
        raise BakeError("shader entry count exceeds the baked pack format")

    index = bytearray()
    blob = bytearray()
    for key, spirv in ordered:
        if len(blob) % 4:
            raise BakeError("internal error: output SPIR-V blob lost word alignment")
        word_offset = len(blob) // 4
        word_count = len(spirv) // 4
        if word_offset > MAX_U32 or word_count > MAX_U32 or word_offset + word_count > MAX_U32:
            raise BakeError("shader SPIR-V data exceeds the baked pack's 32-bit word ranges")
        index.extend(PACK_ENTRY.pack(
            key.source_hash_low,
            key.source_hash_high,
            key.source_length,
            key.shader_type,
            word_offset,
            word_count,
        ))
        blob.extend(spirv)

    header = PACK_HEADER.pack(PACK_MAGIC, PACK_VERSION, cache_version, len(ordered), len(blob), 0, 0)
    return bytes(header + index), bytes(blob)


def _write_temp(directory: Path, prefix: str, data: bytes, *, suffix: str = ".tmp") -> Path:
    fd, name = tempfile.mkstemp(prefix=prefix, suffix=suffix, dir=directory)
    path = Path(name)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
    except BaseException:
        path.unlink(missing_ok=True)
        raise
    return path


def _replace_pair(output_dir: Path, index_data: bytes, blob_data: bytes) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    index_path = output_dir / "baked_shaders.idx"
    blob_path = output_dir / "baked_shaders.bin"
    staged_index: Path | None = None
    staged_blob: Path | None = None
    recovery_index: Path | None = None
    preserve_recovery = False
    try:
        staged_index = _write_temp(output_dir, ".baked-index-", index_data)
        staged_blob = _write_temp(output_dir, ".baked-blob-", blob_data)
        # Each replacement is atomic on the output filesystem. A pair cannot
        # be atomically swapped by portable filesystem APIs, so retain old
        # bytes in a complete recovery file before the first output is replaced.
        old_index = index_path.read_bytes() if index_path.exists() else None
        if old_index is not None:
            recovery_index = _write_temp(
                output_dir, ".baked-index-recovery-", old_index, suffix=".bak"
            )
        index_replaced = False
        try:
            os.replace(staged_index, index_path)
            index_replaced = True
            os.replace(staged_blob, blob_path)
        except OSError:
            if index_replaced:
                if recovery_index is None:
                    index_path.unlink(missing_ok=True)
                else:
                    try:
                        os.replace(recovery_index, index_path)
                        recovery_index = None
                    except OSError:
                        # A mixed pack must not remain loadable. Keep the prior
                        # index in the recovery file if restoring it failed.
                        try:
                            index_path.unlink(missing_ok=True)
                        except OSError:
                            quarantine_path = index_path.with_name(index_path.name + ".invalid")
                            try:
                                os.replace(index_path, quarantine_path)
                            except OSError:
                                pass
                        preserve_recovery = recovery_index.exists()
            raise
        if recovery_index is not None:
            recovery_index.unlink(missing_ok=True)
            recovery_index = None
    finally:
        if staged_index is not None:
            staged_index.unlink(missing_ok=True)
        if staged_blob is not None:
            staged_blob.unlink(missing_ok=True)
        if recovery_index is not None and not preserve_recovery:
            recovery_index.unlink(missing_ok=True)


def bake(
    input_pairs: Sequence[tuple[Path | str, Path | str]],
    output_dir: Path | str,
    *,
    expected_version: int | None = None,
) -> BakeResult:
    """Validate and merge cache index/blob pairs, then write a baked pack."""
    if not input_pairs:
        raise BakeError("at least one explicit cache index/blob pair is required")
    cache_version = _current_cache_version() if expected_version is None else expected_version
    if not 0 <= cache_version <= MAX_U32:
        raise BakeError("expected shader cache version is outside the pack format")

    merged: dict[ShaderKey, bytes] = {}
    for index_path, blob_path in input_pairs:
        entries = _read_cache(Path(index_path), Path(blob_path), cache_version)
        for key, spirv in entries.items():
            previous = merged.get(key)
            if previous is not None and previous != spirv:
                raise BakeError(f"input caches contain conflicting data for shader key {key}")
            merged[key] = spirv

    if not merged:
        raise BakeError("input caches contain no shaders to bake")

    index_data, blob_data = _build_pack(merged, cache_version)
    _replace_pair(Path(output_dir), index_data, blob_data)
    return BakeResult(len(merged), len(blob_data))


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input", action="append", nargs=2, metavar=("INDEX", "BLOB"), required=True,
        help="explicit PCSX2 vulkan_shaders.idx and vulkan_shaders.bin pair; repeat to merge caches",
    )
    parser.add_argument("--output", type=Path, required=True, help="directory for baked_shaders.idx/.bin")
    args = parser.parse_args(argv)
    try:
        result = bake(args.input, args.output)
    except (BakeError, OSError, struct.error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(f"wrote {args.output / 'baked_shaders.idx'} and baked_shaders.bin")
    print(f"shaders: {result.entry_count}; SPIR-V bytes: {result.blob_bytes}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
