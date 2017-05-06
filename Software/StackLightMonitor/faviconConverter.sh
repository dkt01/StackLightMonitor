#!/usr/bin/env bash

FAVICON_SVG="favicon.svg"
FAVICON_ICO="favicon.ico"
FAVICON_HEADER="favicon_ico.h"

VAR_TYPE="const PROGMEM char"
VAR_NAME="favicon_ico[]"

HTTP_HEADER="HTTP/1.0 200 OK\r\nContent-Type: image/x-icon\r\nPragma: no-cache\r\n\r\n"

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
convert "$DIR/$FAVICON_SVG" -alpha off -resize 32x32 -define icon:auto-resize="32,16" -colors 16 "$DIR/$FAVICON_ICO"

# Write favicon to c++ header file as base64 encoded icon
echo -n "$VAR_TYPE $VAR_NAME = \"$HTTP_HEADER" > "$DIR/$FAVICON_HEADER"
hexdump -v -e '"\\" "x" 1/1 "%02X" "\n"' "$DIR/$FAVICON_ICO" | tr -d '\n' >> "$DIR/$FAVICON_HEADER"
# cat "$DIR/$FAVICON_ICO" | base64 | tr -d '\n' >> "$DIR/$FAVICON_HEADER"
echo "\";" >> "$DIR/$FAVICON_HEADER"