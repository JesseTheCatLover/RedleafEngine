from pathlib import Path
import json
import sys


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]

CONFIG_PATH = SCRIPT_DIR / "assets.json"
SOURCE_ASSETS_DIR = PROJECT_ROOT / "SourceAssets"
ASSETS_DIR = PROJECT_ROOT / "Assets"

IGNORED_FILES = {
    ".DS_Store",
    "Thumbs.db",
    "desktop.ini",
}

def load_config() -> dict:
    try:
        with CONFIG_PATH.open("r", encoding="utf-8") as file:
            return json.load(file)
    except FileNotFoundError:
        print(f"ERROR: Configuration file not found: {CONFIG_PATH}")
        sys.exit(1)
    except json.JSONDecodeError as error:
        print(f"ERROR: Invalid JSON in {CONFIG_PATH}: {error}")
        sys.exit(1)


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


def main() -> None:
    config = load_config()

    source_files = collect_files(SOURCE_ASSETS_DIR)

    raw_files: list[Path] = []

    for raw_asset in config.get("raw_assets", []):
        raw_path = ASSETS_DIR / raw_asset

        if not raw_path.exists():
            print(f"WARNING: Raw asset path does not exist: {raw_path}")
            continue

        if raw_path.is_file():
            raw_files.append(raw_path)
            continue

        raw_files.extend(collect_files(raw_path))

    source_files = sorted(source_files)
    raw_files = sorted(raw_files)

    print("Redleaf Asset Package")
    print("=====================")
    print()

    print("Source Assets:")
    if source_files:
        for path in source_files:
            relative_path = path.relative_to(PROJECT_ROOT)
            size = path.stat().st_size
            print(f"  {relative_path} ({size:,} bytes)")
    else:
        print("  <none>")

    print()
    print("Raw Assets:")
    if raw_files:
        for path in raw_files:
            relative_path = path.relative_to(PROJECT_ROOT)
            size = path.stat().st_size
            print(f"  {relative_path} ({size:,} bytes)")
    else:
        print("  <none>")

    print()
    print(f"Total source files: {len(source_files)}")
    print(f"Total raw files:    {len(raw_files)}")
    print(f"Total files:        {len(source_files) + len(raw_files)}")


if __name__ == "__main__":
    main()