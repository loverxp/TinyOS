"""Test forktest using QEMU with TCP telnet serial."""
import subprocess
import time
import os
import socket
import threading

QEMU = r"D:\Program Files\qemu\qemu-system-i386.exe"
KERNEL = "build/tinyos.bin"
TCP_PORT = 14444

os.makedirs("logs", exist_ok=True)

# Start QEMU with serial over TCP (telnet mode)
qemu = subprocess.Popen(
    [QEMU, "-kernel", KERNEL, "-m", "32",
     "-drive", "file=disk.img,format=raw,if=ide",
     "-netdev", "user,id=net0",
     "-device", "ne2k_pci,netdev=net0",
     "-serial", f"tcp::{TCP_PORT},server,nowait",
     "-vga", "std"],
    stdin=subprocess.DEVNULL,
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    cwd="d:\\Codes\\Learning\\TinyOS"
)

print(f"[test] QEMU started, serial TCP on port {TCP_PORT}")
time.sleep(2)

# Connect to QEMU serial port via TCP
print("[test] Connecting to serial...")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(5)

connected = False
for attempt in range(15):
    try:
        sock.connect(("127.0.0.1", TCP_PORT))
        connected = True
        print("[test] Connected!")
        break
    except (ConnectionRefusedError, ConnectionAbortedError) as e:
        print(f"[test] Retry {attempt+1}: {e}")
        time.sleep(2)
    except OSError as e:
        print(f"[test] Retry {attempt+1}: {e}")
        time.sleep(2)

if not connected:
    print("[test] FAILED to connect!")
    qemu.kill()
    exit(1)

# Read all data from serial
received = bytearray()

def receiver():
    sock.settimeout(0.5)
    while True:
        try:
            data = sock.recv(4096)
            if not data:
                break
            received.extend(data)
        except socket.timeout:
            continue
        except (ConnectionError, OSError):
            break

rthread = threading.Thread(target=receiver, daemon=True)
rthread.start()

# Wait for boot (long)
print("[test] Waiting 12 seconds for boot...")
time.sleep(12)

# Print boot output so far
boot_text = bytes(received).decode(errors="replace")
print(f"[test] Boot output ({len(boot_text)} chars):")
for line in boot_text.split("\n")[-20:]:
    print(f"  |{line.rstrip()}")

# Send forktest command
print("\n[test] Sending 'forktest' command...")
sock.sendall(b"forktest\r\n")

# Wait for execution
print("[test] Waiting 20 seconds...")
time.sleep(20)

# Save all data
with open("logs/serial_tcp.log", "wb") as f:
    f.write(bytes(received))

# Print relevant lines
all_text = bytes(received).decode(errors="replace")
print("\n[test] === FORK/SCHED/CHILD/PARENT LINES ===")
for line in all_text.split("\n"):
    lower = line.lower()
    if any(kw in lower for kw in ["fork", "sched", "child", "parent", "exit",
                                    "frame", "error", "bug", "exception",
                                    "fault", "panic", "sleep", "wake"]):
        print(f"  {line}")

print("\n[test] === LAST 40 LINES ===")
lines = all_text.split("\n")
print("\n".join(lines[-40:]))

# Clean up
sock.close()
qemu.kill()
qemu.wait(timeout=3)
print("\n[test] Done")