from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import sys


# --- Paths ---

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]

CONFIG_PATH = SCRIPT_DIR / "configuration.json"
CMAKE_PATH = PROJECT_ROOT / "CMakeLists.txt"

SOURCE_ASSETS_DIR = PROJECT_ROOT / "SourceAssets"
ASSETS_DIR = PROJECT_ROOT / "Assets"

DEFAULT_OUTPUT_DIR = (
        PROJECT_ROOT
        / "Distribution"
        / "Assets"
)


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

    if config.get("format_version") != 1:
        print(
            "ERROR: Unsupported configuration format version: "
            f"{config.get('format_version')}"
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


def load_asset_version() -> str:
    try:
        cmake_text = CMAKE_PATH.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"ERROR: CMakeLists.txt not found: {CMAKE_PATH}")
        sys.exit(1)

    match = re.search(
        r'^\s*set\s*\(\s*REDLEAF_ASSETS_VERSION\s+"([^"]+)"\s*\)',
        cmake_text,
        re.MULTILINE,
    )

    if match is None:
        print(
            "ERROR: REDLEAF_ASSETS_VERSION was not found "
            "in CMakeLists.txt."
        )
        sys.exit(1)

    version = match.group(1).strip()

    if not version:
        print("ERROR: REDLEAF_ASSETS_VERSION is empty.")
        sys.exit(1)

    return version


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

    return sorted(set(raw_files))


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
        package_path = (
                Path("RawAssets")
                / relative_path.relative_to("Assets")
        )

    else:
        raise ValueError(f"Unsupported asset path: {path}")

    return package_path.as_posix()


# --- Package construction ---

def collect_asset_entries(
        source_files: list[Path],
        raw_files: list[Path],
) -> list[dict]:
    entries = []
    package_paths: set[str] = set()

    for path in source_files + raw_files:
        package_path = get_package_path(path)

        if package_path in package_paths:
            print(
                "ERROR: Duplicate package path detected: "
                f"{package_path}"
            )
            sys.exit(1)

        package_paths.add(package_path)

        entries.append(
            {
                "package_path": package_path,
                "source_path": path,
                "sha256": calculate_sha256(path),
                "size": path.stat().st_size,
            }
        )

    return sorted(
        entries,
        key=lambda entry: entry["package_path"],
    )


def build_manifest(
        entries: list[dict],
        package_version: str,
) -> dict:
    files = {}

    for entry in entries:
        files[entry["package_path"]] = {
            "sha256": entry["sha256"],
            "size": entry["size"],
        }

    return {
        "format_version": 1,
        "package_version": package_version,
        "files": files,
    }


def write_manifest(
        manifest: dict,
        output_dir: Path,
) -> None:
    manifest_path = output_dir / "manifest.json"

    with manifest_path.open("w", encoding="utf-8") as file:
        json.dump(
            manifest,
            file,
            indent=4,
            ensure_ascii=False,
        )
        file.write("\n")


def stage_blobs(
        entries: list[dict],
        output_dir: Path,
) -> int:
    unique_hashes: set[str] = set()

    for entry in entries:
        sha256 = entry["sha256"]
        unique_hashes.add(sha256)

        blob_path = output_dir / f"blob-{sha256}"

        if blob_path.exists():
            continue

        shutil.copyfile(
            entry["source_path"],
            blob_path,
        )

    return len(unique_hashes)


# --- Command line ---

def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build a RedleafEngine asset distribution package."
    )

    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help=(
            "Output directory. Defaults to "
            "Distribution/Assets/<version>."
        ),
    )

    return parser.parse_args()


# --- Output ---

def print_summary(
        entries: list[dict],
        unique_blob_count: int,
        package_version: str,
        output_dir: Path,
) -> None:
    source_entries = [
        entry
        for entry in entries
        if entry["package_path"].startswith("SourceAssets/")
    ]

    raw_entries = [
        entry
        for entry in entries
        if entry["package_path"].startswith("RawAssets/")
    ]

    print("Redleaf Asset Package")
    print("=====================")
    print()
    print(f"Package version: {package_version}")
    print()

    print("Source Assets:")

    if source_entries:
        for entry in source_entries:
            print(
                f"  {entry['package_path']} "
                f"({entry['size']:,} bytes)"
            )
    else:
        print("  <none>")

    print()
    print("Raw Assets:")

    if raw_entries:
        for entry in raw_entries:
            print(
                f"  {entry['package_path']} "
                f"({entry['size']:,} bytes)"
            )
    else:
        print("  <none>")

    print()
    print(f"Total files:        {len(entries)}")
    print(f"Unique blobs:       {unique_blob_count}")
    print()
    print(f"Package written to: {output_dir}")


# --- Main ---

def main() -> None:
    args = parse_arguments()

    config = load_config()
    package_version = load_asset_version()

    source_files = collect_files(SOURCE_ASSETS_DIR)
    raw_files = collect_raw_assets(config)

    entries = collect_asset_entries(
        source_files,
        raw_files,
    )

    output_dir = (
        args.output
        if args.output is not None
        else DEFAULT_OUTPUT_DIR / package_version
    )

    output_dir = output_dir.resolve()
    output_dir.mkdir(
        parents=True,
        exist_ok=True,
    )

    manifest = build_manifest(
        entries,
        package_version,
    )

    write_manifest(
        manifest,
        output_dir,
    )

    unique_blob_count = stage_blobs(
        entries,
        output_dir,
    )

    print_summary(
        entries,
        unique_blob_count,
        package_version,
        output_dir,
    )


if __name__ == "__main__":
    main()