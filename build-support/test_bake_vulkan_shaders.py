from __future__ import annotations

import importlib.util
import struct
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "build-support" / "bake_vulkan_shaders.py"
SPEC = importlib.util.spec_from_file_location("bake_vulkan_shaders", SCRIPT)
baker = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = baker
SPEC.loader.exec_module(baker)

CACHE_HEADER = struct.Struct("<IIII16s")
CACHE_ENTRY = struct.Struct("<QQIIII")
PACK_HEADER = struct.Struct("<IIIIQII")
PACK_ENTRY = struct.Struct("<QQIIII")
CACHE_VERSION = 109


def spirv(tag: int) -> bytes:
    # A minimal complete vertex module with the generator word varied per fixture.
    words = (
        0x07230203, 0x00010000, tag, 6, 0,
        0x00020011, 1,                         # OpCapability Shader
        0x0003000E, 0, 1,                     # OpMemoryModel Logical GLSL450
        0x0005000F, 0, 4, 0x6E69616D, 0,      # OpEntryPoint Vertex %4 "main"
        0x00020013, 2,                        # %2 = OpTypeVoid
        0x00030021, 3, 2,                     # %3 = OpTypeFunction %2
        0x00050036, 2, 4, 0, 3,               # %4 = OpFunction %2 None %3
        0x000200F8, 5,                        # %5 = OpLabel
        0x000100FD,                           # OpReturn
        0x00010038,                           # OpFunctionEnd
    )
    return struct.pack(f"<{len(words)}I", *words)


def cache_files(directory: Path, entries, *, version=CACHE_VERSION, header_length=32, header_version=1):
    directory.mkdir(parents=True, exist_ok=True)
    index = bytearray(struct.pack("<I", version))
    index.extend(CACHE_HEADER.pack(header_length, header_version, 0x5143, 0x1234, bytes(range(16))))
    blob = bytearray()
    for hash_lo, hash_hi, source_len, stage, payload in entries:
        offset = len(blob)
        blob.extend(payload)
        index.extend(CACHE_ENTRY.pack(hash_lo, hash_hi, source_len, stage, offset, len(payload) // 4))
    idx_path, blob_path = directory / "vulkan_shaders.idx", directory / "vulkan_shaders.bin"
    idx_path.write_bytes(index)
    blob_path.write_bytes(blob)
    return idx_path, blob_path


class BakeVulkanShaderPackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_emits_loader_compatible_header_entries_and_word_offsets(self):
        pair = cache_files(self.root / "one", [
            (0x20, 0x2, 91, 1, spirv(2)),
            (0x10, 0x1, 45, 0, spirv(1)),
        ])
        out = self.root / "out"

        result = baker.bake([pair], out, expected_version=CACHE_VERSION)

        index = (out / "baked_shaders.idx").read_bytes()
        blob = (out / "baked_shaders.bin").read_bytes()
        self.assertEqual(result.entry_count, 2)
        self.assertEqual(PACK_HEADER.unpack_from(index), (0x4B425845, 1, 109, 2, len(blob), 0, 0))
        first = PACK_ENTRY.unpack_from(index, PACK_HEADER.size)
        second = PACK_ENTRY.unpack_from(index, PACK_HEADER.size + PACK_ENTRY.size)
        self.assertEqual(first, (0x10, 0x1, 45, 0, 0, 29))
        self.assertEqual(second, (0x20, 0x2, 91, 1, 29, 29))
        self.assertEqual(blob, spirv(1) + spirv(2))

    def test_merges_in_sorted_order_and_deduplicates_identical_entries(self):
        item_a = (0xA, 0xB, 100, 0, spirv(1))
        item_b = (0x1, 0x2, 200, 2, spirv(2))
        pair_a = cache_files(self.root / "a", [item_a, item_b])
        pair_b = cache_files(self.root / "b", [item_b])
        out1, out2 = self.root / "out1", self.root / "out2"

        baker.bake([pair_a, pair_b], out1, expected_version=CACHE_VERSION)
        baker.bake([pair_b, pair_a], out2, expected_version=CACHE_VERSION)

        self.assertEqual((out1 / "baked_shaders.idx").read_bytes(), (out2 / "baked_shaders.idx").read_bytes())
        self.assertEqual((out1 / "baked_shaders.bin").read_bytes(), (out2 / "baked_shaders.bin").read_bytes())
        self.assertEqual(PACK_HEADER.unpack_from((out1 / "baked_shaders.idx").read_bytes())[3], 2)

    def test_rejects_conflicting_duplicate_without_replacing_outputs(self):
        pair_a = cache_files(self.root / "a", [(1, 2, 3, 0, spirv(1))])
        pair_b = cache_files(self.root / "b", [(1, 2, 3, 0, spirv(2))])
        out = self.root / "out"
        out.mkdir()
        old_index, old_blob = b"previous-index", b"previous-blob"
        (out / "baked_shaders.idx").write_bytes(old_index)
        (out / "baked_shaders.bin").write_bytes(old_blob)

        with self.assertRaises(baker.BakeError):
            baker.bake([pair_a, pair_b], out, expected_version=CACHE_VERSION)

        self.assertEqual((out / "baked_shaders.idx").read_bytes(), old_index)
        self.assertEqual((out / "baked_shaders.bin").read_bytes(), old_blob)

    def test_rejects_stale_cache_version(self):
        pair = cache_files(self.root / "stale", [], version=CACHE_VERSION - 1)
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_incomplete_index_record(self):
        pair = cache_files(self.root / "partial", [])
        pair[0].write_bytes(pair[0].read_bytes() + b"partial")
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_invalid_pipeline_header(self):
        pair = cache_files(self.root / "bad-header", [], header_version=7)
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_pipeline_header_shorter_than_vulkan_header(self):
        pair = cache_files(self.root / "short-pipeline-header", [], header_length=31)
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_pipeline_header_with_nonfixed_length(self):
        pair = cache_files(self.root / "long-pipeline-header", [], header_length=33)
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_empty_merged_cache_without_replacing_outputs(self):
        pair = cache_files(self.root / "empty-cache", [])
        out = self.root / "out"
        out.mkdir()
        prior = {"baked_shaders.idx": b"old-index", "baked_shaders.bin": b"old-blob"}
        for name, contents in prior.items():
            (out / name).write_bytes(contents)

        with self.assertRaises(baker.BakeError):
            baker.bake([pair], out, expected_version=CACHE_VERSION)

        for name, contents in prior.items():
            self.assertEqual((out / name).read_bytes(), contents)

    def test_rejects_unknown_shader_stage(self):
        pair = cache_files(self.root / "bad-stage", [(1, 2, 3, 3, spirv(1))])
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_misaligned_blob_offset(self):
        pair = cache_files(self.root / "unaligned", [(1, 2, 3, 0, spirv(1))])
        data = bytearray(pair[0].read_bytes())
        struct.pack_into("<I", data, 4 + CACHE_HEADER.size + 24, 2)
        pair[0].write_bytes(data)
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_blob_range_past_end(self):
        pair = cache_files(self.root / "range", [(1, 2, 3, 0, spirv(1))])
        data = bytearray(pair[0].read_bytes())
        struct.pack_into("<I", data, 4 + CACHE_HEADER.size + 28, 99)
        pair[0].write_bytes(data)
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_zero_sized_blob(self):
        pair = cache_files(self.root / "empty-blob", [(1, 2, 3, 0, spirv(1))])
        data = bytearray(pair[0].read_bytes())
        struct.pack_into("<I", data, 4 + CACHE_HEADER.size + 28, 0)
        pair[0].write_bytes(data)
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_unaligned_blob_file(self):
        pair = cache_files(self.root / "unaligned-blob", [])
        pair[1].write_bytes(b"\x00")
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_non_spirv_and_short_spirv_payloads(self):
        for name, payload in (("magic", struct.pack("<6I", 0, 0x10000, 1, 2, 0, 7)),
                              ("short", struct.pack("<I", 0x07230203))):
            with self.subTest(name=name):
                pair = cache_files(self.root / name, [(1, 2, 3, 1, payload)])
                with self.assertRaises(baker.BakeError):
                    baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_invalid_spirv_header_fields(self):
        invalid = struct.pack("<6I", 0x07230203, 0x00010700, 1, 2, 0, 7)
        pair = cache_files(self.root / "invalid-spirv-header", [(1, 2, 3, 1, invalid)])
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_zero_or_truncated_spirv_instruction(self):
        for name, count_word in (("zero-word-count", 0x00000011), ("truncated-instruction", 0xFFFF0011)):
            with self.subTest(name=name):
                payload = bytearray(spirv(1))
                struct.pack_into("<I", payload, 5 * 4, count_word)
                pair = cache_files(self.root / name, [(1, 2, 3, 0, bytes(payload))])
                with self.assertRaises(baker.BakeError):
                    baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_rejects_truncated_cache_header(self):
        pair = cache_files(self.root / "truncated-header", [])
        pair[0].write_bytes(pair[0].read_bytes()[:20])
        with self.assertRaises(baker.BakeError):
            baker.bake([pair], self.root / "out", expected_version=CACHE_VERSION)

    def test_cli_reads_explicit_pairs_and_writes_pack(self):
        pair = cache_files(self.root / "cli", [(1, 2, 3, 2, spirv(1))])
        out = self.root / "cli-out"

        status = baker.main(["--input", str(pair[0]), str(pair[1]), "--output", str(out)])

        self.assertEqual(status, 0)
        self.assertTrue((out / "baked_shaders.idx").is_file())
        self.assertEqual((out / "baked_shaders.bin").read_bytes(), spirv(1))

    def test_reads_cache_version_from_tracked_source_path(self):
        self.assertEqual(baker._current_cache_version(), CACHE_VERSION)

    def test_first_rename_failure_leaves_prior_pair_and_cleans_staging_files(self):
        pair = cache_files(self.root / "first-rename", [(1, 2, 3, 0, spirv(1))])
        out = self.root / "out"
        out.mkdir()
        prior = {"baked_shaders.idx": b"old-index", "baked_shaders.bin": b"old-blob"}
        for name, contents in prior.items():
            (out / name).write_bytes(contents)

        with mock.patch.object(baker.os, "replace", side_effect=OSError("first rename failed")) as replace:
            with self.assertRaises(OSError):
                baker.bake([pair], out, expected_version=CACHE_VERSION)

        self.assertEqual(replace.call_count, 1)
        for name, contents in prior.items():
            self.assertEqual((out / name).read_bytes(), contents)
        self.assertEqual(list(out.glob("*.tmp")), [])

    def test_second_rename_failure_rolls_back_prior_pair_and_cleans_staging_files(self):
        pair = cache_files(self.root / "second-rename", [(1, 2, 3, 0, spirv(1))])
        out = self.root / "out"
        out.mkdir()
        prior = {"baked_shaders.idx": b"old-index", "baked_shaders.bin": b"old-blob"}
        for name, contents in prior.items():
            (out / name).write_bytes(contents)
        real_replace = baker.os.replace
        calls = 0

        def fail_second(source, destination):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("second rename failed")
            return real_replace(source, destination)

        with mock.patch.object(baker.os, "replace", side_effect=fail_second):
            with self.assertRaises(OSError):
                baker.bake([pair], out, expected_version=CACHE_VERSION)

        self.assertEqual(calls, 3)  # initial index, failed blob, restored old index
        for name, contents in prior.items():
            self.assertEqual((out / name).read_bytes(), contents)
        self.assertEqual(list(out.glob("*.tmp")), [])

    def test_recovery_staging_failure_preserves_prior_output_pair(self):
        pair = cache_files(self.root / "recovery-stage", [(1, 2, 3, 0, spirv(1))])
        out = self.root / "out"
        out.mkdir()
        prior = {"baked_shaders.idx": b"old-index", "baked_shaders.bin": b"old-blob"}
        for name, contents in prior.items():
            (out / name).write_bytes(contents)
        write_temp = baker._write_temp

        def fail_recovery_stage(directory, prefix, data, **kwargs):
            if prefix.startswith(".baked-index-recovery-"):
                raise OSError("recovery staging failed")
            return write_temp(directory, prefix, data, **kwargs)

        with mock.patch.object(baker, "_write_temp", side_effect=fail_recovery_stage):
            with self.assertRaises(OSError):
                baker.bake([pair], out, expected_version=CACHE_VERSION)

        for name, contents in prior.items():
            self.assertEqual((out / name).read_bytes(), contents)
        self.assertEqual(list(out.glob("*.tmp")), [])

    def test_rollback_rename_failure_invalidates_new_index_and_retains_recovery(self):
        pair = cache_files(self.root / "rollback-rename", [(1, 2, 3, 0, spirv(1))])
        out = self.root / "out"
        out.mkdir()
        old_index, old_blob = b"old-index", b"old-blob"
        (out / "baked_shaders.idx").write_bytes(old_index)
        (out / "baked_shaders.bin").write_bytes(old_blob)
        real_replace = baker.os.replace
        calls = 0

        def fail_blob_and_rollback(source, destination):
            nonlocal calls
            calls += 1
            if calls in (2, 3):
                raise OSError("injected output or rollback rename failure")
            return real_replace(source, destination)

        with mock.patch.object(baker.os, "replace", side_effect=fail_blob_and_rollback):
            with self.assertRaises(OSError):
                baker.bake([pair], out, expected_version=CACHE_VERSION)

        self.assertFalse((out / "baked_shaders.idx").exists())
        self.assertEqual((out / "baked_shaders.bin").read_bytes(), old_blob)
        recovery_files = list(out.glob(".baked-index-recovery-*.bak"))
        self.assertEqual(len(recovery_files), 1)
        self.assertEqual(recovery_files[0].read_bytes(), old_index)
        self.assertEqual(list(out.glob("*.tmp")), [])

    def test_old_index_read_failure_leaves_outputs_and_cleans_staging_files(self):
        pair = cache_files(self.root / "old-index-read", [(1, 2, 3, 0, spirv(1))])
        out = self.root / "out"
        out.mkdir()
        prior = {"baked_shaders.idx": b"old-index", "baked_shaders.bin": b"old-blob"}
        for name, contents in prior.items():
            (out / name).write_bytes(contents)
        original_read_bytes = Path.read_bytes

        def fail_index_read(path):
            if path == out / "baked_shaders.idx":
                raise OSError("old index read failed")
            return original_read_bytes(path)

        with mock.patch.object(Path, "read_bytes", fail_index_read):
            with self.assertRaises(OSError):
                baker.bake([pair], out, expected_version=CACHE_VERSION)

        for name, contents in prior.items():
            self.assertEqual((out / name).read_bytes(), contents)
        self.assertEqual(list(out.glob("*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
