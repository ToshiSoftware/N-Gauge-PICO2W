#!/bin/zsh
# arduino-task.sh : VS Code tasks から呼ぶビルド/書き込みスクリプト
#   usage: arduino-task.sh compile | upload | monitor
#
# - Arduino IDE 同梱の arduino-cli と、IDE と同じ設定ファイルを使う
# - ビルド出力は Google Drive 外 (~/Library/Caches) に置く（同期対象にしない）
# - 書き込み先ポートは自動検出。環境変数 PORT で明示指定も可
#   (例: PORT=/dev/cu.usbmodem1101 .vscode/arduino-task.sh upload)
# - シリアルポートが無く BOOTSEL ドライブ(RP2350)がマウントされていれば UF2 を直接コピー

set -e

CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
CONFIG="$HOME/.arduinoIDE/arduino-cli.yaml"
FQBN="rp2040:rp2040:rpipico2w"
SKETCH_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SKETCH_NAME="$(basename "$SKETCH_DIR")"
BUILD_DIR="$HOME/Library/Caches/arduino-build/$SKETCH_NAME"
BAUD="${BAUD:-115200}"

cli() { "$CLI" --config-file "$CONFIG" "$@"; }

find_port() {
  if [[ -n "$PORT" ]]; then echo "$PORT"; return; fi
  local ports=(/dev/cu.usbmodem*(N))
  (( ${#ports} )) && echo "${ports[1]}"
  return 0
}

find_bootsel_drive() {
  for d in /Volumes/RP2350 /Volumes/RPI-RP2; do
    [[ -d "$d" ]] && { echo "$d"; return 0; }
  done
  return 0
}

do_compile() {
  echo "=== Compile: $SKETCH_NAME ($FQBN) ==="
  cli compile --fqbn "$FQBN" --build-path "$BUILD_DIR" --warnings default "$SKETCH_DIR"
}

do_upload() {
  local port drive uf2
  port="$(find_port)"
  if [[ -n "$port" ]]; then
    echo "=== Upload via $port ==="
    cli upload --fqbn "$FQBN" --input-dir "$BUILD_DIR" -p "$port" "$SKETCH_DIR"
    return
  fi
  drive="$(find_bootsel_drive)"
  if [[ -n "$drive" ]]; then
    uf2="$BUILD_DIR/$SKETCH_NAME.ino.uf2"
    [[ -f "$uf2" ]] || { echo "UF2 が見つかりません: $uf2"; exit 1; }
    echo "=== Copy UF2 to BOOTSEL drive $drive ==="
    cp "$uf2" "$drive/"
    echo "done."
    return
  fi
  echo "ERROR: ボードが見つかりません。USB 接続を確認するか、BOOTSEL を押しながら接続してください。"
  echo "       (ポートを明示する場合: PORT=/dev/cu.usbmodemXXXX)"
  exit 1
}

do_monitor() {
  local port
  port="$(find_port)"
  [[ -n "$port" ]] || { echo "ERROR: シリアルポートが見つかりません。"; exit 1; }
  echo "=== Serial monitor $port @ $BAUD (Ctrl+C で終了) ==="
  cli monitor -p "$port" -c "baudrate=$BAUD"
}

case "$1" in
  compile) do_compile ;;
  upload)  do_upload ;;
  build-upload) do_compile && do_upload ;;
  monitor) do_monitor ;;
  *) echo "usage: $0 compile|upload|build-upload|monitor"; exit 2 ;;
esac
