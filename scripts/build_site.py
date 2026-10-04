"""Package only the static website and the already published archived log."""
from pathlib import Path
import hashlib
import shutil

root = Path(__file__).resolve().parents[1]
output = root / "build-pages"
output.mkdir(exist_ok=True)
for name in ("index.html", "style.css", "app.js", "favicon.svg"):
    shutil.copyfile(root / "site" / name, output / name)
(output / "logs").mkdir(exist_ok=True)
original = root / "docs/logs/daheng-2026-10-04.log"
archived = output / "logs/daheng-2026-10-04.log"
shutil.copyfile(original, archived)
assert hashlib.sha256(original.read_bytes()).digest() == hashlib.sha256(archived.read_bytes()).digest()
(output / ".nojekyll").touch()
print(f"Static website ready: {output}")
