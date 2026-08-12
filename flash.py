#!/usr/bin/env python3
"""Build/upload/upload-fs helper for the home_automation ESP8266 firmware.

Uses arduino-cli under the hood. Board settings (FQBN, port, board options,
build properties) are read from a TOML config file so the common upload
invocation doesn't require repeating a long flag list on every call.

Subcommands:
    compile      build the sketch, do not upload
    upload        compile and upload firmware
    upload-fs     build a SPIFFS image from the given files/dirs and upload it
    inspect-fs    build the SPIFFS image and unpack it so contents can be seen

The firmware uses SPIFFS (not LittleFS), so the image is built with `mkspiffs`
matching the core's bundled tool. The script resolves the effective upload
recipe + flash layout by running `arduino-cli board details ... --show-properties`
with the active board options, so upload-fs picks the correct FS offset
automatically.
"""

from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import tomllib  # py >=3.11
from pathlib import Path
from typing import Any, Iterable


# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------


class Config:
    """Parsed flash.toml. Stores the three sections we care about."""

    def __init__(self, data: dict[str, Any]):
        self.fqbn: str = data["build"]["fqbn"]
        self.port: str | None = data["build"].get("port")
        self.compile_board_options: dict[str, str] = (
            data.get("build", {}).get("board_options", {}) or {}
        )
        self.compile_build_properties: dict[str, str] = (
            data.get("build", {}).get("build_properties", {}) or {}
        )
        self.upload_board_options: dict[str, str] = (
            data.get("upload", {}).get("board_options", {})
            or self.compile_board_options
        )
        self.upload_build_properties: dict[str, str] = (
            data.get("upload", {}).get("build_properties", {})
            or self.compile_build_properties
        )
        self.verify: bool = data.get("upload", {}).get("verify", False)
        ufs = data.get("upload_fs") or {}
        self.uploadfs_board_options: dict[str, str] = (
            ufs.get("board_options") or self.upload_board_options
        )
        self.uploadfs_build_properties: dict[str, str] = (
            ufs.get("build_properties") or self.upload_build_properties
        )
        self.mkspiffs_path: str | None = (data.get("mkspiffs") or {}).get("path")
        self.port_override: str | None = None

    @classmethod
    def load(cls, path: Path) -> "Config":
        with path.open("rb") as f:
            data = tomllib.load(f)
        if "build" not in data or "fqbn" not in data["build"]:
            raise SystemExit(f"config {path}: missing [build].fqbn")
        return cls(data)


# ---------------------------------------------------------------------------
# arduino-cli wrappers
# ---------------------------------------------------------------------------


def _board_options_arg(opts: dict[str, str]) -> list[str]:
    if not opts:
        return []
    return ["--board-options", ",".join(f"{k}={v}" for k, v in opts.items())]


def _build_property_args(props: dict[str, str]) -> list[str]:
    args: list[str] = []
    for k, v in props.items():
        args.append("--build-property")
        args.append(f"{k}={v}")
    return args


def run(cmd: list[str], *, dry: bool = False, check: bool = True) -> int:
    if dry:
        print("+", " ".join(_shquote(c) for c in cmd))
        return 0
    print("+", " ".join(_shquote(c) for c in cmd), file=sys.stderr)
    return subprocess.run(cmd, check=check).returncode


def _shquote(s: str) -> str:
    if not s or any(ch in s for ch in " \t\"'\\$`"):
        return "'" + s.replace("'", "'\\''") + "'"
    return s


def arduino_cli_compile(
    cfg: Config,
    sketch: Path,
    *,
    board_options: dict[str, str],
    build_properties: dict[str, str],
    export_dir: Path | None = None,
    dry: bool = False,
) -> int:
    cmd = ["arduino-cli", "compile", "--fqbn", cfg.fqbn]
    cmd += _board_options_arg(board_options)
    cmd += _build_property_args(build_properties)
    if export_dir is not None:
        cmd += ["--output-dir", str(export_dir)]
    cmd += [str(sketch)]
    return run(cmd, dry=dry)


def arduino_cli_upload(
    cfg: Config,
    sketch: Path,
    *,
    board_options: dict[str, str],
    build_properties: dict[str, str],
    dry: bool = False,
) -> int:
    # compile + upload in a single arduino-cli invocation
    cmd = ["arduino-cli", "compile", "--fqbn", cfg.fqbn, "--upload"]
    cmd += _board_options_arg(board_options)
    cmd += _build_property_args(build_properties)
    if cfg.port:
        cmd += ["--port", cfg.port]
    if cfg.verify:
        cmd.append("--verify")
    cmd += [str(sketch)]
    return run(cmd, dry=dry)


# ---------------------------------------------------------------------------
# board property lookup
# ---------------------------------------------------------------------------


def get_board_properties(
    fqbn: str,
    board_options: dict[str, str],
    *,
    extra: dict[str, str] | None = None,
) -> dict[str, str]:
    """Run `arduino-cli board details --show-properties` and return a flat
    property map. Uses the *expanded* form so menu selections are already
    applied; `extra` fills runtime placeholders arduino-cli can't know
    (e.g. serial.port, build.path)."""
    fqbn_full = fqbn
    if board_options:
        fqbn_full = fqbn + ":" + ",".join(f"{k}={v}" for k, v in board_options.items())
    res = subprocess.run(
        [
            "arduino-cli",
            "board",
            "details",
            "-b",
            fqbn_full,
            "--show-properties=expanded",
            "--json",
        ],
        capture_output=True,
        text=True,
    )
    if res.returncode != 0:
        sys.stderr.write(res.stderr)
        raise SystemExit(f"arduino-cli board details failed (rc={res.returncode})")
    data = json.loads(res.stdout)
    props: dict[str, str] = {}
    for line in data.get("build_properties", []) or []:
        if "=" not in line:
            continue
        k, v = line.split("=", 1)
        props[k] = v
    if extra:
        for k, v in extra.items():
            props[k] = v
    return props


def fs_layout(props: dict[str, str]) -> dict[str, int]:
    """Extract the LittleFS flash layout from board properties."""
    try:
        start = _parse_int(props["build.spiffs_start"])
        end = _parse_int(props["build.spiffs_end"])
        block = _parse_int(props["build.spiffs_blocksize"])
        page = _parse_int(props["build.spiffs_pagesize"])
    except KeyError as e:
        raise SystemExit(
            f"board properties missing {e}. "
            f"Pick a Flash Size board option that reserves space for a filesystem "
            f"(e.g. eesz=4M2M), not 'FS:none'."
        )
    except ValueError:
        raise SystemExit(
            "board reserves no space for a filesystem (spiffs_start is empty). "
            "Pick a Flash Size board option with FS space, e.g. eesz=4M2M."
        )
    if end <= start:
        raise SystemExit(
            f"filesystem partition is empty (start=0x{start:X} end=0x{end:X}). "
            f"Pick a Flash Size board option with FS space, e.g. eesz=4M2M."
        )
    return {"start": start, "end": end, "block": block, "page": page}


def _parse_int(s: str) -> int:
    return int(s, 0)  # understands 0x... and decimal


def _resolve_key(props: dict[str, str], key: str, context_prefix: str) -> str | None:
    """Resolve a {placeholder} the way arduino-cli does: try the bare key
    first, then try it under the tool namespace derived from the context.

    `tools.esptool.upload.pattern` referencing `{cmd}` resolves to
    `tools.esptool.cmd`: the namespace is `tools.<toolname>.`, i.e. the first
    three dotted segments of the context key.
    """
    if key in props:
        return props[key]
    if context_prefix and context_prefix.startswith("tools."):
        parts = context_prefix.split(".")
        # tools.<toolname>.<recipe-group>.<field> -> tools.<toolname>.
        if len(parts) >= 3:
            ns = parts[0] + "." + parts[1] + "."
            prefixed = ns + key
            if prefixed in props:
                return props[prefixed]
    return None


def _expand_recipe(props: dict[str, str], recipe: str, *, context_key: str = "") -> str:
    """Best-effort {property} substitution matching arduino-cli's expansion,
    including the short-form `{cmd}`-style references scoped to the recipe's
    own property prefix."""
    out = recipe
    for _ in range(10):
        new = ""
        i = 0
        changed = False
        while i < len(out):
            ch = out[i]
            if ch == "{" and i + 1 < len(out):
                j = out.find("}", i + 1)
                if j == -1:
                    new += out[i:]
                    break
                key = out[i + 1 : j]
                val = _resolve_key(props, key, context_key)
                if val is not None:
                    new += val
                    changed = True
                else:
                    new += "{" + key + "}"
                i = j + 1
            else:
                new += ch
                i += 1
        out = new
        if not changed:
            break
    return out


def upload_recipe(props: dict[str, str]) -> str:
    return props.get("tools.esptool.upload.pattern", "")


def upload_prebuilt_firmware(cfg: Config, image: Path, *, dry: bool = False) -> int:
    """Upload a pre-built firmware .bin using esptool via the core's upload.py."""
    port = cfg.port_override or cfg.port
    extra = {
        "serial.port": port or "",
        "build.path": str(image.parent),
        "build.project_name": image.stem,
        "upload.verbose": "",
        "upload.erase_cmd": "",
    }
    props = get_board_properties(cfg.fqbn, cfg.upload_board_options, extra=extra)
    recipe = upload_recipe(props)
    expanded = _expand_recipe(props, recipe, context_key="tools.esptool.upload.pattern")
    tokens = [t for t in shlex.split(expanded) if t]
    if "write_flash" not in tokens:
        raise SystemExit(
            "could not find `write_flash` in the expanded upload recipe:\n  " + expanded
        )
    idx = tokens.index("write_flash")
    head = tokens[:idx]
    # Use --before no_reset so esptool does not toggle DTR/RTS (not wired).
    # Flash mode is entered manually via GPIO0 + RST before upload.
    for i, t in enumerate(head):
        if t == "--before":
            head[i + 1] = "no_reset"
        elif t == "--after":
            head[i + 1] = "no_reset"
        elif t == "--baud":
            head[i + 1] = "115200"
    tail = ["write_flash", "0x0", str(image)]
    cmd = head + tail
    return run(cmd, dry=dry)


def upload_prebuilt_fs(cfg: Config, image: Path, *, dry: bool = False) -> int:
    """Upload a pre-built SPIFFS image using esptool."""
    port = cfg.port_override or cfg.port
    extra = {
        "serial.port": port or "",
        "build.path": str(image.parent),
        "build.project_name": image.stem,
        "upload.verbose": "",
        "upload.erase_cmd": "",
    }
    props = get_board_properties(cfg.fqbn, cfg.uploadfs_board_options, extra=extra)
    layout = fs_layout(props)
    return _esptool_upload_image(props, layout, image, dry=dry)


# ---------------------------------------------------------------------------
# upload-fs
# ---------------------------------------------------------------------------


def find_mkspiffs(hint: str | None) -> str:
    if hint:
        if not Path(hint).exists():
            raise SystemExit(f"mkspiffs not found at configured path: {hint}")
        return hint
    # Prefer the core's bundled mkspiffs: its SPIFFS on-disk version must
    # match the firmware's SPIFFS library, and only the core's copy is
    # guaranteed to. Fall back to PATH if the core tool isn't installed.
    core = _find_core_tool("mkspiffs")
    if core:
        return core
    p = shutil.which("mkspiffs")
    if p:
        return p
    raise SystemExit(
        "mkspiffs not found. Set [mkspiffs].path in the config, install "
        "the ESP8266 core via `arduino-cli core install esp8266:esp8266`, "
        "or put mkspiffs on PATH."
    )


def _find_core_tool(name: str) -> str | None:
    """Locate a tool bundled with the installed esp8266 core."""
    res = subprocess.run(
        ["arduino-cli", "config", "get", "directories.data", "--json"],
        capture_output=True,
        text=True,
    )
    if res.returncode != 0:
        return None
    try:
        data_dir = Path(json.loads(res.stdout).strip('"'))
    except (json.JSONDecodeError, ValueError):
        return None
    base = data_dir / "packages" / "esp8266" / "tools" / name
    if not base.is_dir():
        return None
    for v in sorted(base.iterdir(), reverse=True):
        cand = v / name
        if cand.exists() and os.access(cand, os.X_OK):
            return str(cand)
    return None


def collect_files(inputs: Iterable[Path], staging: Path) -> None:
    """Copy inputs (files or directory trees) into `staging` flat-rooted.
    Directory contents are merged at the root; files keep their basename."""
    staging.mkdir(parents=True, exist_ok=True)
    for inp in inputs:
        inp = inp.resolve()
        if inp.is_dir():
            for child in inp.rglob("*"):
                if child.is_file():
                    rel = child.relative_to(inp)
                    dst = staging / rel
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(child, dst)
        elif inp.is_file():
            shutil.copy2(inp, staging / inp.name)
        else:
            raise SystemExit(f"upload-fs: not a file or directory: {inp}")


def build_spiffs_image(
    mkspiffs: str,
    staging: Path,
    image: Path,
    layout: dict[str, int],
    *,
    dry: bool = False,
) -> int:
    size = layout["end"] - layout["start"]
    cmd = [
        mkspiffs,
        "-c",
        str(staging),
        "-b",
        str(layout["block"]),
        "-p",
        str(layout["page"]),
        "-s",
        str(size),
        str(image),
    ]
    return run(cmd, dry=dry, check=False)


def upload_fs(
    cfg: Config,
    inputs: list[Path],
    *,
    dry: bool = False,
) -> int:
    mkspiffs = find_mkspiffs(cfg.mkspiffs_path)

    with tempfile.TemporaryDirectory(prefix="flash-fs-") as tmp:
        tmpdir = Path(tmp)
        staging = tmpdir / "staging"
        image = tmpdir / "fs.spiffs.bin"

        # Provide the runtime values arduino-cli can't fill in for a bare
        # `board details` call (no actual compile/port context).
        extra = {
            "serial.port": cfg.port_override or cfg.port or "",
            "build.path": str(image.parent),
            "build.project_name": image.stem,
            "upload.verbose": "",
            "upload.erase_cmd": "",
        }
        props = get_board_properties(cfg.fqbn, cfg.uploadfs_board_options, extra=extra)
        layout = fs_layout(props)
        print(
            f"FS layout: start=0x{layout['start']:X} end=0x{layout['end']:X} "
            f"size={layout['end'] - layout['start']} block={layout['block']} page={layout['page']}",
            file=sys.stderr,
        )

        collect_files(inputs, staging)
        rc = build_spiffs_image(mkspiffs, staging, image, layout, dry=dry)
        if rc != 0:
            return rc

        return _esptool_upload_image(props, layout, image, dry=dry)


def _esptool_upload_image(
    props: dict[str, str],
    layout: dict[str, int],
    image: Path,
    *,
    dry: bool = False,
) -> int:
    """Invoke esptool via the core's upload.py with the FS image + address.

    We expand the core's `tools.esptool.upload.pattern` ourselves so we can
    swap the `write_flash 0x0 <bin>` tail for `write_flash <start> <image>`.
    This reuses the exact same esptool/python3/upload.py the firmware upload
    uses (same baud, reset method, port), only the address + file differ.
    """
    recipe = upload_recipe(props)
    expanded = _expand_recipe(props, recipe, context_key="tools.esptool.upload.pattern")
    tokens = [t for t in shlex.split(expanded) if t]
    if "write_flash" not in tokens:
        raise SystemExit(
            "could not find `write_flash` in the expanded upload recipe:\n  " + expanded
        )
    idx = tokens.index("write_flash")
    head = tokens[:idx]  # esptool/upload.py invocation + flags
    # Use --before/--after no_reset so esptool does not toggle DTR/RTS.
    # Flash mode is entered manually via GPIO0 + RST before upload.
    for i, t in enumerate(head):
        if t == "--before":
            head[i + 1] = "no_reset"
        elif t == "--after":
            head[i + 1] = "no_reset"
        elif t == "--baud":
            head[i + 1] = "115200"
    tail = ["write_flash", f"0x{layout['start']:X}", str(image)]
    cmd = head + tail
    return run(cmd, dry=dry)


def inspect_fs(
    cfg: Config,
    inputs: list[Path],
    *,
    outdir: Path | None = None,
    dry: bool = False,
) -> int:
    """Build the SPIFFS image, then unpack it back to a directory so the
    contents can be inspected. With -o, write the image + unpacked tree there
    and leave them for manual inspection; otherwise use a temp dir and print
    the file listing."""
    mkspiffs = find_mkspiffs(cfg.mkspiffs_path)

    if outdir is not None:
        outdir.mkdir(parents=True, exist_ok=True)
        staging = outdir / "staging"
        image = outdir / "fs.spiffs.bin"
        unpack = outdir / "unpacked"
        keep = True
    else:
        tmp = tempfile.mkdtemp(prefix="flash-inspect-")
        tmpdir = Path(tmp)
        staging = tmpdir / "staging"
        image = tmpdir / "fs.spiffs.bin"
        unpack = tmpdir / "unpacked"
        keep = False

    try:
        extra = {
            "serial.port": cfg.port_override or cfg.port or "",
            "build.path": str(image.parent),
            "build.project_name": image.stem,
            "upload.verbose": "",
            "upload.erase_cmd": "",
        }
        props = get_board_properties(cfg.fqbn, cfg.uploadfs_board_options, extra=extra)
        layout = fs_layout(props)
        print(
            f"FS layout: start=0x{layout['start']:X} end=0x{layout['end']:X} "
            f"size={layout['end'] - layout['start']} block={layout['block']} page={layout['page']}",
            file=sys.stderr,
        )

        collect_files(inputs, staging)
        rc = build_spiffs_image(mkspiffs, staging, image, layout, dry=dry)
        if rc != 0:
            return rc

        # Unpack the freshly built image so we can see what actually landed.
        unpack.mkdir(parents=True, exist_ok=True)
        rc = run([mkspiffs, "-u", str(unpack), str(image)], dry=dry, check=False)
        if rc != 0:
            print(
                "warning: mkspiffs could not unpack the image; "
                "the image may still be valid for the device.",
                file=sys.stderr,
            )

        print(f"\nImage:  {image}", file=sys.stderr)
        print(f"Staged: {staging}", file=sys.stderr)
        print(f"Unpacked: {unpack}", file=sys.stderr)
        print("---- files staged ----", file=sys.stderr)
        _tree(staging)
        if unpack.exists() and any(unpack.iterdir()):
            print("---- files unpacked from image ----", file=sys.stderr)
            _tree(unpack)
        return 0
    finally:
        if not keep:
            shutil.rmtree(staging.parent, ignore_errors=True)


def _tree(root: Path) -> None:
    for p in sorted(root.rglob("*")):
        if p.is_file():
            rel = p.relative_to(root)
            print(f"  {p.stat().st_size:>8}  /{rel}", file=sys.stderr)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="flash.py",
        description="Build/upload/upload-fs helper for the ESP8266 firmware.",
    )
    p.add_argument(
        "-c",
        "--config",
        type=Path,
        default=Path("flash.toml"),
        help="path to the flash config file (default: flash.toml)",
    )
    p.add_argument(
        "-s",
        "--sketch",
        type=Path,
        default=None,
        help="sketch directory (default: project root containing this script)",
    )
    p.add_argument(
        "-n",
        "--dry-run",
        action="store_true",
        help="print commands instead of running them",
    )
    p.add_argument(
        "-p",
        "--port",
        type=str,
        default=None,
        help="serial port override (e.g. /dev/serial0)",
    )
    sub = p.add_subparsers(dest="command", required=True)

    sub.add_parser("compile", help="compile but do not upload")
    sub_upload = sub.add_parser("upload", help="compile and upload firmware")
    sub_upload.add_argument(
        "--image",
        type=Path,
        default=None,
        help="pre-built firmware .bin to upload (skip compile)",
    )

    ufs = sub.add_parser(
        "upload-fs",
        help="build a LittleFS image from the given files/dirs and upload it",
    )
    ufs.add_argument(
        "inputs",
        nargs="*",
        type=Path,
        help="files or directories to pack into the filesystem image",
    )
    ufs.add_argument(
        "--image",
        type=Path,
        default=None,
        help="pre-built SPIFFS image to upload (skip build)",
    )

    ifs = sub.add_parser(
        "inspect-fs",
        help="build the LittleFS image and unpack it so contents can be inspected",
    )
    ifs.add_argument(
        "inputs",
        nargs="+",
        type=Path,
        help="files or directories to pack into the filesystem image",
    )
    ifs.add_argument(
        "-o",
        "--outdir",
        type=Path,
        default=None,
        help="keep the image + unpacked tree under this dir instead of a temp dir",
    )
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    cfg = Config.load(args.config)
    cfg.port_override = args.port
    sketch = args.sketch or (args.config.parent / "home_automation.ino").resolve()
    if not sketch.exists():
        raise SystemExit(f"sketch not found: {sketch}")

    if args.command == "compile":
        return arduino_cli_compile(
            cfg,
            sketch,
            board_options=cfg.compile_board_options,
            build_properties=cfg.compile_build_properties,
            dry=args.dry_run,
        )

    if args.command == "upload":
        if args.image:
            return upload_prebuilt_firmware(cfg, args.image, dry=args.dry_run)
        return arduino_cli_upload(
            cfg,
            sketch,
            board_options=cfg.upload_board_options,
            build_properties=cfg.upload_build_properties,
            dry=args.dry_run,
        )

    if args.command == "upload-fs":
        if args.image:
            return upload_prebuilt_fs(cfg, args.image, dry=args.dry_run)
        if not args.inputs:
            raise SystemExit("upload-fs: inputs required unless --image is given")
        return upload_fs(cfg, args.inputs, dry=args.dry_run)

    if args.command == "inspect-fs":
        return inspect_fs(cfg, args.inputs, outdir=args.outdir, dry=args.dry_run)

    raise SystemExit(f"unknown command: {args.command}")


if __name__ == "__main__":
    sys.exit(main())
