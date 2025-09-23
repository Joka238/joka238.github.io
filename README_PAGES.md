# Telepaint Bridge auf GitHub Pages

Zwei Optionen:

## A) Branch `gh-pages` (empfohlen)
1. Im Repo `joka238/telepaint-bridge` diesen Inhalt in die Root ausrollen.
2. In den Repo‑Settings → Pages: Source = `Deploy from a branch`, Branch = `gh-pages` / `/ (root)`.
3. Pushen, fertig. Seite liegt unter `https://joka238.github.io/telepaint-bridge/`.

## B) Ordner `docs/` auf `main`
1. Diesen Inhalt in den Ordner `docs/` legen (so dass `docs/index.html` existiert).
2. In den Repo‑Settings → Pages: Source = `Deploy from a branch`, Branch = `main` / `/docs`.
3. Pushen.

**Wichtig**
- `.nojekyll` liegt bei, damit GitHub nichts wegfiltert.
- Es gibt bewusst **keine** Skripte und **keine** Datenreferenzen. Die Seite ist nur Host/Brücke.
- `verify.sh` kann lokal sicherstellen, dass keine Skripte oder `data/*`-Referenzen eingeschlichen sind.
