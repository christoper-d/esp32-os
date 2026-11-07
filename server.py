#!/usr/bin/env python3
import http.server
import socketserver
import socket
import os
import sys

PORT = 8080
DIRECTORY = os.path.dirname(os.path.abspath(__file__))

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

def get_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

if __name__ == "__main__":
    local_ip = get_ip()
    print("=" * 60)
    print(" 🚀 SIMULADOR TFT_eSPI 1.69\" IPS (240x280) INICIADO")
    print("=" * 60)
    print(f"\n 👉 Abre en tu navegador local (en este dispositivo):")
    print(f"    http://localhost:{PORT}")
    print(f"\n 👉 O desde tu PC / otro dispositivo en la misma red Wi-Fi:")
    print(f"    http://{local_ip}:{PORT}")
    print("\n Presiona Ctrl+C para detener el servidor.\n")
    print("=" * 60)
    
    # Allow address reuse
    socketserver.TCPServer.allow_reuse_address = True
    try:
        with socketserver.TCPServer(("", PORT), Handler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\nServidor detenido. ¡Hasta la próxima!")
        sys.exit(0)
