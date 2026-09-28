from pathlib import Path
import subprocess
import sys

from package_assets import (
    DEFAULT_OUTPUT_DIR,
    load_asset_version,
)


# --- Paths ---

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]

PACKAGE_SCRIPT = SCRIPT_DIR / "package_assets.py"

DISTRIBUTION_DIR = DEFAULT_OUTPUT_DIR


# --- Process helpers ---

def run_command(
        command: list[str],
        *,
        cwd: Path | None = None,
) -> None:
    print(f"$ {' '.join(command)}")

    try:
        subprocess.run(
            command,
            cwd=cwd,
            check=True,
        )
    except FileNotFoundError:
        print(f"ERROR: Command not found: {command[0]}")
        sys.exit(1)
    except subprocess.CalledProcessError as error:
        print(
            "ERROR: Command failed with exit code "
            f"{error.returncode}: {command[0]}"
        )
        sys.exit(error.returncode)


# --- GitHub ---

def verify_github_cli() -> None:
    run_command(["gh", "auth", "status"])


def release_exists(tag: str) -> bool:
    result = subprocess.run(
        ["gh", "release", "view", tag],
        cwd=PROJECT_ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    return result.returncode == 0


# --- Packaging ---

def build_package(package_version: str) -> Path:
    output_dir = DISTRIBUTION_DIR / package_version

    if output_dir.exists():
        print(
            "ERROR: Distribution package already exists:"
        )
        print(f"  {output_dir}")
        print()
        print(
            "Remove it manually if you intentionally want "
            "to rebuild this version."
        )
        sys.exit(1)

    run_command(
        [
            sys.executable,
            str(PACKAGE_SCRIPT),
        ],
        cwd=PROJECT_ROOT,
    )

    return output_dir


# --- Publishing ---

def publish_release(
        package_version: str,
        package_dir: Path,
) -> None:
    tag = f"assets-{package_version}"

    if release_exists(tag):
        print(
            f"ERROR: GitHub release already exists: {tag}"
        )
        sys.exit(1)

    manifest = package_dir / "manifest.json"

    blobs = sorted(
        package_dir.glob("blob-*")
    )

    if not manifest.exists():
        print("ERROR: manifest.json was not generated.")
        sys.exit(1)

    if not blobs:
        print("ERROR: No asset blobs were generated.")
        sys.exit(1)

    release_assets = [
        str(manifest),
        *(str(blob) for blob in blobs),
    ]

    run_command(
        [
            "gh",
            "release",
            "create",
            tag,
            *release_assets,
            "--title",
            f"RedleafEngine Assets {package_version}",
            "--notes",
            (
                "RedleafEngine asset distribution package "
                f"{package_version}."
            ),
            "--latest=false",
        ],
        cwd=PROJECT_ROOT,
    )


# --- Main ---

def main() -> None:
    package_version = load_asset_version()

    print("Redleaf Asset Publisher")
    print("=======================")
    print()
    print(f"Asset version: {package_version}")
    print()

    print("Checking GitHub authentication...")
    verify_github_cli()

    print()
    print(f"Building asset package {package_version}...")
    package_dir = build_package(package_version)

    print()
    print(f"Publishing asset package {package_version}...")
    publish_release(
        package_version,
        package_dir,
    )

    print()
    print("Asset package published successfully.")
    print()
    print(f"Version: {package_version}")
    print(f"Release: assets-{package_version}")
    print(f"Local:   {package_dir}")


if __name__ == "__main__":
    main()