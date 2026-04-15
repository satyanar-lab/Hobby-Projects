#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(git -C "$PROJECT_ROOT" rev-parse --show-toplevel)"
PROJECT_NAME="$(basename "$PROJECT_ROOT")"
BRANCH_NAME="$(git -C "$REPO_ROOT" branch --show-current)"
COMMIT_MESSAGE="${1:-WIP: ${PROJECT_NAME} snapshot}"

git -C "$REPO_ROOT" add "$PROJECT_NAME"

if git -C "$REPO_ROOT" diff --cached --quiet; then
    echo "No changes to commit for ${PROJECT_NAME}."
    exit 0
fi

git -C "$REPO_ROOT" commit -m "$COMMIT_MESSAGE"
git -C "$REPO_ROOT" push origin "$BRANCH_NAME"

echo "Snapshot pushed for ${PROJECT_NAME} on branch ${BRANCH_NAME}."