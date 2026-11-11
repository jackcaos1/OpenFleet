#!/usr/bin/env python3
"""Prueft die Schichtentrennung: engine/src darf keinen Plattformcode enthalten (siehe CLAUDE.md, Abschnitt Plattformen).

  python3 -I tools/check_layering.py [wurzel]

Erlaubt im Kern: C++-Standardbibliothek und OpenGL ES (ueber render/Gl.h). Alles andere (Android, SDL, JNI, Windows, POSIX-Systemaufrufe,
Emscripten) gehoert in die Plattformschichten (desktop/, android/, spaeter web/).
"""
import os
import re
import sys

FORBIDDEN = [
    (r"#\s*include\s*[<\"](android|jni|SDL|windows|unistd|sys/|EGL/|emscripten|pthread|dlfcn)", "Plattform-Header"),
    (r"__ANDROID__|_WIN32|__linux__|__APPLE__|__EMSCRIPTEN__", "Plattform-#ifdef"),
]
GL_ONLY = re.compile(r"#\s*include\s*[<\"](GLES|GL/|OpenGL)")
GL_ALLOWED = {"render/Gl.h"}


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    src = os.path.join(root, "engine", "src")
    bad = 0
    for d, _, files in os.walk(src):
        for f in files:
            if not f.endswith((".h", ".cpp")):
                continue
            path = os.path.join(d, f)
            rel = os.path.relpath(path, src).replace(os.sep, "/")
            for n, line in enumerate(open(path, encoding="utf-8"), 1):
                for pat, what in FORBIDDEN:
                    if re.search(pat, line):
                        print("%s:%d: %s im Kern: %s" % (rel, n, what, line.strip()))
                        bad += 1
                if GL_ONLY.search(line) and rel not in GL_ALLOWED:
                    print("%s:%d: GL-Header nur ueber render/Gl.h einbinden: %s" % (rel, n, line.strip()))
                    bad += 1
    print("Schichtenpruefung: %d Verstoesse" % bad)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
