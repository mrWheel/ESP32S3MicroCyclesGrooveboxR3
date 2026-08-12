#!/usr/bin/env python3
#
#-- Version Date: 12-08-2026 -- (dd-mm-eeyy)
#
from __future__ import annotations

import argparse
import difflib
import hashlib
import shutil
import subprocess
import sys
import tempfile
import re
from dataclasses import dataclass
from datetime import date
from pathlib import Path


TEMPLATE_REPO = "https://github.com/mrWheel/templateRepo"


# Default paths to sync from the template repository.
# Add more paths here when the template repository grows.
DEFAULT_PATHS = [
    ".github/workflows",
    "tools/git-hooks",
    ".clang-format",
    ".codingRules.md",
    "createProjectStructure.py",
    "createProjectStructure.md",
]


@dataclass(frozen=True)
class FileStats:
    sizeBytes: int
    mtime: float
    lineCount: int
    sha256: str


class UserQuitRequested(Exception):
    pass


class ConfigurationValidationError(Exception):
    pass


TAG_RELEASE_ENV_KEYS = [
    "PROGRAM_NAME",
    "PROGRAM_SRC",
    "PROGRAM_DIR",
]


TAG_RELEASE_ENV_PROMPTS = {
    "PROGRAM_NAME": "Program/project name",
    "PROGRAM_SRC": "Main source file (e.g. main.cpp)",
    "PROGRAM_DIR": "Source directory (e.g. src)",
}


def run(cmd: list[str], cwd: Path | None = None) -> str:
    """Run command, raise on error, return stdout."""
    p = subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if p.returncode != 0:
        raise RuntimeError(
            f"Command failed ({p.returncode}): {' '.join(cmd)}\n"
            f"STDOUT:\n{p.stdout}\nSTDERR:\n{p.stderr}"
        )
    return p.stdout.strip()


def _read_text_safe(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return None


def _count_lines(path: Path) -> int:
    text = _read_text_safe(path)
    if text is None:
        return 0
    return text.count("\n") + (0 if text.endswith("\n") or text == "" else 1)


def _calc_sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def _calc_sha256_for_compare(path: Path) -> str:
    return _calc_sha256_file(path)


def _is_tag_release_yml(path: Path) -> bool:
    return path.as_posix().endswith(".github/workflows/tag-release.yml")


def _merge_tag_release_env_values(template_text: str, existing_text: str) -> str:
    env_values: dict[str, str] = {}

    for key in TAG_RELEASE_ENV_KEYS:
        m = re.search(rf"^(\s*{key}\s*:\s*)(.*)$", existing_text, flags=re.MULTILINE)
        if m:
            env_values[key] = m.group(2)

    if not env_values:
        return template_text

    merged_lines: list[str] = []
    for line in template_text.splitlines(keepends=True):
        replaced = False
        for key, value in env_values.items():
            m = re.match(rf"^(\s*{key}\s*:\s*).*$", line)
            if not m:
                continue

            newline = "\n" if line.endswith("\n") else ""
            merged_lines.append(m.group(1) + value + newline)
            replaced = True
            break

        if not replaced:
            merged_lines.append(line)

    return "".join(merged_lines)


def _extract_tag_release_env_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for key in TAG_RELEASE_ENV_KEYS:
        m = re.search(rf"^\s*{key}\s*:\s*(.*)$", text, flags=re.MULTILINE)
        if m:
            values[key] = m.group(1).strip()
    return values


def _strip_yaml_outer_quotes(value: str) -> str:
    stripped = value.strip()
    if len(stripped) >= 2 and (
        (stripped.startswith('"') and stripped.endswith('"'))
        or (stripped.startswith("'") and stripped.endswith("'"))
    ):
        return stripped[1:-1]
    return stripped


def _yaml_double_quote(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def _prompt_tag_release_env_values(
    template_text: str,
    existing_text: str | None,
) -> dict[str, str] | None:
    if not sys.stdin.isatty():
        return None

    template_values = _extract_tag_release_env_values(template_text)
    existing_values = _extract_tag_release_env_values(existing_text) if existing_text else {}

    print("")
    print("Configure .github/workflows/tag-release.yml values:")

    prompted: dict[str, str] = {}
    for key in TAG_RELEASE_ENV_KEYS:
        default_raw = existing_values.get(key, template_values.get(key, ""))
        default_value = _strip_yaml_outer_quotes(default_raw)
        prompt_label = TAG_RELEASE_ENV_PROMPTS.get(key, key)
        user_value = input(f"  {key} ({prompt_label}) [{default_value}]: ").strip()
        prompted[key] = user_value if user_value else default_value

    return prompted


def _apply_tag_release_env_values(template_text: str, values: dict[str, str]) -> str:
    output = template_text
    for key in TAG_RELEASE_ENV_KEYS:
        if key not in values:
            continue
        output = re.sub(
            rf"^(\s*{key}\s*:\s*).*$",
            rf"\1{_yaml_double_quote(values[key])}",
            output,
            flags=re.MULTILINE,
        )
    return output


def _normalize_tag_release_for_compare(text: str) -> str:
    normalized = text
    for key in TAG_RELEASE_ENV_KEYS:
        normalized = re.sub(
            rf"^(\s*{key}\s*:\s*).*$",
            rf"\1\"<normalized>\"",
            normalized,
            flags=re.MULTILINE,
        )
    return normalized


def _is_placeholder_value(value: str, template_value: str) -> bool:
    normalized = _strip_yaml_outer_quotes(value).strip()
    template_normalized = _strip_yaml_outer_quotes(template_value).strip()
    return (
        not normalized
        or normalized == template_normalized
        or (normalized.startswith("<") and normalized.endswith(">"))
    )


def validate_tag_release_configuration(
    repo_root: Path,
    template_dir: Path,
) -> None:
    """Validate the workflow values using its framework-independent path semantics."""
    workflow_rel = Path(".github/workflows/tag-release.yml")
    workflow_path = repo_root / workflow_rel
    template_workflow_path = template_dir / workflow_rel

    workflow_text = _read_text_safe(workflow_path)
    template_text = _read_text_safe(template_workflow_path)
    if workflow_text is None:
        raise ConfigurationValidationError(
            f"Cannot read {workflow_rel}. The project cannot be prepared for the "
            "flasherWebsite until this workflow exists and is readable."
        )

    values = _extract_tag_release_env_values(workflow_text)
    template_values = _extract_tag_release_env_values(template_text or "")
    problems: list[str] = []

    for key in TAG_RELEASE_ENV_KEYS:
        if key not in values:
            problems.append(f"{key}: field is missing from the workflow env section")
            continue
        if _is_placeholder_value(values[key], template_values.get(key, "")):
            problems.append(
                f"{key}: still contains the template/default value "
                f"{values[key]!r}"
            )

    program_dir_value = _strip_yaml_outer_quotes(values.get("PROGRAM_DIR", "")).strip()
    program_src_value = _strip_yaml_outer_quotes(values.get("PROGRAM_SRC", "")).strip()

    if program_dir_value and not _is_placeholder_value(
        values.get("PROGRAM_DIR", ""), template_values.get("PROGRAM_DIR", "")
    ):
        program_dir = Path(program_dir_value)
        if program_dir.is_absolute():
            problems.append("PROGRAM_DIR: must be relative to the target project root")
        else:
            resolved_dir = (repo_root / program_dir).resolve()
            try:
                resolved_dir.relative_to(repo_root)
            except ValueError:
                problems.append("PROGRAM_DIR: resolves outside the target project")
            else:
                if not resolved_dir.is_dir():
                    problems.append(
                        f"PROGRAM_DIR: directory does not exist: {program_dir_value!r}"
                    )

    if (
        program_dir_value
        and program_src_value
        and not _is_placeholder_value(
            values.get("PROGRAM_DIR", ""), template_values.get("PROGRAM_DIR", "")
        )
        and not _is_placeholder_value(
            values.get("PROGRAM_SRC", ""), template_values.get("PROGRAM_SRC", "")
        )
    ):
        program_src = Path(program_src_value)
        if program_src.is_absolute():
            problems.append(
                "PROGRAM_SRC: must be relative to PROGRAM_DIR, matching the workflow's "
                "PROGRAM_DIR/PROGRAM_SRC semantics"
            )
        else:
            source_path = (repo_root / program_dir_value / program_src).resolve()
            try:
                source_path.relative_to(repo_root)
            except ValueError:
                problems.append(
                    "PROGRAM_SRC: PROGRAM_DIR/PROGRAM_SRC resolves outside the target project"
                )
            else:
                if not source_path.is_file():
                    problems.append(
                        "PROGRAM_SRC: file does not exist at the workflow path "
                        f"{program_dir_value!r}/{program_src_value!r}"
                    )

    if problems:
        details = "\n".join(f"  - {problem}" for problem in problems)
        raise ConfigurationValidationError(
            "Invalid .github/workflows/tag-release.yml configuration.\n"
            "Before this template can be applied, edit the workflow env section and set:\n"
            "  - PROGRAM_NAME to this project's actual name (not the template/default name).\n"
            "  - PROGRAM_DIR to an existing source directory relative to the project root.\n"
            "  - PROGRAM_SRC to the source file containing PROG_VERSION, relative to "
            "PROGRAM_DIR.\n"
            "These fields are framework-independent: use the real paths for either a "
            "PlatformIO or ESP-IDF project.\n"
            "Detected problems:\n"
            f"{details}"
        )


def _copy_file_with_special_handling(src: Path, dst: Path) -> bool:
    if not _is_tag_release_yml(src) or not _is_tag_release_yml(dst):
        shutil.copy2(src, dst)
        return True

    template_text = _read_text_safe(src)
    existing_text = _read_text_safe(dst) if dst.exists() else None

    if template_text is None:
        shutil.copy2(src, dst)
        return True

    prompted_values = _prompt_tag_release_env_values(template_text, existing_text)
    if prompted_values is not None:
        merged_text = _apply_tag_release_env_values(template_text, prompted_values)
        if existing_text is not None and merged_text == existing_text:
            print(
                "Info: No effective content changes after prompting "
                "PROGRAM_NAME/PROGRAM_SRC/PROGRAM_DIR in .github/workflows/tag-release.yml."
            )
            return False

        dst.write_text(merged_text, encoding="utf-8")
        shutil.copystat(src, dst)
        print(
            "Info: Updated PROGRAM_NAME/PROGRAM_SRC/PROGRAM_DIR values "
            "in .github/workflows/tag-release.yml from runtime input."
        )
        return True

    if existing_text is None:
        print(
            "Warning: No interactive TTY detected; copying .github/workflows/tag-release.yml "
            "without replacing PROGRAM_NAME/PROGRAM_SRC/PROGRAM_DIR placeholders."
        )
        shutil.copy2(src, dst)
        return True

    merged_text = _merge_tag_release_env_values(template_text, existing_text)

    if merged_text == existing_text:
        print(
            "Info: No effective content changes after preserving "
            "PROGRAM_NAME/PROGRAM_SRC/PROGRAM_DIR in .github/workflows/tag-release.yml."
        )
        return False

    dst.write_text(merged_text, encoding="utf-8")
    shutil.copystat(src, dst)
    print(
        "Info: Preserved existing PROGRAM_NAME/PROGRAM_SRC/PROGRAM_DIR values "
        "in .github/workflows/tag-release.yml."
    )
    return True


def get_file_stats(path: Path) -> FileStats:
    st = path.stat()
    return FileStats(
        sizeBytes=st.st_size,
        mtime=st.st_mtime,
        lineCount=_count_lines(path),
        sha256=_calc_sha256_for_compare(path),
    )


def make_unified_diff(src: Path, dst: Path) -> str:
    srcText = _read_text_safe(src)
    dstText = _read_text_safe(dst)

    if srcText is None or dstText is None:
        return ""

    srcNorm = srcText.splitlines(keepends=True)
    dstNorm = dstText.splitlines(keepends=True)

    diffLines = difflib.unified_diff(
        dstNorm,
        srcNorm,
        fromfile=str(dst),
        tofile=str(src),
        lineterm="",
    )
    return "\n".join(diffLines)


def files_differ(src: Path, dst: Path, compare: str) -> bool:
    # dst is assumed to exist
    if _is_tag_release_yml(src) and _is_tag_release_yml(dst):
        src_text = _read_text_safe(src)
        dst_text = _read_text_safe(dst)
        if src_text is not None and dst_text is not None:
            src_norm = _normalize_tag_release_for_compare(src_text)
            dst_norm = _normalize_tag_release_for_compare(dst_text)
            return src_norm != dst_norm

    if compare == "size":
        return src.stat().st_size != dst.stat().st_size

    if compare == "mtime":
        return src.stat().st_mtime != dst.stat().st_mtime

    if compare == "lines":
        return _count_lines(src) != _count_lines(dst)

    if compare == "diff":
        d = make_unified_diff(src, dst)
        return d.strip() != ""

    # default: hash compare
    return _calc_sha256_for_compare(src) != _calc_sha256_for_compare(dst)


def _format_stats(label: str, stats: FileStats) -> str:
    return (
        f"{label}: size={stats.sizeBytes}B, "
        f"mtime={stats.mtime:.0f}, "
        f"lines={stats.lineCount}, "
        f"sha256={stats.sha256[:12]}…"
    )


def prompt_existing_file_action(
    src: Path,
    dst: Path,
    compare: str,
    show_diff: bool,
) -> str:
    # returns: "skip" | "overwrite" | "quit"
    srcStats = get_file_stats(src)
    dstStats = get_file_stats(dst)

    diffText = ""
    if compare == "diff" or show_diff:
        diffText = make_unified_diff(src, dst)

    print("")
    print(f"Warning: File exists: {dst}")
    print(_format_stats("  target ", dstStats))
    print(_format_stats("  template", srcStats))

    if show_diff and diffText.strip():
        print("")
        print("----- diff -----")
        print(diffText)
        print("-----------------------------")

    if not sys.stdin.isatty():
        print("Info: No interactive TTY detected; skipping.")
        return "skip"

    while True:
        print("")
        print("Choose action (case-insensitive): [O]verwrite, [K]eep, [D]iff, [Q]uit")
        choice = input("> ").strip().lower()

        if choice in ["k", "keep", ""]:
            return "skip"

        if choice in ["o", "overwrite"]:
            return "overwrite"

        if choice in ["d", "diff"]:
            if not diffText:
                diffText = make_unified_diff(src, dst)

            if diffText.strip():
                print("")
                print("----- diff -----")
                print(diffText)
                print("-----------------------------")
            else:
                print("Info: No content differences found between template and target file.")
            continue

        if choice in ["q", "quit"]:
            return "quit"

        print("Warning: Unknown choice. Use O/K/D/Q.")


def copy_tree_with_policy(
    src: Path,
    dst: Path,
    on_existing: str,
    compare: str,
    show_diff: bool,
) -> tuple[int, int, int]:
    """
    Copy src into dst, recursively.

    Returns: (copied_files, skipped_files, overwritten_files)
    """
    copied = 0
    skipped = 0
    overwritten = 0

    def handle_file(srcFile: Path, dstFile: Path) -> None:
        nonlocal copied, skipped, overwritten

        dstFile.parent.mkdir(parents=True, exist_ok=True)

        if not dstFile.exists():
            changed = _copy_file_with_special_handling(srcFile, dstFile)
            if changed:
                copied += 1
            else:
                skipped += 1
            return

        # If file content is identical, never prompt or overwrite.
        if not files_differ(srcFile, dstFile, compare):
            skipped += 1
            return

        # dst exists
        if on_existing == "skip":
            skipped += 1
            return

        if on_existing == "overwrite":
            changed = _copy_file_with_special_handling(srcFile, dstFile)
            if changed:
                overwritten += 1
            else:
                skipped += 1
            return

        # on_existing == "ask"
        action = prompt_existing_file_action(
            src=srcFile,
            dst=dstFile,
            compare=compare,
            show_diff=show_diff,
        )

        if action == "skip":
            skipped += 1
            return

        if action == "quit":
            raise UserQuitRequested()

        changed = _copy_file_with_special_handling(srcFile, dstFile)
        if changed:
            overwritten += 1
        else:
            skipped += 1

    if src.is_file():
        handle_file(src, dst)
        return (copied, skipped, overwritten)

    for path in src.rglob("*"):
        rel = path.relative_to(src)
        target = dst / rel

        if path.is_dir():
            target.mkdir(parents=True, exist_ok=True)
            continue

        handle_file(path, target)

    return (copied, skipped, overwritten)


def ensure_exec_bits(hooks_dir: Path) -> None:
    """
    Try to set executable bit for hook files on POSIX.
    """
    if not hooks_dir.exists():
        return

    for f in hooks_dir.iterdir():
        if f.is_file():
            try:
                mode = f.stat().st_mode
                # add u+x,g+x,o+x
                f.chmod(mode | 0o111)
            except Exception:
                # don’t fail the whole script for chmod issues
                pass


def ensure_executable(path: Path) -> None:
    if not path.exists() or not path.is_file():
        return

    try:
        mode = path.stat().st_mode
        path.chmod(mode | 0o111)
    except Exception:
        pass


def ensure_pre_commit_hook(template_dir: Path, repo_root: Path, hooks_path: str) -> bool:
    src_pre_commit = template_dir / "tools/git-hooks/pre-commit"
    dst_pre_commit = repo_root / hooks_path / "pre-commit"

    if not src_pre_commit.exists() or not src_pre_commit.is_file():
        print(
            "Warning: Template pre-commit hook not found at tools/git-hooks/pre-commit.",
            file=sys.stderr,
        )
        return False

    dst_pre_commit.parent.mkdir(parents=True, exist_ok=True)

    if not dst_pre_commit.exists():
        shutil.copy2(src_pre_commit, dst_pre_commit)
        print(f"Installed hook file: {hooks_path}/pre-commit")

    ensure_executable(dst_pre_commit)
    return dst_pre_commit.exists()


def set_hooks_path(repo_root: Path, hooks_path: str) -> None:
    # Set hooks path (relative is fine).
    run(["git", "config", "core.hooksPath", hooks_path], cwd=repo_root)

def apply_self_update_from_template(
    template_dir: Path,
    repo_root: Path,
    args: argparse.Namespace,
) -> bool:
    """
    Update applyTemplate.py from template root if present.
    Deferred update: copy to a temp file first, then apply policy and replace target.
    """
    src_self = template_dir / "applyTemplate.py"
    dst_self = repo_root / "applyTemplate.py"

    if not src_self.exists():
        return False

    tmp_self = template_dir.parent / "applyTemplate_self_update.py"
    shutil.copy2(src_self, tmp_self)

    do_update = True

    if dst_self.exists():
        if _calc_sha256_for_compare(tmp_self) == _calc_sha256_for_compare(dst_self):
            do_update = False
        elif args.on_existing == "skip":
            do_update = False
        elif args.on_existing == "ask":
            action = prompt_existing_file_action(
                src=tmp_self,
                dst=dst_self,
                compare=args.compare,
                show_diff=args.show_diff,
            )
            if action == "skip":
                do_update = False
            elif action == "quit":
                raise UserQuitRequested()

    if not do_update:
        return False

    changed = _copy_file_with_special_handling(tmp_self, dst_self)
    #-x-update_version_date_header(dst_self)
    return changed


def parse_args() -> tuple[argparse.ArgumentParser, argparse.Namespace]:
    ap = argparse.ArgumentParser(
        description=(
            "Apply mrWheel/templateRepo files to a target project directory "
            "(ask on differences by default) and enable git hooks."
        ),
        epilog=(
            "Example:\n"
            "  ./applyTemplate.py /path/to/project\n"
            "\n"
            "If no project path is provided, this script prints help text."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    ap.add_argument(
        "project_path",
        nargs="?",
        help="Path to the target project root.",
    )
    ap.add_argument(
        "--template",
        default=TEMPLATE_REPO,
        help=(
            f"Template repo URL or local template directory (default: {TEMPLATE_REPO}). "
            "When a local directory is provided, files are read from its current working tree."
        ),
    )
    ap.add_argument(
        "--paths",
        nargs="*",
        default=DEFAULT_PATHS,
        help=f"Paths to copy from template (default: {', '.join(DEFAULT_PATHS)})",
    )
    ap.add_argument(
        "--hooks-path",
        default="tools/git-hooks",
        help="Where hooks live in target repo; will be set as git core.hooksPath (default: tools/git-hooks)",
    )
    ap.add_argument(
        "--on-existing",
        choices=["skip", "ask", "overwrite"],
        default="ask",
        help="What to do when target file already exists (default: ask)",
    )
    ap.add_argument(
        "--compare",
        choices=["hash", "mtime", "size", "lines", "diff"],
        default="hash",
        help="How to detect differences when --on-existing ask is used (default: hash)",
    )
    ap.add_argument(
        "--show-diff",
        action="store_true",
        help="When asking on existing files, show unified diff automatically.",
    )
    return ap, ap.parse_args()


def main() -> int:
    parser, args = parse_args()

    if not args.project_path:
        print("Template Repo Applier")
        print("")
        print("This script applies shared template files to a target project directory.")
        print("It copies configured paths from the template repo and enables Git hooks.")
        print("")
        parser.print_help()
        return 1

    repo_root = Path(args.project_path).expanduser().resolve()

    if not repo_root.exists() or not repo_root.is_dir():
        print(f"Error: Project path does not exist or is not a directory: {repo_root}", file=sys.stderr)
        return 2

    # Check that git exists
    try:
        run(["git", "--version"])
    except Exception as e:
        print(f"Error: Git does not seem available: {e}", file=sys.stderr)
        return 2

    self_updated = False
    pre_commit_ready = False

    try:
        template_candidate = Path(args.template).expanduser()
        tmp_ctx: tempfile.TemporaryDirectory[str] | None = None

        if template_candidate.exists() and template_candidate.is_dir():
            template_dir = template_candidate.resolve()
            print(f"Using local template working tree: {template_dir}")
        else:
            tmp_ctx = tempfile.TemporaryDirectory(prefix="templateRepo_")
            tmp = Path(tmp_ctx.name)
            template_dir = tmp / "template"

            print(f"Cloning template: {args.template}")
            run(["git", "clone", "--depth", "1", args.template, str(template_dir)])

        total_copied = 0
        total_skipped = 0
        total_overwritten = 0

        for rel in args.paths:
            src = template_dir / rel
            dst = repo_root / rel

            if not src.exists():
                print(f"Warning: Not found in template, skipping: {rel}")
                continue

            dst.mkdir(parents=True, exist_ok=True) if src.is_dir() else None

            copied, skipped, overwritten = copy_tree_with_policy(
                src=src,
                dst=dst,
                on_existing=args.on_existing,
                compare=args.compare,
                show_diff=args.show_diff,
            )

            total_copied += copied
            total_skipped += skipped
            total_overwritten += overwritten

            extra = ""
            if args.on_existing != "skip":
                extra = f", ~{overwritten} overwritten"

            print(f"Applied {rel}: +{copied} copied, {skipped} skipped{extra}")

        # thisProject.png is stored at the template root but belongs in the
        # target project's metadata directory. Apply the normal file policy.
        project_image_src = template_dir / "thisProject.png"
        project_image_dst = repo_root / "projectMetaData/thisProject.png"
        if not project_image_src.is_file():
            raise ConfigurationValidationError(
                "Required template image is missing: thisProject.png must exist at "
                "the template repository root."
            )

        copied, skipped, overwritten = copy_tree_with_policy(
            src=project_image_src,
            dst=project_image_dst,
            on_existing=args.on_existing,
            compare=args.compare,
            show_diff=args.show_diff,
        )
        total_copied += copied
        total_skipped += skipped
        total_overwritten += overwritten
        extra = ""
        if args.on_existing != "skip":
            extra = f", ~{overwritten} overwritten"
        print(
            "Applied thisProject.png -> projectMetaData/thisProject.png: "
            f"+{copied} copied, {skipped} skipped{extra}"
        )

        validate_tag_release_configuration(
            repo_root=repo_root,
            template_dir=template_dir,
        )
        print(
            "Validated tag-release.yml for a PlatformIO or ESP-IDF project: "
            "PROGRAM_NAME, PROGRAM_DIR, and PROGRAM_SRC are project-specific and valid."
        )

        # Self-update is handled last (from template root applyTemplate.py)
        self_updated = apply_self_update_from_template(
            template_dir=template_dir,
            repo_root=repo_root,
            args=args,
        )
        if self_updated:
            print("applyTemplate.py was updated from template.")

        pre_commit_ready = ensure_pre_commit_hook(
            template_dir=template_dir,
            repo_root=repo_root,
            hooks_path=args.hooks_path,
        )
    except UserQuitRequested:
        print("Aborted by user.")
        return 130
    except ConfigurationValidationError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 2
    finally:
        if "tmp_ctx" in locals() and tmp_ctx is not None:
            tmp_ctx.cleanup()

    # Enable hooks
    hooks_dir = repo_root / args.hooks_path
    if hooks_dir.exists() and pre_commit_ready:
        ensure_exec_bits(hooks_dir)
        try:
            set_hooks_path(repo_root, args.hooks_path)
            print(f"Git hooks enabled: core.hooksPath = {args.hooks_path}")
        except Exception as e:
            print(
                f"Warning: Could not set core.hooksPath in target project ({e}).",
                file=sys.stderr,
            )
    else:
        print(
            f"Warning: Hook activation skipped. Ensure {args.hooks_path}/pre-commit exists.",
            file=sys.stderr,
        )

    ensure_executable(repo_root / "createProjectStructure.py")

    # Also update this file's header date even if only hooks/workflows changed
    #-x-update_version_date_header(repo_root / "applyTemplate.py")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
    
