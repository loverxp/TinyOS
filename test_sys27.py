"""Test syscall 27 via TCP serial."""
import socket
import subprocess
import time
import sys
import os

# Load local config (create local_config.py from local_config.example.py)
QEMU_PATH = ""
try:
    from local_config import QEMU_PATH, PROJECT_DIR
except ImportError:
    QEMU_PATH = os.environ.get("QEMU_PATH", "")
    PROJECT_DIR = os.environ.get("PROJECT_DIR", "")

KERNEL = "build/tinyos.bin"
TCP_PORT = 14444

qemu = subprocess.Popen(
    [QEMU_PATH, "-kernel", KERNEL, "-m", "32", "-vga", "std",
     "-serial", f"tcp::{TCP_PORT},server,nowait",
     "-drive", "file=disk.img,format=raw,if=ide",
     "-netdev", "user,id=net0", "-device", "ne2k_pci,netdev=net0"],
    stdin=subprocess.DEVNULL,
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    cwd=PROJECT_DIR if PROJECT_DIR else None
)

print("Waiting for QEMU...")
time.sleep(3)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10)
try:
    sock.connect(("127.0.0.1", TCP_PORT))
    print("Connected to QEMU serial")
except Exception as e:
    print(f"Failed to connect: {e}")
    qemu.kill()
    sys.exit(1)

def read_output(timeout=1.5):
    """Read and print available output."""
    sock.settimeout(timeout)
    data = b""
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk: break
            data += chunk
            sock.settimeout(0.3)
    except socket.timeout:
        pass
    if data:
        # Filter printable lines
        for line in data.decode(errors='replace').split('\n'):
            line = line.strip()
            if line:
                print(f"  {line}")
    return data

# Wait for boot
time.sleep(1)
read_output(3)

# Send Enter to get prompt
sock.sendall(b"\r\n")
time.sleep(1)
read_output(1)

# Test uptime
print("\n--- Testing uptime ---")
sock.sendall(b"uptime\r\n")
time.sleep(3)
read_output(3)

# Test date
print("\n--- Testing date ---")
sock.sendall(b"date\r\n")
time.sleep(2)
read_output(2)

# Test rand
print("\n--- Testing rand ---")
sock.sendall(b"rand\r\n")
time.sleep(2)
read_output(2)

# Test meminfo
print("\n--- Testing meminfo ---")
sock.sendall(b"meminfo\r\n")
time.sleep(2)
read_output(2)

# Test diskinfo
print("\n--- Testing diskinfo ---")
sock.sendall(b"diskinfo\r\n")
time.sleep(2)
read_output(2)

sock.close()
qemu.kill()
print("\nDone!")