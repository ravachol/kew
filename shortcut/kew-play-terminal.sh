#!/bin/sh
term=$(kreadconfig6 --file kdeglobals --group General --key TerminalApplication 2>/dev/null)
[ -z "$term" ] && term=konsole
exec "$term" -e kew play "$@"
