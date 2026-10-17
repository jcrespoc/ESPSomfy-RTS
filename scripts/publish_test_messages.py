#!/usr/bin/env python3
"""
Script para inyectar y verificar mensajes retenidos (retained) de prueba en el broker MQTT.
Permite validar la limpieza reactiva del firmware ESPSomfy RTS sin dependencias externas (usa sockets puros).
"""

import sys
import socket
import struct
import argparse
import time

def encode_remaining_length(length: int) -> bytes:
    encoded = bytearray()
    while True:
        b = length % 128
        length = length // 128
        if length > 0:
            b |= 0x80
        encoded.append(b)
        if length == 0:
            break
    return bytes(encoded)

class SimpleMQTTClient:
    def __init__(self, host: str, port: int, username: str = None, password: str = None, client_id: str = "mqtt-test-injector"):
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.client_id = client_id
        self.sock = None

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((self.host, self.port))

        var_header = b"\x00\x04MQTT\x04"
        flags = 0x02  # CleanSession
        if self.username: flags |= 0x80
        if self.password: flags |= 0x40
        var_header += bytes([flags, 0x00, 0x3C])  # keepalive 60s

        payload = struct.pack(">H", len(self.client_id)) + self.client_id.encode("utf-8")
        if self.username:
            payload += struct.pack(">H", len(self.username)) + self.username.encode("utf-8")
        if self.password:
            payload += struct.pack(">H", len(self.password)) + self.password.encode("utf-8")

        packet = b"\x10" + encode_remaining_length(len(var_header) + len(payload)) + var_header + payload
        self.sock.sendall(packet)

        resp = self.sock.recv(4)
        if len(resp) >= 4 and resp[0] == 0x20 and resp[3] == 0x00:
            return True
        raise ConnectionError(f"Error conectando al broker MQTT (CONNACK error code: {resp[3] if len(resp)>=4 else 'desconocido'})")

    def publish(self, topic: str, payload: str, retain: bool = True):
        topic_bytes = topic.encode("utf-8")
        payload_bytes = payload.encode("utf-8")
        var_header = struct.pack(">H", len(topic_bytes)) + topic_bytes
        fixed_header = 0x31 if retain else 0x30  # 0x30 = PUBLISH QoS 0, +1 if retain
        packet = bytes([fixed_header]) + encode_remaining_length(len(var_header) + len(payload_bytes)) + var_header + payload_bytes
        self.sock.sendall(packet)

    def disconnect(self):
        if self.sock:
            try:
                self.sock.sendall(b"\xE0\x00")
                self.sock.close()
            except Exception:
                pass
            self.sock = None

def get_test_cases(server_id: str, root_topic: str, disco_topic: str):
    return [
        {
            "category": "1. Bug v2.5.2 (Discovery bajo rootTopic)",
            "topic": f"{root_topic}/{disco_topic}/cover/{server_id}/1/config",
            "payload": '{"name":"Shade 1","unique_id":"old_bug_cover1"}',
            "expect": "DEBE SER PURGADO (0 bytes)",
        },
        {
            "category": "1. Bug v2.5.2 (Discovery switch bajo rootTopic)",
            "topic": f"{root_topic}/{disco_topic}/switch/{server_id}/2/config",
            "payload": '{"name":"Switch 2","unique_id":"old_bug_switch2"}',
            "expect": "DEBE SER PURGADO (0 bytes)",
        },
        {
            "category": "2. Persiana Huérfana / Eliminada (ID 99 inexistente)",
            "topic": f"{root_topic}/shades/99/position",
            "payload": "50",
            "expect": "DEBE SER PURGADO (0 bytes)",
        },
        {
            "category": "2. Persiana Huérfana / Eliminada (ID 99 inexistente)",
            "topic": f"{root_topic}/shades/99/name",
            "payload": "Persiana Fantasma 99",
            "expect": "DEBE SER PURGADO (0 bytes)",
        },
        {
            "category": "3. Persiana ACTIVA (ID 1 existente en el controlador)",
            "topic": f"{root_topic}/shades/1/position",
            "payload": "100",
            "expect": "DEBE CONSERVARSE (NO purgar)",
        },
        {
            "category": "4. Grupo Huérfano / Eliminado (ID 99 inexistente)",
            "topic": f"{root_topic}/groups/99/direction",
            "payload": "0",
            "expect": "DEBE SER PURGADO (0 bytes)",
        },
        {
            "category": "5. Discovery huérfano con serverId (Persiana 99 inexistente)",
            "topic": f"{disco_topic}/cover/{server_id}/99/config",
            "payload": '{"name":"Ghost Shade 99","unique_id":"ghost_99"}',
            "expect": "DEBE SER PURGADO (0 bytes)",
        },
        {
            "category": "5. Discovery huérfano con serverId (Switch 99 inexistente)",
            "topic": f"{disco_topic}/switch/{server_id}/99/config",
            "payload": '{"name":"Ghost Switch 99","unique_id":"ghost_switch_99"}',
            "expect": "DEBE SER PURGADO (0 bytes)",
        },
        {
            "category": "6. Desambiguación propia (Formato viejo sin serverId perteneciente a este equipo)",
            "topic": f"{disco_topic}/cover/99/config",
            "payload": f'{{"name":"Old Ambiguous Cover","unique_id":"mqtt_{server_id}_shade99","identifiers":["mqtt_espsomfyrts_{server_id}"]}}',
            "expect": "DEBE SER PURGADO (Contiene nuestro serverId en payload)",
        },
        {
            "category": "6. Desambiguación propia switch (Formato viejo sin serverId perteneciente a este equipo)",
            "topic": f"{disco_topic}/switch/99/config",
            "payload": f'{{"name":"Old Ambiguous Switch","unique_id":"mqtt_{server_id}_switch99","identifiers":["mqtt_espsomfyrts_{server_id}"]}}',
            "expect": "DEBE SER PURGADO (Contiene nuestro serverId en payload)",
        },
        {
            "category": "7. Seguridad Multi-Dispositivo (Formato viejo perteneciente a OTRO equipo)",
            "topic": f"{disco_topic}/cover/88/config",
            "payload": '{"name":"Other Device Shade","unique_id":"mqtt_OTHERDEVICE_shade88","identifiers":["mqtt_espsomfyrts_OTHERDEVICE"]}',
            "expect": "DEBE CONSERVARSE (NO pertenece a este serverId)",
        },
        {
            "category": "7. Seguridad Multi-Dispositivo switch (Formato viejo perteneciente a OTRO equipo)",
            "topic": f"{disco_topic}/switch/88/config",
            "payload": '{"name":"Other Device Switch","unique_id":"mqtt_OTHERDEVICE_switch88","identifiers":["mqtt_espsomfyrts_OTHERDEVICE"]}',
            "expect": "DEBE CONSERVARSE (NO pertenece a este serverId)",
        },
    ]

# 19 tópicos publicados por persiana por ESPSomfy RTS (SomfyShade::publish y SomfyShade::publishState)
SHADE_PROPERTIES = [
    ("shadeId", "{id}"),
    ("name", "Shade {id}"),
    ("remoteAddress", "1000{id:02d}"),
    ("shadeType", "0"),
    ("tiltType", "0"),
    ("flags", "0"),
    ("flipCommands", "false"),
    ("flipPosition", "false"),
    ("position", "100"),
    ("direction", "0"),
    ("target", "100"),
    ("lastRollingCode", "10{id:02d}"),
    ("mypos", "-1"),
    ("myTiltPos", "-1"),
    ("tiltDirection", "0"),
    ("tiltPosition", "0"),
    ("tiltTarget", "0"),
    ("windy", "false"),
    ("sunny", "false"),
]

def get_n_shades_cases(root_topic: str, num_shades: int, start_id: int = 1):
    cases = []
    for shade_id in range(start_id, start_id + num_shades):
        for prop, val_template in SHADE_PROPERTIES:
            payload = val_template.format(id=shade_id)
            cases.append({
                "category": f"Persiana {shade_id} ({prop})",
                "topic": f"{root_topic}/shades/{shade_id}/{prop}",
                "payload": payload,
                "expect": f"Tópico {prop} de persiana {shade_id}",
            })
    return cases

def main():
    parser = argparse.ArgumentParser(description="Publicador de mensajes de prueba para validación de MQTTPurgeManager")
    parser.add_argument("--host", default="localhost", help="Host del broker MQTT (defecto: localhost)")
    parser.add_argument("--port", type=int, default=1883, help="Puerto del broker MQTT (defecto: 1883)")
    parser.add_argument("-u", "--user", default=None, help="Usuario MQTT (opcional)")
    parser.add_argument("-P", "--password", default=None, help="Contraseña MQTT (opcional)")
    parser.add_argument("--server-id", default="7DF504", help="Server ID del ESPSomfy RTS a probar (defecto: 7DF504)")
    parser.add_argument("--root-topic", default=None, help="Root topic (defecto: espsomfy-<server-id>)")
    parser.add_argument("--disco-topic", default="homeassistant", help="Topic discovery (defecto: homeassistant)")
    parser.add_argument("-n", "--shades", type=int, default=None, help="Genera los 19 tópicos para N persianas (ej. -n 8)")
    parser.add_argument("--start-shade-id", type=int, default=1, help="ID inicial de persiana al usar -n/--shades (defecto: 1)")
    parser.add_argument("--with-cases", action="store_true", help="Al usar -n/--shades, incluye también los casos estándar de prueba")
    parser.add_argument("--delay", type=float, default=None, help="Pausa en segundos entre mensajes (defecto: automático)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Muestra el detalle completo de cada mensaje publicado")
    parser.add_argument("--clear", action="store_true", help="En lugar de inyectar datos, vacía manualmente los topics enviando payload vacío con retain")

    args = parser.parse_args()
    root_topic = args.root_topic if args.root_topic else f"espsomfy-{args.server_id}"

    client = SimpleMQTTClient(args.host, args.port, args.user, args.password)
    try:
        print(f"[*] Conectando al broker MQTT en {args.host}:{args.port}...")
        client.connect()
        print("[+] Conectado correctamente.\n")
    except Exception as e:
        print(f"[!] Error de conexión: {e}")
        sys.exit(1)

    if args.shades is not None and args.shades > 0:
        test_cases = get_n_shades_cases(root_topic, args.shades, args.start_shade_id)
        if args.with_cases:
            test_cases = get_test_cases(args.server_id, root_topic, args.disco_topic) + test_cases
    else:
        test_cases = get_test_cases(args.server_id, root_topic, args.disco_topic)

    action_label = "LIMPIANDO" if args.clear else "INYECTANDO"
    print(f"{'='*80}")
    print(f" {action_label} MENSAJES DE PRUEBA EN EL BROKER")
    print(f" Server ID objetivo: {args.server_id}")
    print(f" Root Topic:        {root_topic}")
    print(f" Discovery Topic:   {args.disco_topic}")
    if args.shades:
        print(f" Modo N Persianas:  {args.shades} persianas ({args.shades * 19} tópicos, IDs {args.start_shade_id}..{args.start_shade_id + args.shades - 1})")
    print(f" Total mensajes:    {len(test_cases)}")
    print(f"{'='*80}\n")

    delay = args.delay if args.delay is not None else (0.005 if len(test_cases) > 30 else 0.03)
    is_bulk = len(test_cases) > 20 and not args.verbose

    for i, tc in enumerate(test_cases, 1):
        topic = tc["topic"]
        payload = "" if args.clear else tc["payload"]

        client.publish(topic, payload, retain=True)

        if is_bulk:
            if i % 19 == 0 or i == len(test_cases):
                print(f"  -> [{i:4d}/{len(test_cases):4d}] {tc['category']}: publicado OK")
        else:
            print(f"[{i}/{len(test_cases)}] {tc['category']}")
            print(f"    Topic:    {topic}")
            if not args.clear:
                print(f"    Payload:  {payload}")
                print(f"    Esperado: {tc['expect']}")
            print("    -> Publicado con retain=true\n")

        if delay > 0:
            time.sleep(delay)

    client.disconnect()
    print(f"\n{'='*80}")
    if args.clear:
        print(f"[+] Todos los {len(test_cases)} topics han sido vaciados manualmente (0 bytes, retain=true).")
    else:
        print(f"[+] {len(test_cases)} mensajes inyectados con éxito en el broker.")
        print("[*] Pasos siguientes:")
        print("    1. Reinicia tu placa ESPSomfy RTS o conecta el monitor serie.")
        print("    2. Observa el comportamiento de MQTTPurgeManager al recibir los mensajes retenidos.")
    print(f"{'='*80}")

if __name__ == "__main__":
    main()

