import hashlib
import pathlib
import runpy
import tempfile


def package(bodies, descriptor, output):
	here = pathlib.Path(__file__).resolve().parent.parent
	table = descriptor.read_bytes()
	if hashlib.sha256(table).hexdigest() != "f8954c46bf74754207074cb92c16c3cc8eb5207d99cf6a46920390181300eac8":
		raise ValueError("unqualified actual Acc constructor creation recipe")
	prefixes = {
		"action.inc": (table[:2592], "591fa73f5abcd1b5257e7ee3c271672452fa63c93c9b888aa9d1063445c8e6d0"),
		"index.inc": (table[:2913], "96dd3c3e016cdf2594d02b97a6d6f07e09a2e46561f4e0a7c32b69e2211ff173"),
	}
	frame = table.split(b"/* Actual admitted Acc constructor/down classifier:")[0]
	if hashlib.sha256(frame).hexdigest() != "888dbff17e481d65fb071612fce4f05b24853843623a804db76c6f918e3ec0de":
		raise ValueError("unqualified actual Acc frame prefix")
	for data, expected in prefixes.values():
		if hashlib.sha256(data).hexdigest() != expected:
			raise ValueError("unqualified actual Acc creation prefix")
	# Fresh algorithm bodies belong to this invocation. Historical modules only
	# qualify each finite target stage; none is read as an algorithm input.
	with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
		stage = pathlib.Path(temporary)
		transport = stage / "frame.inc"
		transport.write_bytes(frame)
		for name, (data, _) in prefixes.items():
			(stage / name).write_bytes(data)
		framed = stage / "framed"
		action = stage / "action"
		indexed = stage / "indexed"
		runpy.run_path(str(here / "acc_frame/pack.py"))["package"](bodies, transport, framed)
		runpy.run_path(str(here / "acc_recipe/pack.py"))["package"](framed, stage / "action.inc", action)
		runpy.run_path(str(here / "acc_indices/pack.py"))["package"](action, stage / "index.inc", indexed)
		runpy.run_path(str(here / "acc_create/pack.py"))["package"](indexed, descriptor, output)
