#!/usr/bin/env python3
"""
gen_c_cpp_properties.py
arduino-cli のビルド結果 (compile_commands.json) から VS Code IntelliSense 用の
.vscode/c_cpp_properties.json を生成する。

使い方: 一度コンパイルしてから実行（コア/ライブラリを更新した時も再実行）
    python3 .vscode/gen_c_cpp_properties.py

生成ファイルには個人のパスを含めない:
  $HOME 配下        -> ${env:HOME}
  スケッチの親フォルダ -> ${workspaceFolder}/..
"""
import json
import os
import shlex
import sys

HOME = os.path.expanduser("~")
VSCODE_DIR = os.path.dirname(os.path.abspath(__file__))
SKETCH_DIR = os.path.dirname(VSCODE_DIR)
SKETCH_NAME = os.path.basename(SKETCH_DIR)
SKETCHBOOK_DIR = os.path.dirname(SKETCH_DIR)
BUILD_DIR = os.path.join(HOME, "Library/Caches/arduino-build", SKETCH_NAME)
CCJSON = os.path.join(BUILD_DIR, "compile_commands.json")


def portable(path):
    path = os.path.normpath(path)
    if path == SKETCH_DIR or path.startswith(SKETCH_DIR + os.sep):
        return "${workspaceFolder}" + path[len(SKETCH_DIR):]
    if path.startswith(SKETCHBOOK_DIR + os.sep):
        return "${workspaceFolder}/.." + path[len(SKETCHBOOK_DIR):]
    if path.startswith(HOME + os.sep):
        return "${env:HOME}" + path[len(HOME):]
    return path


def expand_args(args):
    """@response-file を展開"""
    out = []
    for a in args:
        if a.startswith("@") and os.path.isfile(a[1:]):
            with open(a[1:], encoding="utf-8") as f:
                out.extend(expand_args(shlex.split(f.read())))
        else:
            out.append(a)
    return out


def main():
    if not os.path.isfile(CCJSON):
        sys.exit(f"compile_commands.json がありません。先にコンパイルしてください: {CCJSON}")
    with open(CCJSON, encoding="utf-8") as f:
        entries = json.load(f)
    entry = next((e for e in entries if e["file"].endswith(f"{SKETCH_NAME}.ino.cpp")), None)
    if entry is None:
        sys.exit("スケッチのコンパイルコマンドが見つかりません")
    args = entry.get("arguments") or shlex.split(entry["command"])
    compiler = args[0]
    args = expand_args(args[1:])

    includes, defines, cargs = [], [], []
    prefix = ""
    std = "gnu++17"
    i = 0
    while i < len(args):
        a = args[i]
        if a == "-I" or a == "-D":
            a = a + args[i + 1]
            i += 1
        if a.startswith("-iprefix"):
            prefix = a[len("-iprefix"):]
        elif a.startswith("-iwithprefixbefore"):
            includes.append(prefix.rstrip("/") + a[len("-iwithprefixbefore"):])
        elif a.startswith("-I"):
            p = a[2:]
            if not p.startswith(BUILD_DIR):  # ビルド一時フォルダは除外
                includes.append(p)
        elif a.startswith("-D"):
            defines.append(a[2:])
        elif a.startswith("-std="):
            std = a[5:]
        elif a.startswith(("-mcpu", "-mthumb", "-march", "-mfloat-abi", "-mcmse")):
            cargs.append(a)
        i += 1

    seen = set()
    inc = ["${workspaceFolder}"]
    for p in includes:
        p = portable(p)
        if p not in seen:
            seen.add(p)
            inc.append(p)

    config = {
        "configurations": [{
            "name": "Pico 2W (arduino-pico)",
            "compilerPath": portable(compiler),
            "compilerArgs": cargs,
            "intelliSenseMode": "gcc-arm",
            "cStandard": "gnu17",
            "cppStandard": std,
            "includePath": inc,
            "defines": defines,
            # .ino は Arduino.h を暗黙に include するので強制インクルード
            "forcedInclude": [
                next(p for p in inc if p.endswith("cores/rp2040")) + "/Arduino.h"],
        }],
        "version": 4,
    }
    out = os.path.join(VSCODE_DIR, "c_cpp_properties.json")
    with open(out, "w", encoding="utf-8") as f:
        f.write("// 自動生成: .vscode/gen_c_cpp_properties.py （手で編集しないでください）\n")
        json.dump(config, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print(f"generated: {out}  (includes={len(inc)}, defines={len(defines)})")


if __name__ == "__main__":
    main()
