#!/usr/bin/env python3

import os
import sys
import re
import shutil
import subprocess

# ─────────────────────────────────────────────────────────────
# 🎯 CONFIGURACIÓN GENERAL — Variables definidas por el user para el servidor
# ─────────────────────────────────────────────────────────────

only_print_variables = False

# Ruta física en el servidor remoto donde se alojan los archivos OTA
remote_path = "/home/apachebcn/web/otasensors.pedroreinarojas.com/public_html"

# Datos de conexión SSH
remote_host = "38.242.239.189"
remote_user = "root"
remote_port = "88"

# Archivos locales generados por la compilación (ADAPTADO PARA ARDUINO IDE/ESP8266)
local_pathfile_built_bin = './build/esp8266.esp8266.generic/esp8266.ino.bin'
target_server_desired_file_bin = 'Firmware.bin'
target_server_desired_file_version_txt = 'Firmware.version.txt'

# Fichero de versión (en la raíz)
version_file = './version.h'

# Archivos locales generados por la compilación
local_pathfile_bin = f'./build/{target_server_desired_file_bin}'
local_pathfile_version_txt = f'./build/{target_server_desired_file_version_txt}'

# ─────────────────────────────────────────────────────────────
# 🧠 FUNCIONES DE EXTRACCIÓN DE VARIABLES
# ─────────────────────────────────────────────────────────────

def extract_define_value(file_path, define_name):
    """Extrae el valor de un #define simple"""
    with open(file_path, 'r') as f:
        for line in f:
            match = re.match(rf'#define\s+{define_name}\s+(.*)', line)
            if match:
                value = match.group(1).strip()
                if value.startswith('"') and value.endswith('"'):
                    value = value[1:-1]
                return value
    return None

# ─────────────────────────────────────────────────────────────
# 🚧 VERIFICACIÓN DE ENTORNO
# ─────────────────────────────────────────────────────────────

if not os.path.isfile(local_pathfile_built_bin):
    print(f"❌ ERROR: El binario {local_pathfile_built_bin} no existe. Compila primero el proyecto antes de ejecutar este script.")
    sys.exit(1)

# ─────────────────────────────────────────────────────────────
# 📤 EXTRACCIÓN DE VARIABLES DEL PROYECTO
# ─────────────────────────────────────────────────────────────

version = extract_define_value(version_file, 'VERSION')

# ─────────────────────────────────────────────────────────────
# 📋 MOSTRAR VARIABLES
# ─────────────────────────────────────────────────────────────

GREEN = "\033[92m"
RED = "\033[31m"
RESET = "\033[0m"

print("\n\r🔍 Variables capturadas del proyecto:")
print(f"{GREEN}VERSION:{RESET} {version}")
print(f"{GREEN}REMOTE_PATH:{RESET} {remote_path}")
if (only_print_variables):
    print(f"{RED}END WITH ONLY PRINT VARIABLES{RESET}")
    sys.exit(0)

# ─────────────────────────────────────────────────────────────
# 🛠️ PREPARACIÓN DE ARCHIVOS LOCALES
# ─────────────────────────────────────────────────────────────

print("\n\r🛠️ Preparando archivos locales...")
shutil.copy(local_pathfile_built_bin, local_pathfile_bin)
print(f"✅ Copiado: {local_pathfile_built_bin} → {local_pathfile_bin}")

with open(local_pathfile_version_txt, 'w') as f:
    f.write(version + '\n\r')

print(f"✅ Generado: {local_pathfile_version_txt} con versión {version}")

# ─────────────────────────────────────────────────────────────
# 📦 EJECUCIÓN DE RSYNC CON SALIDA EN TIEMPO REAL
# ─────────────────────────────────────────────────────────────

def run_rsync(local_file, remote_file, remote_dir, remote_host, remote_user, remote_port):
    """Ejecuta rsync y muestra salida en tiempo real"""
    remote_full_path = f"{remote_user}@{remote_host}:{remote_dir}/{remote_file}"
    cmd = [
        "rsync", "-avz", "--inplace", "--checksum",
        "-e", f"ssh -p {remote_port}",
        local_file,
        remote_full_path
    ]
    print(f"\n\r📦 Ejecutando rsync:")
    print(' '.join(cmd))
    try:
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in process.stdout:
            print(line.strip())
        process.wait()
        if process.returncode == 0:
            print("✅ rsync completado con éxito.")
        else:
            print(f"❌ Error en rsync. Código de salida: {process.returncode}")
            sys.exit(1)
    except Exception as e:
        print(f"❌ Excepción durante rsync: {e}")
        sys.exit(1)

print("\n\r📤 Subiendo archivos con rsync:")
run_rsync(local_pathfile_bin, "Firmware.bin", remote_path, remote_host, remote_user, remote_port)
run_rsync(local_pathfile_version_txt, "Firmware.version.txt", remote_path, remote_host, remote_user, remote_port)

# ─────────────────────────────────────────────────────────────
# 🌐 MOSTRAR URL PÚBLICA DEL FIRMWARE
# ─────────────────────────────────────────────────────────────

ota_url = "https://otasensors.pedroreinarojas.com"
print("\n\r🌐 URL pública del firmware:")
print(f"{ota_url}/Firmware.bin")
print()
print("RECUERDA DAR EL AVISO A LA WEBAPP PARA LAS NOTIFICACIONES DE NUEVO FIRMWARE")
