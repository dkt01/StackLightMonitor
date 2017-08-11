#!/usr/bin/env bash

################################################################################
### @file faviconConverter.sh
###
### @brief Converts favicon svg to ico encoded as a compressed http response
###
### @author David Turner (dkt01)
###
### @copyright This file is part of StackLightMonitor
###            (https://github.com/dkt01/StackLightMonitor).
### @copyright StackLightMonitor is free software: you can redistribute it and/or modify
###            it under the terms of the GNU General Public License as published by
###            the Free Software Foundation, either version 3 of the License, or
###            (at your option) any later version.
### @copyright StackLightMonitor is distributed in the hope that it will be useful,
###            but WITHOUT ANY WARRANTY; without even the implied warranty of
###            MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
###            GNU General Public License for more details.
### @copyright You should have received a copy of the GNU General Public License
###            along with StackLightMonitor.  If not, see <http://www.gnu.org/licenses/>.
################################################################################

FAVICON_SVG="favicon.svg"
FAVICON_ICO="favicon.ico"
FAVICON_HEADER="favicon_ico.h"

VAR_TYPE="const PROGMEM char"
VAR_NAME="favicon_ico[]"

HTTP_HEADER="HTTP/1.0 200 OK\r\nContent-Type: image/x-icon\r\nContent-Encoding: gzip\r\nPragma: no-cache\r\n\r\n"

# Get directory of generator script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ ! -f "$DIR/$FAVICON_SVG" ]
then
    echo >&2 "Could not find source vector.  Ensure it is named \"$DIR/$FAVICON_SVG\""
    exit 2
fi

# Ensure ImageMagick is available
type html-minifier >/dev/null 2>&1 || \
  { \
    echo >&2 "convert is required!  Install from http://www.imagemagick.org/script/index.php"; \
    exit 1; \
  }

# Generate favicon icons at 16x16 and 32x32 resolutions
convert "$DIR/$FAVICON_SVG" -transparent white -resize 32x32 -define icon:auto-resize="32,16" -colors 16 "$DIR/$FAVICON_ICO"

# Compress favicon
gzip --best -S .gz "$DIR/$FAVICON_ICO"

# Write favicon to c++ header file as base64 encoded icon
echo -n "$VAR_TYPE $VAR_NAME = \"$HTTP_HEADER" > "$DIR/$FAVICON_HEADER"
hexdump -v -e '"\\" "x" 1/1 "%02X" "\n"' "$DIR/${FAVICON_ICO}.gz" | tr -d '\n' >> "$DIR/$FAVICON_HEADER"
echo "\";" >> "$DIR/$FAVICON_HEADER"