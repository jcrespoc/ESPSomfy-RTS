#!/usr/bin/env python3
"""
ws_log_client.py - Cliente de prueba WebSocket para ESPSomfy-RTS
================================================================
Cliente WebSocket autónomo (sin dependencias externas, usa librería estándar de Python).
Se conecta al puerto 8080 del ESP32, envía 'join:log' para suscribirse a los logs,
consume activamente el flujo de red para evitar saturar el buffer TCP del ESP32
y muestra los logs con formato y coloreado por severidad.

Uso:
    python3 ws_log_client.py <IP_DEL_ESP32> [opciones]

Ejemplos:
    python3 ws_log_client.py 192.168.1.150
    python3 ws_log_client.py 192.168.1.150 --apikey "mi_token_hex"
    python3 ws_log_client.py 192.168.1.150 --all          # Muestra también eventos de persianas/wifi
    python3 ws_log_client.py 192.168.1.150 -o logs.txt    # Guarda en archivo además de pantalla
    python3 ws_log_client.py 192.168.1.150 --filter WiFi  # Filtra logs que contengan 'WiFi'
"""

import sys
import os
import socket
import ssl
import struct
import base64
import hashlib
import json
import argparse
import time
import signal
import re

# Códigos de escape ANSI para colores en terminal
COLOR_RESET = "\033[0m"
COLOR_RED = "\033[31;1m"
COLOR_YELLOW = "\033[33;1m"
COLOR_GREEN = "\033[32m"
COLOR_CYAN = "\033[36m"
COLOR_GRAY = "\033[90m"
COLOR_MAGENTA = "\033[35m"
COLOR_BOLD = "\033[1m"


class WebSocketClient:
    """Cliente WebSocket RFC 6455 ligero sin dependencias externas."""

    OPCODE_CONT = 0x0
    OPCODE_TEXT = 0x1
    OPCODE_BIN = 0x2
    OPCODE_CLOSE = 0x8
    OPCODE_PING = 0x9
    OPCODE_PONG = 0xA

    def __init__(self, host: str, port: int = 8080, path: str = "/", timeout: float = 60.0, use_ssl: bool = False):
        self.host = host
        self.port = port
        self.path = path if path.startswith("/") else "/" + path
        self.timeout = timeout
        self.use_ssl = use_ssl
        self.sock = None
        self._connected = False

    def connect(self):
        raw_sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        if self.use_ssl:
            context = ssl.create_default_context()
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            self.sock = context.wrap_socket(raw_sock, server_hostname=self.host)
        else:
            self.sock = raw_sock

        # Desactivar retardo de Nagle para enviar tramas inmediatamente
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        # Generar clave para la negociación WebSocket (RFC 6455)
        ws_key = base64.b64encode(os.urandom(16)).decode("ascii")

        req = (
            f"GET {self.path} HTTP/1.1\r\n"
            f"Host: {self.host}:{self.port}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {ws_key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n"
            f"User-Agent: ws_log_client/1.0\r\n"
            f"\r\n"
        )
        self.sock.sendall(req.encode("latin1"))

        # Leer respuesta HTTP del servidor
        header_bytes = b""
        while b"\r\n\r\n" not in header_bytes:
            chunk = self.sock.recv(512)
            if not chunk:
                raise ConnectionError("El servidor cerró la conexión durante la negociación HTTP.")
            header_bytes += chunk

        head_text = header_bytes.split(b"\r\n\r\n", 1)[0].decode("latin1", errors="replace")
        status_line = head_text.splitlines()[0]
        if " 101 " not in status_line:
            raise ConnectionError(f"Fallo en la negociación WebSocket del servidor: {status_line}")

        self._connected = True

    def send_text(self, text: str):
        """Envía una trama de texto enmascarada según RFC 6455."""
        if not self._connected or not self.sock:
            return
        payload = text.encode("utf-8")
        self._send_frame(self.OPCODE_TEXT, payload)

    def _send_frame(self, opcode: int, payload: bytes):
        length = len(payload)
        mask_key = os.urandom(4)
        masked_payload = bytearray(b ^ mask_key[i % 4] for i, b in enumerate(payload))

        header = bytearray([0x80 | opcode])
        if length < 126:
            header.append(0x80 | length)
        elif length < 65536:
            header.append(0x80 | 126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(0x80 | 127)
            header.extend(struct.pack("!Q", length))

        frame = bytes(header + mask_key + masked_payload)
        self.sock.sendall(frame)

    def _recv_exact(self, num_bytes: int) -> bytes:
        data = bytearray()
        while len(data) < num_bytes:
            packet = self.sock.recv(num_bytes - len(data))
            if not packet:
                raise ConnectionResetError("Conexión cerrada por el servidor.")
            data.extend(packet)
        return bytes(data)

    def recv_message(self):
        """Lee la siguiente trama WebSocket recibida del servidor."""
        while self._connected:
            b0, b1 = struct.unpack("!BB", self._recv_exact(2))
            opcode = b0 & 0x0F
            is_masked = bool(b1 & 0x80)
            payload_len = b1 & 0x7F

            if payload_len == 126:
                payload_len = struct.unpack("!H", self._recv_exact(2))[0]
            elif payload_len == 127:
                payload_len = struct.unpack("!Q", self._recv_exact(8))[0]

            mask_key = self._recv_exact(4) if is_masked else None
            raw_payload = self._recv_exact(payload_len)

            if is_masked and mask_key:
                payload = bytes(b ^ mask_key[i % 4] for i, b in enumerate(raw_payload))
            else:
                payload = raw_payload

            # Gestión de control PING / PONG
            if opcode == self.OPCODE_PING:
                # Responder PONG inmediatamente para mantener la conexión viva
                self._send_frame(self.OPCODE_PONG, payload)
                continue
            elif opcode == self.OPCODE_PONG:
                continue
            elif opcode == self.OPCODE_CLOSE:
                self._connected = False
                return None, None
            elif opcode == self.OPCODE_TEXT:
                return opcode, payload.decode("utf-8", errors="replace")
            else:
                return opcode, payload

        return None, None

    def close(self):
        if self._connected and self.sock:
            try:
                self._send_frame(self.OPCODE_CLOSE, b"")
            except Exception:
                pass
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
                self.sock.close()
            except Exception:
                pass
        self._connected = False


def colorize_log(msg: str) -> str:
    """Añade color ANSI al mensaje de log según su severidad."""
    if "[E]" in msg or "ERROR" in msg or "Error" in msg:
        return f"{COLOR_RED}{msg}{COLOR_RESET}"
    elif "[W]" in msg or "WARN" in msg or "Warning" in msg:
        return f"{COLOR_YELLOW}{msg}{COLOR_RESET}"
    elif "[I]" in msg or "INFO" in msg:
        return f"{COLOR_GREEN}{msg}{COLOR_RESET}"
    elif "[D]" in msg or "DEBUG" in msg:
        return f"{COLOR_CYAN}{msg}{COLOR_RESET}"
    elif "[V]" in msg or "VERBOSE" in msg or "[T]" in msg:
        return f"{COLOR_GRAY}{msg}{COLOR_RESET}"
    return msg


def main():
    parser = argparse.ArgumentParser(
        description="Cliente de prueba WebSocket para la emisión de logs en ESPSomfy-RTS (puerto 8080)"
    )
    parser.add_argument("host", help="Dirección IP o hostname del ESP32")
    parser.add_argument("-p", "--port", type=int, default=8080, help="Puerto WebSocket (por defecto: 8080)")
    parser.add_argument("-k", "--apikey", help="Clave de API / Token si la seguridad está habilitada")
    parser.add_argument("-r", "--room", default="log", help="Nombre de sala a unirse (por defecto: 'log')")
    parser.add_argument("-a", "--all", action="store_true", help="Mostrar también eventos de estado (shades, wifi, fw)")
    parser.add_argument("-f", "--filter", help="Filtrar mensajes que contengan este texto (case-insensitive)")
    parser.add_argument("-o", "--output", help="Ruta de archivo para guardar los logs capturados")
    parser.add_argument("--raw", action="store_true", help="Mostrar tramas WebSocket sin procesar")
    parser.add_argument("--no-color", action="store_true", help="Desactivar colores en la consola")

    args = parser.parse_args()

    # Construir ruta con query apikey si se suministra
    path = "/"
    if args.apikey:
        path = f"/?apikey={args.apikey}"

    url_str = f"ws://{args.host}:{args.port}{path}"
    print(f"{COLOR_BOLD}=== ESPSomfy-RTS WebSocket Log Client ==={COLOR_RESET}")
    print(f"Conectando a {url_str} ...")

    client = WebSocketClient(args.host, port=args.port, path=path, timeout=60.0)

    # Manejar Ctrl+C de forma limpia
    interrupted = False

    def sig_handler(sig, frame):
        nonlocal interrupted
        interrupted = True
        print(f"\n{COLOR_YELLOW}Interrupción recibida, desconectando...{COLOR_RESET}")
        try:
            client.send_text(f"leave:{args.room}")
            time.sleep(0.1)
            client.close()
        except Exception:
            pass
        sys.exit(0)

    signal.signal(signal.SIGINT, sig_handler)

    try:
        client.connect()
        print(f"{COLOR_GREEN}✓ Conexión establecida con éxito.{COLOR_RESET}")
    except Exception as e:
        print(f"{COLOR_RED}✗ Error al conectar: {e}{COLOR_RESET}")
        print("\nConsejo:")
        print(" - Comprueba que la IP sea correcta y accesible.")
        print(" - Si la seguridad completa está activa, añade: --apikey <token>")
        return 1

    # Suscribirse a la sala de logs
    join_cmd = f"join:{args.room}"
    print(f"Enviando suscripción: '{join_cmd}' ...")
    client.send_text(join_cmd)
    print(f"{COLOR_CYAN}Esperando logs... (Pulsa Ctrl+C para salir){COLOR_RESET}\n" + "-" * 70)

    out_file = None
    if args.output:
        try:
            out_file = open(args.output, "a", encoding="utf-8")
            print(f"{COLOR_GRAY}[Guardando logs en: {args.output}]{COLOR_RESET}")
        except Exception as e:
            print(f"{COLOR_RED}Error al abrir archivo de salida: {e}{COLOR_RESET}")

    try:
        while not interrupted:
            opcode, msg = client.recv_message()
            if msg is None:
                print(f"{COLOR_YELLOW}Conexión cerrada por el servidor.{COLOR_RESET}")
                break

            if args.raw:
                print(f"[RAW] {msg}")
                continue

            # El servidor envía eventos en formato Socket.IO: 42["<evento>", ...]
            # O mensajes simples como "Connected"
            if msg.startswith('42["log",'):
                try:
                    parsed = json.loads(msg[2:])
                    log_text = parsed[1] if len(parsed) > 1 else str(parsed)
                except Exception:
                    log_text = msg[8:-1].replace('\\"', '"')

                # Aplicar filtro si se especificó
                if args.filter and args.filter.lower() not in log_text.lower():
                    continue

                if out_file:
                    out_file.write(log_text + "\n")
                    out_file.flush()

                if args.no_color:
                    print(log_text)
                else:
                    print(colorize_log(log_text))

            elif msg == "Connected":
                print(f"{COLOR_CYAN}[SERVER] {msg}{COLOR_RESET}")

            elif args.all:
                # Mostrar otros eventos (shadeState, wifiStrength, etc.)
                if msg.startswith("42["):
                    try:
                        parsed = json.loads(msg[2:])
                        event_name = parsed[0]
                        data = parsed[1] if len(parsed) > 1 else ""
                        print(f"{COLOR_MAGENTA}[EVENT:{event_name}]{COLOR_RESET} {data}")
                    except Exception:
                        print(f"{COLOR_MAGENTA}[EVENT]{COLOR_RESET} {msg}")
                else:
                    print(f"{COLOR_GRAY}{msg}{COLOR_RESET}")

    except (ConnectionResetError, BrokenPipeError):
        print(f"\n{COLOR_YELLOW}El servidor desconectó el socket.{COLOR_RESET}")
    except Exception as e:
        print(f"\n{COLOR_RED}Error durante la recepción: {e}{COLOR_RESET}")
    finally:
        if out_file:
            out_file.close()
        client.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())

