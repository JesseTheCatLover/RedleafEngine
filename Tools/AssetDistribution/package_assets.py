from pathlib import Path
import argparse
import hashlib
import json
import sys


# --- Paths ---

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]

CONFIG_PATH = SCRIPT_DIR / "assets.json"
MANIFEST_PATH = SCRIPT_DIR / "manifest.json"

SOURCE_ASSETS_DIR = PROJECT_ROOT / "SourceAssets"
ASSETS_DIR = PROJECT_ROOT / "Assets"


# --- Configuration ---

IGNORED_FILES = {
    ".DS_Store",
    "Thumbs.db",
    "desktop.ini",
}


# --- Configuration loading ---

def load_config() -> dict:
    try:
        with CONFIG_PATH.open("r", encoding="utf-8") as file:
            config = json.load(file)
    except FileNotFoundError:
        print(f"ERROR: Configuration file not found: {CONFIG_PATH}")
        sys.exit(1)
    except json.JSONDecodeError as error:
        print(f"ERROR: Invalid JSON in {CONFIG_PATH}: {error}")
        sys.exit(1)

    if not isinstance(config, dict):
        print("ERROR: Configuration root must be an object.")
        sys.exit(1)

    if "format_version" not in config:
        print("ERROR: Configuration is missing 'format_version'.")
        sys.exit(1)

    if config["format_version"] != 1:
        print(
            f"ERROR: Unsupported configuration format version: "
            f"{config['format_version']}"
        )
        sys.exit(1)

    raw_assets = config.get("raw_assets", [])

    if not isinstance(raw_assets, list):
        print("ERROR: 'raw_assets' must be an array.")
        sys.exit(1)

    for raw_asset in raw_assets:
        if not isinstance(raw_asset, str):
            print("ERROR: Every entry in 'raw_assets' must be a string.")
            sys.exit(1)

    return config


# --- File discovery ---

def collect_files(directory: Path) -> list[Path]:
    if not directory.exists():
        return []

    if not directory.is_dir():
        print(f"ERROR: Expected directory: {directory}")
        sys.exit(1)

    return sorted(
        path
        for path in directory.rglob("*")
        if path.is_file() and path.name not in IGNORED_FILES
    )


def collect_raw_assets(config: dict) -> list[Path]:
    raw_files: list[Path] = []

    for raw_asset in config.get("raw_assets", []):
        raw_path = ASSETS_DIR / raw_asset

        if not raw_path.exists():
            print(f"ERROR: Raw asset path does not exist: {raw_path}")
            sys.exit(1)

        if raw_path.is_file():
            if raw_path.name not in IGNORED_FILES:
                raw_files.append(raw_path)

            continue

        raw_files.extend(collect_files(raw_path))

    return sorted(raw_files)


# --- Asset metadata ---

def calculate_sha256(path: Path) -> str:
    sha256 = hashlib.sha256()

    with path.open("rb") as file:
        while chunk := file.read(1024 * 1024):
            sha256.update(chunk)

    return sha256.hexdigest()


def get_package_path(path: Path) -> str:
    relative_path = path.relative_to(PROJECT_ROOT)

    if relative_path.parts[0] == "SourceAssets":
        package_path = relative_path

    elif relative_path.parts[0] == "Assets":
        package_path = Path("RawAssets") / relative_path.relative_to("Assets")

    else:
        raise ValueError(f"Unsupported asset path: {path}")

    return package_path.as_posix()


# --- Manifest generation ---

def build_manifest(
        source_files: list[Path],
        raw_files: list[Path],
        package_version: str,
) -> dict:
    files = {}

    for path in source_files + raw_files:
        package_path = get_package_path(path)

        files[package_path] = {
            "sha256": calculate_sha256(path),
            "size": path.stat().st_size,
        }

    return {
        "format_version": 1,
        "package_version": package_version,
        "files": dict(sorted(files.items())),
    }


def write_manifest(manifest: dict) -> None:
    with MANIFEST_PATH.open("w", encoding="utf-8") as file:
        json.dump(
            manifest,
            file,
            indent=4,
            ensure_ascii=False,
        )
        file.write("\n")


# --- Command line ---

def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate the RedleafEngine asset distribution manifest."
    )

    parser.add_argument(
        "--version",
        required=True,
        help="Asset package version.",
    )

    return parser.parse_args()


# --- Output ---

def print_summary(
        source_files: list[Path],
        raw_files: list[Path],
) -> None:
    print("Redleaf Asset Package")
    print("=====================")
    print()

    print("Source Assets:")

    if source_files:
        for path in source_files:
            relative_path = path.relative_to(PROJECT_ROOT)
            size = path.stat().st_size

            print(
                f"  {relative_path} "
                f"({size:,} bytes)"
            )
    else:
        print("  <none>")

    print()
    print("Raw Assets:")

    if raw_files:
        for path in raw_files:
            relative_path = path.relative_to(PROJECT_ROOT)
            size = path.stat().st_size

            print(
                f"  {relative_path} "
                f"({size:,} bytes)"
            )
    else:
        print("  <none>")

    print()
    print(f"Total source files: {len(source_files)}")
    print(f"Total raw files:    {len(raw_files)}")
    print(f"Total files:        {len(source_files) + len(raw_files)}")


# --- Main ---

def main() -> None:
    args = parse_arguments()
    config = load_config()

    source_files = collect_files(SOURCE_ASSETS_DIR)
    raw_files = collect_raw_assets(config)

    print_summary(source_files, raw_files)

    manifest = build_manifest(
        source_files,
        raw_files,
        args.version,
    )

    write_manifest(manifest)

    print()
    print(f"Manifest written to: {MANIFEST_PATH}")


if __name__ == "__main__":
    main()