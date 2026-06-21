#!/usr/bin/env python

#
# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
"""Command-line entry point for ``lagrange.scripts.meshstat``.

Configures a colored stderr handler on the library logger and forwards to
:func:`lagrange.scripts.meshstat.main`.
"""

from __future__ import annotations

import logging
import platform
import sys

import colorama
import lagrange.scripts.meshstat as meshstat


class _ColorFormatter(logging.Formatter):
    _COLORS = {
        logging.WARNING: colorama.Fore.YELLOW,
        logging.ERROR: colorama.Fore.RED,
        logging.CRITICAL: colorama.Fore.RED + colorama.Style.BRIGHT,
    }

    def format(self, record: logging.LogRecord) -> str:
        msg = super().format(record)
        color = self._COLORS.get(record.levelno)
        if color is None:
            return msg
        return f"{color}{msg}{colorama.Style.RESET_ALL}"


class _StderrHandler(logging.StreamHandler):
    """StreamHandler that resolves :data:`sys.stderr` on each emit."""

    def __init__(self) -> None:
        super().__init__()

    @property
    def stream(self):
        return sys.stderr

    @stream.setter
    def stream(self, value):  # ignored; resolved dynamically
        pass


def _configure_logging() -> None:
    """Install a stderr handler emitting WARNING+ records on the meshstat logger.

    Idempotent: re-invoking this function in the same interpreter does not
    install a duplicate handler.
    """
    if platform.system() == "Windows":
        colorama.just_fix_windows_console()
    logger = logging.getLogger(meshstat.__name__)
    if any(isinstance(h, _StderrHandler) for h in logger.handlers):
        return
    handler = _StderrHandler()
    handler.setLevel(logging.WARNING)
    handler.setFormatter(_ColorFormatter("%(levelname)s: %(message)s"))
    logger.addHandler(handler)
    logger.setLevel(logging.WARNING)
    logger.propagate = False


if __name__ == "__main__":
    _configure_logging()
    raise SystemExit(meshstat.main())
