# command | title | folder | executable | desktop categories | MIME types
ADOBE_APP_CATALOG="$ADOBEWINE_SRC/config/apps.txt"
ADOBE_PROGRAM_FILES="$WINEPREFIX/drive_c/Program Files/Adobe"

app_folder() {
  local folder="$1" candidate
  if [[ "$folder" == *'{year}' ]]; then
    folder="${folder%'{year}'}"
    local newest="${folder}2026"
    for candidate in "$ADOBE_PROGRAM_FILES/$folder"20[0-9][0-9]; do
      [ -d "$candidate" ] && newest="${candidate##*/}"
    done
    folder="$newest"
  fi
  printf '%s\n' "$folder"
}
