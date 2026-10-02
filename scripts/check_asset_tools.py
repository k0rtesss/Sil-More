#!/usr/bin/env python3
"""Check GUID ownership and audio-conversion file preservation in temp folders."""
from contextlib import redirect_stdout
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import io
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


def load(name):
    spec=spec_from_file_location(name,ROOT/"tools"/(name+".py"))
    module=module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class AssetTools(unittest.TestCase):
    def test_guid_canonical_values_and_invalid_input(self):
        tool=load("make_guid")
        self.assertEqual(tool.extract_guid_value("Q:0x0123-4567-89ab-cdef"),"0123456789abcdef")
        self.assertEqual(tool.extract_guid_value("Q:a"),"000000000000000a")
        for value in ("Q:","Q:not-a-guid","Q:123456789abcdef01"):
            with self.assertRaises(ValueError): tool.extract_guid_value(value)

    def test_guid_reserves_later_files_and_never_uses_zero(self):
        tool=load("make_guid")
        with tempfile.TemporaryDirectory() as folder:
            first=Path(folder)/"first.txt"; second=Path(folder)/"second.txt"
            first.write_text("N:1:Missing\n",encoding="utf-8")
            original="N:1:Existing\nQ:0x0123-4567-89ab-cdef\n"
            second.write_text(original,encoding="utf-8")
            args=SimpleNamespace(files=[str(first),str(second)],dry_run=False)
            with patch.object(tool,"parse_args",return_value=args), \
                 patch.object(tool.secrets,"token_hex",side_effect=["0"*16,"0123456789abcdef","fedcba9876543210"]), \
                 redirect_stdout(io.StringIO()):
                tool.main()
            self.assertIn("Q:fedcba9876543210",first.read_text(encoding="utf-8"))
            self.assertEqual(second.read_text(encoding="utf-8"),original)

    def test_guid_dry_run_and_invalid_file_do_not_write(self):
        tool=load("make_guid")
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/"record.txt"
            path.write_bytes(b"N:1:Missing\n")
            self.assertEqual(tool.process_file(path,True,set()),1)
            self.assertEqual(path.read_bytes(),b"N:1:Missing\n")
            path.write_bytes(b"N:1:Existing\nQ:not-a-guid\n")
            with self.assertRaises(ValueError): tool.process_file(path,False,set())
            self.assertEqual(path.read_bytes(),b"N:1:Existing\nQ:not-a-guid\n")

    def test_audio_success_failure_and_launch_error_preserve_other_files(self):
        for module_name,suffix in (("convert_sound_mp3_to_ogg",".mp3"),
                                   ("convert_xtra_wav_to_ogg",".wav")):
            tool=load(module_name)
            for result in ("success","failure","launch-error"):
                with self.subTest(tool=module_name,result=result), tempfile.TemporaryDirectory() as folder:
                    source=Path(folder)/("track"+suffix); source.write_bytes(b"source")
                    destination=source.with_suffix(".ogg"); destination.write_bytes(b"old-output")
                    unrelated=source.with_name("track.tmp.ogg"); unrelated.write_bytes(b"other-track")
                    def run(command,**kwargs):
                        if result=="launch-error": raise OSError("simulated launch failure")
                        Path(command[-1]).write_bytes(b"converted")
                        return SimpleNamespace(returncode=0 if result=="success" else 1,stderr="failed")
                    with patch.object(tool.subprocess,"run",side_effect=run):
                        if result=="success": tool.convert_file(source,"ffmpeg","5",True)
                        else:
                            with self.assertRaises((RuntimeError,OSError)):
                                tool.convert_file(source,"ffmpeg","5",True)
                    self.assertEqual(source.read_bytes(),b"source")
                    self.assertEqual(unrelated.read_bytes(),b"other-track")
                    self.assertEqual(destination.read_bytes(),b"converted" if result=="success" else b"old-output")
                    self.assertEqual(len(list(Path(folder).iterdir())),3)


if __name__=="__main__":
    unittest.main()
