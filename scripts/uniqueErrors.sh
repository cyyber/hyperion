#!/usr/bin/env bash

REPO="$(dirname "$0")/.."

echo "Finding unique failures..."
(
for x in "$@"
do
    echo -n "$x" " # "
    # This subshell is a workaround to prevent the shell from printing
    # "Aborted"
    ("$REPO"/build/test/tools/hypfuzzer < "$x" || true) 2>&1 | head -n 1
done
) | sort -u -t'#' -k 2
