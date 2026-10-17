#!/usr/bin/env bash
# ==============================================================================
# Script para inyectar mensajes retenidos de prueba usando mosquitto_pub
# ==============================================================================

BROKER_HOST="${1:-localhost}"
BROKER_PORT="${2:-1883}"
SERVER_ID="${3:-7DF504}"
NUM_SHADES="${4:-0}"
START_ID="${5:-1}"
ROOT_TOPIC="espsomfy-${SERVER_ID}"
DISCO_TOPIC="homeassistant"

echo "=============================================================================="
echo " Inyectando mensajes retenidos de prueba en MQTT..."
echo " Broker:       ${BROKER_HOST}:${BROKER_PORT}"
echo " Server ID:    ${SERVER_ID}"
echo " Root:         ${ROOT_TOPIC}"
if [ "${NUM_SHADES}" -gt 0 ]; then
echo " N Persianas:  ${NUM_SHADES} (IDs ${START_ID} a $((START_ID + NUM_SHADES - 1)), $((NUM_SHADES * 19)) tópicos)"
fi
echo "=============================================================================="

# 1. Bug v2.5.2 (Discovery bajo rootTopic) -> DEBE SER PURGADO
mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${ROOT_TOPIC}/${DISCO_TOPIC}/cover/${SERVER_ID}/1/config" \
  -m '{"name":"Shade 1","unique_id":"old_bug_cover1"}' -r

mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${ROOT_TOPIC}/${DISCO_TOPIC}/switch/${SERVER_ID}/2/config" \
  -m '{"name":"Switch 2","unique_id":"old_bug_switch2"}' -r

# 2. Persiana huérfana / eliminada (ID 19 inexistente) -> DEBE SER PURGADO
mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${ROOT_TOPIC}/shades/19/position" -m "50" -r

mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${ROOT_TOPIC}/shades/19/name" -m "Persiana Fantasma 19" -r

# 3. Persiana ACTIVA (ID 1 existente en el controlador) -> DEBE CONSERVARSE
mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${ROOT_TOPIC}/shades/1/position" -m "100" -r

# 4. Grupo huérfano / eliminado (ID 19 inexistente) -> DEBE SER PURGADO
mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${ROOT_TOPIC}/groups/19/direction" -m "0" -r

# 5. Discovery huérfano con serverId (Persiana / switch 19 inexistente) -> DEBE SER PURGADO
mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${DISCO_TOPIC}/cover/${SERVER_ID}/19/config" \
  -m '{"name":"Ghost Shade 19","unique_id":"ghost_19"}' -r

mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${DISCO_TOPIC}/switch/${SERVER_ID}/19/config" \
  -m '{"name":"Ghost Switch 19","unique_id":"ghost_switch_19"}' -r

# 6. Desambiguación propia (Formato viejo sin serverId perteneciente a este equipo) -> DEBE SER PURGADO
mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${DISCO_TOPIC}/cover/19/config" \
  -m "{\"name\":\"Old Ambiguous Cover\",\"unique_id\":\"mqtt_${SERVER_ID}_shade19\",\"identifiers\":[\"mqtt_espsomfyrts_${SERVER_ID}\"]}" -r

mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${DISCO_TOPIC}/switch/19/config" \
  -m "{\"name\":\"Old Ambiguous Switch\",\"unique_id\":\"mqtt_${SERVER_ID}_switch19\",\"identifiers\":[\"mqtt_espsomfyrts_${SERVER_ID}\"]}" -r

# 7. Seguridad Multi-Dispositivo (Formato viejo perteneciente a OTRO equipo) -> DEBE CONSERVARSE
mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${DISCO_TOPIC}/cover/28/config" \
  -m '{"name":"Other Device Shade","unique_id":"mqtt_OTHERDEVICE_shade28","identifiers":["mqtt_espsomfyrts_OTHERDEVICE"]}' -r

mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" \
  -t "${DISCO_TOPIC}/switch/28/config" \
  -m '{"name":"Other Device Switch","unique_id":"mqtt_OTHERDEVICE_switch28","identifiers":["mqtt_espsomfyrts_OTHERDEVICE"]}' -r

# 8. Modo N Persianas (19 tópicos por persiana)
if [ "${NUM_SHADES}" -gt 0 ]; then
  echo ""
  echo "[*] Inyectando 19 tópicos para ${NUM_SHADES} persianas..."
  for (( id=START_ID; id<START_ID+NUM_SHADES; id++ )); do
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/shadeId" -m "${id}" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/name" -m "Shade ${id}" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/remoteAddress" -m "1000${id}" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/shadeType" -m "0" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/tiltType" -m "0" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/flags" -m "0" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/flipCommands" -m "false" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/flipPosition" -m "false" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/position" -m "100" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/direction" -m "0" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/target" -m "100" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/lastRollingCode" -m "10${id}" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/mypos" -m "-1" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/myTiltPos" -m "-1" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/tiltDirection" -m "0" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/tiltPosition" -m "0" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/tiltTarget" -m "0" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/windy" -m "false" -r
    mosquitto_pub -h "${BROKER_HOST}" -p "${BROKER_PORT}" -t "${ROOT_TOPIC}/shades/${id}/sunny" -m "false" -r
    echo "  -> Persiana ${id} (19 tópicos publicados)"
  done
fi

echo ""
echo "[+] Mensajes de prueba inyectados con éxito."
echo "[*] Reinicia el ESP32 para verificar la purga reactiva en el monitor serie."

