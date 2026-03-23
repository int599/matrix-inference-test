# AGENTS.md

This repo is intended for GitHub.

## General

- Keep the repo focused on benchmark code, benchmark docs, and versioned result artifacts.
- Prefer simple, reusable scripts and host-generic documentation.

## Repo Hygiene

- When writing tracked files, avoid internal-only details such as hostnames, usernames, and absolute local paths.
- Prefer CPU model naming for result files and benchmark figures.
- Prefer relative paths in generated reports when they are sufficient.
- Published PNG figures under `docs/results/` should use cache-busting hashed filenames. Update Markdown references through the publish script instead of hand-editing stale image paths.
- When republishing a figure for the same logical artifact, remove superseded hashed PNGs so the repo keeps only the current published version.

## Keep Elsewhere

- Internal deployment or sync wrappers
- Local-only workflow notes
- Temporary files and one-off artifacts
