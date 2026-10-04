import argparse
import ctypes
import errno
import hashlib
import json
import os
import pathlib
import runpy
import subprocess
import sys
import tempfile


class CommandFailure(Exception):
	def __init__(self, status):
		self.status = status


def digest(path):
	return hashlib.sha256(path.read_bytes()).hexdigest()


def run(argv, commands):
	result = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	commands.append({"argv": argv, "exit": result.returncode})
	if result.stdout:
		sys.stdout.buffer.write(result.stdout)
	if result.stderr:
		sys.stderr.buffer.write(result.stderr)
	if result.returncode:
		raise CommandFailure(result.returncode if result.returncode in (1, 2, 3, 4) else 2)


def publish_directory(source, destination):
	# Linux no-replace directory publication refuses existing empty directories
	# and symlinks too. There is no overwriting fallback on unsupported hosts.
	library = ctypes.CDLL(None, use_errno=True)
	try:
		rename = library.renameat2
	except AttributeError:
		raise OSError(errno.ENOSYS, "no-replace directory publication unavailable")
	rename.argtypes = (ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint)
	rename.restype = ctypes.c_int
	if rename(-100, os.fsencode(source), -100, os.fsencode(destination), 1):
		code = ctypes.get_errno()
		raise OSError(code, os.strerror(code), str(destination))


def compose(args):
	image = args.image.absolute()
	output = args.output.absolute()
	driver = args.driver.absolute()
	if args.product not in ("source", "object", "archive"):
		raise CommandFailure(4)
	if os.path.lexists(output):
		raise FileExistsError(errno.EEXIST, "output already exists", str(output))
	image_before = digest(image)
	driver_hash = digest(driver)
	commands = []
	package = runpy.run_path(str(pathlib.Path(__file__).resolve().parent.parent / "acc_frame/pack.py"))["package"]
	with tempfile.TemporaryDirectory(prefix=".acc-command-", dir=output.parent) as temporary:
		stage = pathlib.Path(temporary)
		bodies = stage / "bodies"
		bodies.mkdir()
		run([str(driver), str(image), str(bodies), args.successor, str(args.steps)], commands)
		product = stage / "product"
		package(bodies, bodies / "transport.inc", product)
		if args.product != "source":
			run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", *args.cflag,
				"-c", str(product / "component.c"), "-o", str(product / "component.o")], commands)
		if args.product == "archive":
			run([args.ar, "rcs", str(product / "library.a"), str(product / "component.o")], commands)
		if digest(image) != image_before:
			raise OSError(errno.EIO, "input image changed during composition")
		receipt = {"scope": "fixed actual Acc mockup from admitted saved image; not general native lowering",
			"artifact_sha256": image_before, "driver_sha256": driver_hash,
			"successor": args.successor, "steps_per_selector": args.steps,
			"product": args.product, "commands": commands,
			"generated_inputs": {p.name: digest(p) for p in sorted(bodies.iterdir())},
			"outputs": {p.name: digest(p) for p in sorted(product.iterdir())},
			"limits": ["manual role/action/storage/array interpretation; complete checked Scope unproved",
				"Nat32/depth256/node65536/borrowed immutable lifetime/nonoverlap/transactional output",
				"ordinary native Acc refusal remains; no producer/schema/checker/erasure/ABI extension"]}
		(product / "product.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
		publish_directory(product, output)


def main():
	parser = argparse.ArgumentParser(description="Build the fixed actual Acc C mockup from its admitted saved image.")
	parser.add_argument("image", type=pathlib.Path)
	parser.add_argument("output", type=pathlib.Path)
	parser.add_argument("--driver", type=pathlib.Path, required=True)
	parser.add_argument("--product", default="source")
	parser.add_argument("--successor", default="succ_access")
	parser.add_argument("--steps", type=int, default=5000000)
	parser.add_argument("--cc", default="cc")
	parser.add_argument("--ar", default="ar")
	parser.add_argument("--cflag", action="append", default=[])
	args = parser.parse_args()
	if args.steps < 0 or args.steps > (1 << 64) - 1:
		parser.error("steps must be an unsigned 64-bit integer")
	try:
		compose(args)
	except CommandFailure as error:
		return error.status
	except ValueError as error:
		print(str(error), file=sys.stderr)
		return 4
	except OSError as error:
		print(str(error), file=sys.stderr)
		return 2
	return 0


if __name__ == "__main__":
	sys.exit(main())
