"""验证基准协议参数、元数据和 JSON 输出的稳定行为。"""

import argparse
import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest


REPOSITORY = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / "benchmarks"))

import protocol


class BenchmarkProtocolTest(unittest.TestCase):
	def parser(self) -> argparse.ArgumentParser:
		parser = argparse.ArgumentParser()
		protocol.add_common_arguments(parser)
		return parser

	def arguments(self) -> argparse.Namespace:
		return self.parser().parse_args(
			[
				"--implementation",
				"skyuv",
				"--build-type",
				"Release",
				"--allocator",
				"system",
				"--compiler",
				"MSVC 19.41",
				"--output",
				"result.json",
			]
		)

	def test_default_arguments(self) -> None:
		arguments = self.arguments()
		self.assertEqual(arguments.duration, 10.0)
		self.assertEqual(arguments.warmup, 2.0)
		self.assertEqual(arguments.iterations, 5)
		self.assertEqual(arguments.seed, 1)

	def test_invalid_numbers(self) -> None:
		base = [
			"--implementation",
			"skyuv",
			"--build-type",
			"Release",
			"--allocator",
			"system",
			"--compiler",
			"MSVC",
			"--output",
			"result.json",
		]
		for option, value in (("--duration", "0"), ("--warmup", "-1"), ("--iterations", "0"), ("--seed", "-1")):
			with self.subTest(option=option):
				with contextlib.redirect_stderr(io.StringIO()):
					with self.assertRaises(SystemExit):
						self.parser().parse_args(base + [option, value])

	def test_result_and_atomic_write(self) -> None:
		environment = protocol.collect_environment(REPOSITORY)
		result = protocol.create_result("协议自测", "默认参数", self.arguments(), environment)
		self.assertEqual(result["schema_version"], 1)
		self.assertEqual(result["implementation"], "skyuv")
		self.assertEqual(len(result["environment"]["commit"]), 40)
		self.assertIn(result["environment"]["operating_system"], ("Windows", "Linux", "Darwin"))

		with tempfile.TemporaryDirectory() as directory:
			path = pathlib.Path(directory) / "nested" / "result.json"
			protocol.write_result(path, result)
			loaded = json.loads(path.read_text(encoding="utf-8"))
			self.assertEqual(loaded, result)
			self.assertFalse(path.with_name("result.json.tmp").exists())

	def test_schema_matches_generated_result(self) -> None:
		schema_path = REPOSITORY / "benchmarks" / "result.schema.json"
		schema = json.loads(schema_path.read_text(encoding="utf-8"))
		result = protocol.create_result("协议自测", "schema", self.arguments(), {})
		self.assertEqual(schema["properties"]["schema_version"]["const"], protocol.SCHEMA_VERSION)
		self.assertTrue(set(schema["required"]).issubset(result))

	def test_exit_codes_are_stable(self) -> None:
		self.assertEqual(int(protocol.ExitCode.SUCCESS), 0)
		self.assertEqual(int(protocol.ExitCode.INVALID_ARGUMENT), 2)
		self.assertEqual(int(protocol.ExitCode.SETUP_FAILED), 3)
		self.assertEqual(int(protocol.ExitCode.EXECUTION_FAILED), 4)
		self.assertEqual(int(protocol.ExitCode.VALIDATION_FAILED), 5)


if __name__ == "__main__":
	unittest.main()
