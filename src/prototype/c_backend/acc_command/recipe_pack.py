import hashlib
import pathlib
import runpy
import tempfile


def package(bodies, descriptor, output):
	here = pathlib.Path(__file__).resolve().parent.parent
	table = descriptor.read_bytes()
	if hashlib.sha256(table).hexdigest() != "591fa73f5abcd1b5257e7ee3c271672452fa63c93c9b888aa9d1063445c8e6d0":
		raise ValueError("unqualified actual Acc down/action recipe")
	prefix = table.split(b"/* Actual admitted Acc constructor/down classifier:")[0]
	if hashlib.sha256(prefix).hexdigest() != "888dbff17e481d65fb071612fce4f05b24853843623a804db76c6f918e3ec0de":
		raise ValueError("unqualified actual Acc frame prefix")
	# Recompose from this invocation's fresh seven bodies. Historical products
	# qualify bytes, but are not used as the command's algorithm input.
	with tempfile.TemporaryDirectory(dir=output.parent) as temporary:
		stage = pathlib.Path(temporary)
		transport = stage / "transport.inc"
		transport.write_bytes(prefix)
		parent = stage / "parent"
		runpy.run_path(str(here / "acc_frame/pack.py"))["package"](bodies, transport, parent)
		runpy.run_path(str(here / "acc_recipe/pack.py"))["package"](parent, descriptor, output)
