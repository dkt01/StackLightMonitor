#!/usr/bin/env bash

################################################################################
### @file configPageMinifier.sh
###
### @brief Minifies and compresses config html page into an http response
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

CONFIG_PAGE="config.html"
CONFIG_PAGE_MIN="config.min.html"
CONFIG_HEADER="config_html.h"

VAR_TYPE="const PROGMEM char"
VAR_NAME="config_html[]"

HTTP_HEADER="HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Encoding: gzip\r\nPragma: no-cache\r\n\r\n"

# Get directory of generator script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ ! -f "$DIR/$CONFIG_PAGE" ]
then
    echo >&2 "Could not find source config.  Ensure it is named \"$DIR/$CONFIG_PAGE\""
    exit 2
fi

# Ensure html minifier is available
type html-minifier >/dev/null 2>&1 || \
  { \
    echo >&2 "html-minifier is required!  Install from https://github.com/kangax/html-minifier"; \
    exit 1; \
  }

# Minify
# cp "$DIR/$CONFIG_PAGE" "$DIR/$CONFIG_PAGE_MIN"
html-minifier --minify-js true --collapseWhitespace true -o "$DIR/$CONFIG_PAGE_MIN" "$DIR/$CONFIG_PAGE"

if [ $? -ne 0 ]
then
  echo >&2 "HTML minifier failed!"
  rm "$DIR/$CONFIG_PAGE_MIN"
  exit 1
fi

# Remove whitespace at beginning of lines
sed -i '' -e 's/^ *//' "$DIR/$CONFIG_PAGE_MIN"
# Remove any remaining newlines (should be only one at end of file)
tr -d '\n' < "$DIR/$CONFIG_PAGE_MIN" > "$DIR/${CONFIG_PAGE_MIN}_"
mv "$DIR/${CONFIG_PAGE_MIN}_" "$DIR/$CONFIG_PAGE_MIN"

# Compress page content
gzip --best -c "$DIR/${CONFIG_PAGE_MIN}" | hexdump -v -e '/1 "_x%02X"' | sed 's/_/\\/g' > "$DIR/${CONFIG_PAGE_MIN}_"

# Remove any remaining newlines (should be only one at end of file)
tr -d '\n' < "$DIR/${CONFIG_PAGE_MIN}_" > "$DIR/${CONFIG_PAGE_MIN}"
rm "$DIR/${CONFIG_PAGE_MIN}_"

# Write compressed html/js with http headers to c++ header file as constant string
echo -n "$VAR_TYPE $VAR_NAME = \"$HTTP_HEADER" > "$DIR/$CONFIG_HEADER"
cat "$DIR/$CONFIG_PAGE_MIN" >> "$DIR/$CONFIG_HEADER"
echo "\";" >> "$DIR/$CONFIG_HEADER"

# Delete temporary file
rm "$DIR/$CONFIG_PAGE_MIN"
