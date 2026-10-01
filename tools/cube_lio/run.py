#!/usr/bin/env python3
"""Backward-compatible entry point for the unified offline runner."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).resolve().parents[1] / 'offline' / 'run.py'),
                   run_name='__main__')
