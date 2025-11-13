#
# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License. You may obtain a copy
# of the License at http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS
# OF ANY KIND, either express or implied. See the License for the specific language
# governing permissions and limitations under the License.
#
import logging
import colorama  # type: ignore
import platform

if platform.system() == "Windows":
    colorama.just_fix_windows_console()

logger = logging.getLogger("lagrange")
handler = logging.StreamHandler()
formatter = logging.Formatter("[%(asctime)s] [%(name)s] [%(levelname)s] %(message)s")


class ColorFormatter(logging.Formatter):
    grey = colorama.Fore.LIGHTBLACK_EX
    yellow = colorama.Fore.YELLOW
    red = colorama.Fore.RED
    bold_red = colorama.Style.BRIGHT + colorama.Fore.RED
    reset = colorama.Style.RESET_ALL
    format_template = "[%(asctime)s] [%(name)s] {color}[%(levelname)s]{reset} %(message)s"

    FORMATS = {
        logging.DEBUG: format_template.format(color=grey, reset=reset),
        logging.INFO: format_template.format(color=grey, reset=reset),
        logging.WARNING: format_template.format(color=yellow, reset=reset),
        logging.ERROR: format_template.format(color=red, reset=reset),
        logging.CRITICAL: format_template.format(color=bold_red, reset=reset),
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


handler.setFormatter(ColorFormatter())
logger.addHandler(handler)
