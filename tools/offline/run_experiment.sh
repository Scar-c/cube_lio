#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ $# -lt 1 ]]; then
  echo "Usage: tools/offline/run_experiment.sh {eee_01|tunnel_d|shield1} --name RUN_NAME [offline runner options]" >&2
  exit 2
fi
cd "$ROOT"
exec python3 "$ROOT/tools/offline/run.py" "$@"
