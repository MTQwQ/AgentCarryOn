#!/usr/bin/env bash

set -u

export GSK_RENDERER="${GSK_RENDERER:-cairo}"

REFRESH_SECONDS=5
BAR_WIDTH=32
FILE_PATH="${1:-}"

done_flag=0
input_text=""
viewer_pid=""

next_progress() {
  if [[ -z "${PROGRESS_STATE:-}" ]]; then
    PROGRESS_STATE="1"
    echo "$PROGRESS_STATE"
    return
  fi

  if [[ "$PROGRESS_STATE" =~ ^[0-9]+$ ]]; then
    local value=$((PROGRESS_STATE + 1))
    if (( value <= 99 )); then
      PROGRESS_STATE="$value"
    else
      PROGRESS_STATE="99.1"
    fi
    echo "$PROGRESS_STATE"
    return
  fi

  local fraction="${PROGRESS_STATE#99.}"
  local tail="${fraction: -1}"
  if [[ "$tail" == "9" ]]; then
    PROGRESS_STATE="99.${fraction}1"
  else
    local prefix="${fraction::-1}"
    local next_digit=$((tail + 1))
    PROGRESS_STATE="99.${prefix}${next_digit}"
  fi
  echo "$PROGRESS_STATE"
}

render_progress() {
  local value="$1"
  local integer="${value%%.*}"
  local filled=$(( integer * BAR_WIDTH / 100 ))
  if (( filled > BAR_WIDTH )); then
    filled=$BAR_WIDTH
  fi
  local bar
  printf -v bar '%*s' "$filled" ''
  bar="${bar// /=}"
  local remain=$(( BAR_WIDTH - filled ))
  local dots
  printf -v dots '%*s' "$remain" ''
  dots="${dots// /.}"
  printf '\r[%s%s] %s%%' "$bar" "$dots" "$value"
}

progress_loop() {
  while [[ "$done_flag" -eq 0 ]]; do
    local value
    value="$(next_progress)"
    render_progress "$value"
    sleep "$REFRESH_SECONDS"
  done
  printf '\n'
}

ensure_save_copy() {
  local source_file="$1"
  local save_dir
  save_dir="$(pwd)/aimonitor_save"
  mkdir -p "$save_dir"

  local name
  if [[ -n "$source_file" && -f "$source_file" ]]; then
    name="$(basename "$source_file")"
    cp "$source_file" "$save_dir/$name"
  else
    name="aimonitor_saved.txt"
    printf '%s' "${FILE_CONTENT:-}" > "$save_dir/$name"
  fi

  zenity --info --width=500 --text="Text saved to:\n$save_dir/$name" >/dev/null 2>&1 || true
}

show_text_window() {
  local source_file="$1"
  local display_name="No file provided. The text area is empty."
  if [[ -n "$source_file" ]]; then
    display_name="$(basename "$source_file")"
  fi

  while true; do
    local result
    result="$(zenity --text-info \
      --title="AgentCarryOn - $display_name" \
      --filename="${source_file:-/dev/null}" \
      --width=900 \
      --height=640 \
      --ok-label="Close" \
      --extra-button="Save Text" 2>/dev/null)"
    local status=$?

    if [[ "$result" == "Save Text" ]]; then
      ensure_save_copy "$source_file"
      continue
    fi

    if [[ $status -ne 0 && $status -ne 1 ]]; then
      break
    fi
    break
  done
}

collect_input() {
  input_text="$(zenity --entry \
    --title="AgentCarryOn" \
    --text="Enter instructions for the agent:" \
    --width=520 2>/dev/null)"
  return $?
}

if [[ -n "$FILE_PATH" && -f "$FILE_PATH" ]]; then
  FILE_CONTENT="$(cat "$FILE_PATH")"
  show_text_window "$FILE_PATH" &
  viewer_pid=$!
fi

progress_loop &
progress_pid=$!

if collect_input; then
  done_flag=1
  wait "$progress_pid" 2>/dev/null || true
  if [[ -n "$viewer_pid" ]]; then
    kill "$viewer_pid" 2>/dev/null || true
  fi
  printf '%s\n' "$input_text"
else
  done_flag=1
  wait "$progress_pid" 2>/dev/null || true
  if [[ -n "$viewer_pid" ]]; then
    kill "$viewer_pid" 2>/dev/null || true
  fi
fi
