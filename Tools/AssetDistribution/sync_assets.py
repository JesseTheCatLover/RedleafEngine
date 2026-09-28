from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import shutil
import sys


# --- Constants ---

DOWNLOAD_CHUNK_SIZE = 1024 * 1024

# --- Output ---

def print_status(message: str) -> None:
    print(message)


def print_warning(message: str) -> None:
    print(f"[WARNING] {message}")


def print_error(message: str) -> None:
    print(f"ERROR: {message}")


# --- JSON ---

def load_json(path: Path) -> dict:
    try:
        with path.open("r", encoding="utf-8") as file:
            data = json.load(file)

    except FileNotFoundError:
        print_error(f"File not found: {path}")
        sys.exit(1)

    except json.JSONDecodeError as error:
        print_error(
            f"Invalid JSON in {path}: {error}"
        )
        sys.exit(1)

    if not isinstance(data, dict):
        print_error(
            f"Expected a JSON object in: {path}"
        )
        sys.exit(1)

    return data


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    with path.open(
            "w",
            encoding="utf-8",
    ) as file:
        json.dump(
            data,
            file,
            indent=4,
            ensure_ascii=False,
        )
        file.write("\n")


# --- Hashing ---

def calculate_sha256(path: Path) -> str:
    sha256 = hashlib.sha256()

    with path.open("rb") as file:
        while chunk := file.read(DOWNLOAD_CHUNK_SIZE):
            sha256.update(chunk)

    return sha256.hexdigest()


# --- Paths ---

def ensure_path_within_root(
        path: Path,
        root: Path,
) -> None:
    resolved_root = root.resolve()
    resolved_path = path.resolve(strict=False)

    try:
        resolved_path.relative_to(resolved_root)
    except ValueError:
        print_error(
            "Asset path escapes its destination root:\n"
            f"  {path}"
        )
        sys.exit(1)


def get_install_path(
        package_path: str,
        project_root: Path,
) -> Path:
    path = PurePosixPath(package_path)

    if not package_path:
        print_error("Asset package path is empty.")
        sys.exit(1)

    if path.is_absolute():
        print_error(
            f"Absolute asset paths are not allowed: "
            f"{package_path}"
        )
        sys.exit(1)

    if "\\" in package_path:
        print_error(
            "Backslashes are not allowed in asset package paths:\n"
            f"  {package_path}"
        )
        sys.exit(1)

    if ".." in path.parts:
        print_error(
            "Parent directory traversal is not allowed:\n"
            f"  {package_path}"
        )
        sys.exit(1)

    if "." in path.parts or "" in path.parts:
        print_error(
            f"Invalid asset package path:\n"
            f"  {package_path}"
        )
        sys.exit(1)

    if len(path.parts) < 2:
        print_error(
            f"Asset package path must reference a file:\n"
            f"  {package_path}"
        )
        sys.exit(1)

    package_root = path.parts[0]
    relative_path = Path(*path.parts[1:])

    if package_root == "SourceAssets":
        destination_root = project_root / "SourceAssets"

    elif package_root == "RawAssets":
        destination_root = project_root / "Assets"

    else:
        print_error(
            f"Unsupported asset package root: "
            f"{package_root}"
        )
        sys.exit(1)

    destination = destination_root / relative_path

    ensure_path_within_root(
        destination,
        destination_root,
    )

    return destination


# --- Manifest validation ---

def validate_manifest(
        manifest: dict,
        expected_version: str,
        manifest_format_version: int,
        project_root: Path,
) -> None:
    if manifest.get("format_version") != manifest_format_version:
        print_error(
            "Unsupported asset manifest format version: "
            f"{manifest.get('format_version')}"
        )
        sys.exit(1)

    package_version = manifest.get("package_version")

    if package_version != expected_version:
        print_error(
            "Asset package version mismatch.\n"
            f"  Expected: {expected_version}\n"
            f"  Received: {package_version}"
        )
        sys.exit(1)

    files = manifest.get("files")

    if not isinstance(files, dict):
        print_error(
            "Asset manifest 'files' must be an object."
        )
        sys.exit(1)

    for package_path, metadata in files.items():
        if not isinstance(package_path, str):
            print_error(
                "Asset manifest contains an invalid path."
            )
            sys.exit(1)

        if not isinstance(metadata, dict):
            print_error(
                f"Invalid asset metadata: {package_path}"
            )
            sys.exit(1)

        sha256 = metadata.get("sha256")
        size = metadata.get("size")

        if (
                not isinstance(sha256, str)
                or len(sha256) != 64
                or not all(
            character in "0123456789abcdef"
            for character in sha256
        )
        ):
            print_error(
                f"Invalid SHA-256: {package_path}"
            )
            sys.exit(1)

        if not isinstance(size, int) or size < 0:
            print_error(
                f"Invalid size: {package_path}"
            )
            sys.exit(1)

        get_install_path(
            package_path,
            project_root,
        )


def validate_state(
        state: dict,
        state_format_version: int,
        project_root: Path,
) -> None:
    if state.get("format_version") != state_format_version:
        print_error(
            "Unsupported asset state format version: "
            f"{state.get('format_version')}"
        )
        sys.exit(1)

    files = state.get("files")

    if not isinstance(files, dict):
        print_error(
            "Asset state 'files' must be an object."
        )
        sys.exit(1)

    for package_path, metadata in files.items():
        if not isinstance(package_path, str):
            print_error("Asset state contains an invalid path.")
            sys.exit(1)

        if not isinstance(metadata, dict):
            print_error(
                f"Invalid asset state metadata: {package_path}"
            )
            sys.exit(1)

        get_install_path(
            package_path,
            project_root,
        )


def validate_plan(
        plan: dict,
        expected_version: str,
        plan_format_version: int,
) -> None:
    if plan.get("format_version") != plan_format_version:
        print_error(
            "Unsupported asset plan format version: "
            f"{plan.get('format_version')}"
        )
        sys.exit(1)

    if plan.get("package_version") != expected_version:
        print_error(
            "Asset plan package version mismatch.\n"
            f"  Expected: {expected_version}\n"
            f"  Received: {plan.get('package_version')}"
        )
        sys.exit(1)

    downloads = plan.get("downloads")
    removals = plan.get("removals")

    if not isinstance(downloads, list):
        print_error(
            "Asset plan 'downloads' must be an array."
        )
        sys.exit(1)

    if not isinstance(removals, list):
        print_error(
            "Asset plan 'removals' must be an array."
        )
        sys.exit(1)


# --- State ---

def load_state(
        state_path: Path,
        state_format_version: int,
        project_root: Path,
) -> dict | None:
    if not state_path.exists():
        return None

    state = load_json(state_path)
    validate_state(
        state,
        state_format_version,
        project_root,
    )

    return state


def write_state(
        state_path: Path,
        manifest: dict,
        state_format_version: int,
) -> None:
    state = {
        "format_version": state_format_version,
        "package_version": manifest["package_version"],
        "files": manifest["files"],
    }

    write_json(
        state_path,
        state,
    )


# --- Local file checks ---

def is_valid_local_file(
        path: Path,
        metadata: dict,
) -> bool:
    if not path.exists():
        return False

    if path.is_symlink():
        print_error(
            "Refusing to manage a symlinked asset:\n"
            f"  {path}"
        )
        sys.exit(1)

    if not path.is_file():
        return False

    expected_size = metadata["size"]

    if path.stat().st_size != expected_size:
        return False

    return (
            calculate_sha256(path)
            == metadata["sha256"]
    )


# --- Planning ---

def build_download_plan(
        manifest: dict,
        state: dict | None,
        project_root: Path,
        plan_format_version: int,
) -> dict:
    manifest_files = manifest["files"]

    downloads_by_hash: dict[str, dict] = {}
    removals: list[dict] = []

    for package_path, metadata in manifest_files.items():
        destination = get_install_path(
            package_path,
            project_root,
        )

        if is_valid_local_file(
                destination,
                metadata,
        ):
            continue

        sha256 = metadata["sha256"]

        if sha256 not in downloads_by_hash:
            downloads_by_hash[sha256] = {
                "sha256": sha256,
                "size": metadata["size"],
                "files": [],
            }

        downloads_by_hash[sha256]["files"].append(
            package_path
        )


    if state is not None:
        state_files = state.get("files", {})

        for package_path, metadata in state_files.items():
            if package_path in manifest_files:
                continue

            removals.append(
                {
                    "path": package_path,
                    "sha256": metadata.get("sha256"),
                    "size": metadata.get("size"),
                }
            )

    downloads = sorted(
        downloads_by_hash.values(),
        key=lambda entry: entry["sha256"],
    )

    for download in downloads:
        download["files"].sort()

    removals.sort(
        key=lambda entry: entry["path"]
    )

    return {
        "format_version": plan_format_version,
        "package_version": manifest["package_version"],
        "downloads": downloads,
        "removals": removals,
    }


def run_plan(
        manifest_path: Path,
        state_path: Path,
        project_root: Path,
        output_path: Path,
        expected_version: str,
        manifest_format_version: int,
        state_format_version: int,
        plan_format_version: int,
) -> None:
    manifest = load_json(manifest_path)

    validate_manifest(
        manifest,
        expected_version,
        manifest_format_version,
        project_root,
    )

    state = load_state(
        state_path,
        state_format_version,
        project_root,
    )

    plan = build_download_plan(
        manifest,
        state,
        project_root,
        plan_format_version,
    )

    write_json(
        output_path,
        plan,
    )



# --- Finalization ---

def verify_blob(
        blob_path: Path,
        sha256: str,
        size: int,
) -> None:
    if not blob_path.exists():
        print_error(
            f"Required downloaded blob is missing:\n"
            f"  {blob_path}"
        )
        sys.exit(1)

    if blob_path.is_symlink():
        print_error(
            f"Downloaded blob cannot be a symlink:\n"
            f"  {blob_path}"
        )
        sys.exit(1)

    if not blob_path.is_file():
        print_error(
            f"Downloaded blob is not a file:\n"
            f"  {blob_path}"
        )
        sys.exit(1)

    actual_size = blob_path.stat().st_size

    if actual_size != size:
        print_error(
            "Downloaded blob size mismatch.\n"
            f"  Blob:     {blob_path}\n"
            f"  Expected: {size}\n"
            f"  Received: {actual_size}"
        )
        sys.exit(1)

    actual_hash = calculate_sha256(
        blob_path
    )

    if actual_hash != sha256:
        print_error(
            "Downloaded blob SHA-256 mismatch.\n"
            f"  Blob:     {blob_path}\n"
            f"  Expected: {sha256}\n"
            f"  Received: {actual_hash}"
        )
        sys.exit(1)


def install_blob(
        blob_path: Path,
        destination: Path,
) -> None:
    if destination.is_symlink():
        print_error(
            "Refusing to replace a symlinked asset:\n"
            f"  {destination}"
        )
        sys.exit(1)

    destination.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    temporary_path = destination.with_name(
        f".{destination.name}.redleaf"
    )

    if temporary_path.exists():
        temporary_path.unlink()

    shutil.copyfile(
        blob_path,
        temporary_path,
    )

    temporary_path.replace(
        destination
    )


def remove_managed_file(
        project_root: Path,
        entry: dict,
) -> bool:
    package_path = entry["path"]
    sha256 = entry.get("sha256")
    size = entry.get("size")

    destination = get_install_path(
        package_path,
        project_root,
    )

    if not destination.exists():
        return False

    if destination.is_symlink():
        print_warning(
            "Leaving symlinked removed asset untouched:\n"
            f"  {package_path}"
        )
        return False

    if not destination.is_file():
        print_warning(
            "Leaving non-file removed asset untouched:\n"
            f"  {package_path}"
        )
        return False

    if (
            not isinstance(sha256, str)
            or not isinstance(size, int)
    ):
        print_warning(
            "Removed asset has invalid state metadata; "
            "leaving it untouched:\n"
            f"  {package_path}"
        )
        return False

    if destination.stat().st_size != size:
        print_warning(
            "Removed asset was modified locally; "
            "leaving it untouched:\n"
            f"  {package_path}"
        )
        return False

    if calculate_sha256(destination) != sha256:
        print_warning(
            "Removed asset was modified locally; "
            "leaving it untouched:\n"
            f"  {package_path}"
        )
        return False

    print_status(
        f"  Removing: {package_path}"
    )

    destination.unlink()

    return True


def cleanup_cache(cache_dir: Path) -> None:
    if not cache_dir.exists():
        return

    if cache_dir.is_symlink() or not cache_dir.is_dir():
        print_warning(
            "Asset cache path is not a directory; leaving it untouched:\n"
            f"  {cache_dir}"
        )
        return

    shutil.rmtree(cache_dir)


def run_finalize(
        manifest_path: Path,
        state_path: Path,
        plan_path: Path,
        cache_dir: Path,
        project_root: Path,
        expected_version: str,
        manifest_format_version: int,
        state_format_version: int,
        plan_format_version: int,
) -> None:
    manifest = load_json(manifest_path)

    validate_manifest(
        manifest,
        expected_version,
        manifest_format_version,
        project_root,
    )

    plan = load_json(plan_path)

    validate_plan(
        plan,
        expected_version,
        plan_format_version,
    )


    for download in plan["downloads"]:
        sha256 = download["sha256"]
        size = download["size"]

        blob_path = (
                cache_dir
                / f"blob-{sha256}"
        )

        verify_blob(
            blob_path,
            sha256,
            size,
        )

    installed_file_count = 0

    for download in plan["downloads"]:
        sha256 = download["sha256"]

        blob_path = (
                cache_dir
                / f"blob-{sha256}"
        )

        for package_path in download["files"]:
            destination = get_install_path(
                package_path,
                project_root,
            )

            install_blob(
                blob_path,
                destination,
            )

            installed_file_count += 1

    removed_file_count = 0

    for removal in plan["removals"]:
        if remove_managed_file(
                project_root,
                removal,
        ):
            removed_file_count += 1

    write_state(
        state_path,
        manifest,
        state_format_version,
    )

    cleanup_cache(
        cache_dir,
    )

    if installed_file_count > 0 and removed_file_count > 0:
        print_status(
            "Asset distribution: "
            f"{installed_file_count} installed, "
            f"{removed_file_count} removed."
        )
    elif installed_file_count > 0:
        print_status(
            "Asset distribution: "
            f"{installed_file_count} installed."
        )
    elif removed_file_count > 0:
        print_status(
            "Asset distribution: "
            f"{removed_file_count} removed."
        )
    else:
        print_status(
            "Asset distribution: up to date."
        )

    if installed_file_count > 0 or removed_file_count > 0:
        print_status(
            "Asset distribution: sync complete."
        )


# --- Command line ---

def create_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="RedleafEngine asset synchronization helper."
    )

    subparsers = parser.add_subparsers(
        dest="command",
        required=True,
    )

    plan_parser = subparsers.add_parser(
        "plan",
        help="Generate an asset download plan.",
    )

    plan_parser.add_argument(
        "--manifest",
        type=Path,
        required=True,
    )

    plan_parser.add_argument(
        "--state",
        type=Path,
        required=True,
    )

    plan_parser.add_argument(
        "--project-root",
        type=Path,
        required=True,
    )

    plan_parser.add_argument(
        "--output",
        type=Path,
        required=True,
    )

    plan_parser.add_argument(
        "--version",
        required=True,
    )

    plan_parser.add_argument(
        "--manifest-format-version",
        type=int,
        required=True,
    )

    plan_parser.add_argument(
        "--state-format-version",
        type=int,
        required=True,
    )

    plan_parser.add_argument(
        "--plan-format-version",
        type=int,
        required=True,
    )

    finalize_parser = subparsers.add_parser(
        "finalize",
        help="Install downloaded assets and update state.",
    )

    finalize_parser.add_argument(
        "--manifest",
        type=Path,
        required=True,
    )

    finalize_parser.add_argument(
        "--state",
        type=Path,
        required=True,
    )

    finalize_parser.add_argument(
        "--plan",
        type=Path,
        required=True,
    )

    finalize_parser.add_argument(
        "--cache-dir",
        type=Path,
        required=True,
    )

    finalize_parser.add_argument(
        "--project-root",
        type=Path,
        required=True,
    )

    finalize_parser.add_argument(
        "--version",
        required=True,
    )

    finalize_parser.add_argument(
        "--manifest-format-version",
        type=int,
        required=True,
    )

    finalize_parser.add_argument(
        "--state-format-version",
        type=int,
        required=True,
    )

    finalize_parser.add_argument(
        "--plan-format-version",
        type=int,
        required=True,
    )

    return parser


# --- Main ---

def main() -> None:
    parser = create_argument_parser()
    args = parser.parse_args()

    project_root = args.project_root.resolve()

    if args.command == "plan":
        run_plan(
            manifest_path=args.manifest.resolve(),
            state_path=args.state.resolve(),
            project_root=project_root,
            output_path=args.output.resolve(),
            expected_version=args.version,
            manifest_format_version=args.manifest_format_version,
            state_format_version=args.state_format_version,
            plan_format_version=args.plan_format_version,
        )

    elif args.command == "finalize":
        run_finalize(
            manifest_path=args.manifest.resolve(),
            state_path=args.state.resolve(),
            plan_path=args.plan.resolve(),
            cache_dir=args.cache_dir.resolve(),
            project_root=project_root,
            expected_version=args.version,
            manifest_format_version=args.manifest_format_version,
            state_format_version=args.state_format_version,
            plan_format_version=args.plan_format_version,
        )


if __name__ == "__main__":
    main()
