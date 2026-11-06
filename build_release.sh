#!/usr/bin/env bash
set -e

# Obtener versión canónica
VERSION=$(python3 scripts/git_version.py --version-only 2>/dev/null || python3 -c "import sys; sys.path.insert(0, 'scripts'); from git_version import get_git_version; print(get_git_version())" 2>/dev/null || echo "v2.6.2")
DIST_DIR="dist/$VERSION"
mkdir -p "$DIST_DIR"

echo "=== Compilando versión $VERSION en $DIST_DIR ==="

echo "=== 1. Minificando assets web (data-dev -> data) ==="
python3 minify_data.py

echo "=== 2. Generando LittleFS (SomfyController.littlefs.bin) ==="
pio run -e esp32dev -t buildfs
cp .pio/build/esp32dev/littlefs.bin "$DIST_DIR/SomfyController.littlefs.bin"

# Definición de entornos, chips y offsets exactamente como en GitHub Actions
ENVS=(
  "esp32dev:ESP32:0x1000:SomfyController.ino.esp32.bin:SomfyController.onboard.esp32.bin"
  "esp32wrover:ESP32:0x1000:SomfyController.ino.esp32wrover.bin:SomfyController.onboard.esp32wrover.bin"
  "esp32c3:ESP32-C3:0x0:SomfyController.ino.esp32c3.bin:SomfyController.onboard.esp32c3.bin"
  "esp32s2:ESP32-S2:0x1000:SomfyController.ino.esp32s2.bin:SomfyController.onboard.esp32s2.bin"
  "esp32s3_4mb:ESP32-S3:0x0:SomfyController.ino.esp32s3_4mb.bin:SomfyController.onboard.esp32s3_4mb.bin"
  "esp32s3_8mb:ESP32-S3:0x0:SomfyController.ino.esp32s3_8mb.bin:SomfyController.onboard.esp32s3_8mb.bin"
)

echo "=== 3. Compilando firmwares y empaquetando imágenes onboard ==="
for entry in "${ENVS[@]}"; do
  IFS=":" read -r ENV CHIP ADDR FWNAME OBNAME <<< "$entry"
  echo "--> Compilando $ENV ($CHIP)..."
  pio run -e "$ENV"

  echo "--> Creando imagen Onboard unificada para $ENV..."
  python3 -m esptool --chip "$CHIP" \
    merge-bin -o ".pio/build/$ENV/SomfyController.onboard.bin" \
    "$ADDR" ".pio/build/$ENV/bootloader.bin" \
    0x8000 ".pio/build/$ENV/partitions.bin" \
    0x10000 ".pio/build/$ENV/firmware.bin" \
    0x290000 "$DIST_DIR/SomfyController.littlefs.bin"

  echo "--> Empaquetando en ZIP..."
  zip -j ".pio/build/$ENV/$OBNAME.zip" ".pio/build/$ENV/SomfyController.onboard.bin"

  # Copiar artefactos finales listos a la carpeta dist/$VERSION/
  cp ".pio/build/$ENV/firmware.bin" "$DIST_DIR/$FWNAME"
  cp ".pio/build/$ENV/$OBNAME.zip" "$DIST_DIR/$OBNAME.zip"
done

echo ""
echo "=== ¡COMPILACIÓN COMPLETADA CON ÉXITO! ==="
echo "Los archivos listos para la Release están en la carpeta $DIST_DIR:"
ls -lh "$DIST_DIR/"
