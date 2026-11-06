#!/usr/bin/env bash
set -e

# Comprobación preventiva de permisos en la caché de PlatformIO
if [ -d "/home/builder/.platformio" ] && [ ! -w "/home/builder/.platformio" ]; then
    echo "⚠️  ADVERTENCIA DE PERMISOS:"
    echo "   /home/builder/.platformio no tiene permisos de escritura para el usuario actual ($(id -u):$(id -g))."
    echo "   Para solucionarlo, ejecuta una vez en tu máquina host:"
    echo "   docker compose -f docker/docker-compose.yml run --rm --user root builder chown -R builder:builder /home/builder/.platformio"
    echo ""
fi

show_help() {
    cat << 'EOF'
========================================================================
  ESPSomfy-RTS - PlatformIO Build Container
========================================================================

Uso:
  docker compose -f docker/docker-compose.yml run --rm builder [COMANDO] [OPCIONES]

Comandos disponibles:
  help | -h | --help     Muestra este mensaje de ayuda
  build [entorno]        Compila el firmware (por defecto: esp32dev)
  build all | build-all  Compila TODOS los entornos soportados
  buildfs [entorno]      Genera el filesystem LittleFS (por defecto: esp32dev)
  all [entorno]          Compila el firmware Y genera la imagen LittleFS
  all all                Genera LittleFS Y compila TODOS los firmwares
  clean [entorno]        Limpia los archivos del entorno (por defecto: esp32dev)
  clean all | clean-all  Limpia los archivos de compilación de TODOS los entornos
  release                Compila todos los entornos y empaqueta en dist/
  envs | boards          Lista los entornos soportados en platformio.ini
  bash | sh | shell      Abre una consola interactiva dentro del contenedor

Comandos directos de PlatformIO:
  pio <args...>          Ejecuta cualquier comando pio directamente
                         Ejemplo: pio run -e esp32dev -t upload

Entornos disponibles en platformio.ini:
  - esp32dev      (ESP32 estándar / por defecto)
  - esp32wrover   (ESP32 con PSRAM)
  - esp32c3       (ESP32-C3 RISC-V)
  - esp32s2       (ESP32-S2)
  - esp32s3_4mb   (ESP32-S3 con 4MB Flash)
  - esp32s3_8mb   (ESP32-S3 con 8MB Flash)

Ejemplos rápidos:
  docker compose -f docker/docker-compose.yml run --rm builder
  docker compose -f docker/docker-compose.yml run --rm builder build
  docker compose -f docker/docker-compose.yml run --rm builder build all
  docker compose -f docker/docker-compose.yml run --rm builder build esp32c3
  docker compose -f docker/docker-compose.yml run --rm builder buildfs
  docker compose -f docker/docker-compose.yml run --rm builder clean all
  docker compose -f docker/docker-compose.yml run --rm builder all
  docker compose -f docker/docker-compose.yml run --rm builder all all
  docker compose -f docker/docker-compose.yml run --rm builder bash

========================================================================
EOF
}

TARGET_ENVS=(-e esp32dev -e esp32wrover -e esp32c3 -e esp32s2 -e esp32s3_4mb -e esp32s3_8mb)

case "$1" in
    "" | "help" | "-h" | "--help")
        show_help
        ;;
    "build-all" | "buildall")
        echo "==> Compilando TODOS los entornos soportados <=="
        exec pio run "${TARGET_ENVS[@]}"
        ;;
    "build")
        if [ "$2" = "all" ]; then
            echo "==> Compilando TODOS los entornos soportados <=="
            exec pio run "${TARGET_ENVS[@]}"
        fi
        ENV_NAME="${2:-esp32dev}"
        echo "==> Compilando firmware para: $ENV_NAME <=="
        exec pio run -e "$ENV_NAME"
        ;;
    "buildfs" | "fs" | "littlefs")
        ENV_NAME="${2:-esp32dev}"
        echo "==> Generando LittleFS para: $ENV_NAME <=="
        exec pio run -t buildfs -e "$ENV_NAME"
        ;;
    "all")
        if [ "$2" = "all" ]; then
            echo "==> [1/2] Generando LittleFS <=="
            pio run -t buildfs -e esp32dev
            echo "==> [2/2] Compilando TODOS los firmwares <=="
            exec pio run "${TARGET_ENVS[@]}"
        fi
        ENV_NAME="${2:-esp32dev}"
        echo "==> [1/2] Compilando firmware para: $ENV_NAME <=="
        pio run -e "$ENV_NAME"
        echo "==> [2/2] Generando LittleFS para: $ENV_NAME <=="
        exec pio run -t buildfs -e "$ENV_NAME"
        ;;
    "clean-all" | "cleanall")
        echo "==> Limpiando TODOS los entornos soportados <=="
        exec pio run -t clean "${TARGET_ENVS[@]}"
        ;;
    "clean")
        if [ "$2" = "all" ]; then
            echo "==> Limpiando TODOS los entornos soportados <=="
            exec pio run -t clean "${TARGET_ENVS[@]}"
        elif [ -n "$2" ]; then
            echo "==> Limpiando entorno: $2 <=="
            exec pio run -t clean -e "$2"
        else
            echo "==> Limpiando entorno por defecto: esp32dev <=="
            exec pio run -t clean
        fi
        ;;
    "envs" | "boards")
        echo "==> Entornos definidos en platformio.ini: <=="
        INI_PATH="/workspace/platformio.ini"
        [ -f "$INI_PATH" ] || INI_PATH="./platformio.ini"
        [ -f "$INI_PATH" ] || INI_PATH="../platformio.ini"
        if [ -f "$INI_PATH" ]; then
            grep -E '^\[env:[^]]+\]' "$INI_PATH" | sed 's/\[env://; s/\]//'
        fi
        ;;
    "release")
        if [ -f "/workspace/build_release.sh" ]; then
            exec bash /workspace/build_release.sh
        else
            echo "Error: /workspace/build_release.sh no encontrado"
            exit 1
        fi
        ;;
    "bash" | "sh" | "shell")
        exec /bin/bash "${@:2}"
        ;;
    *)
        exec "$@"
        ;;
esac
