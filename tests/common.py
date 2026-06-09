"""Shared utilities for TinyOS integration tests: QEMU, serial, report."""
import socket
import time
import subprocess
import os
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

QEMU = r"D:\Program Files\qemu\qemu-system-i386.exe"
BASE_PORT = 4321
PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FAILURE_FILE = os.path.join(os.path.dirname(__file__), ".test_failures")


def start_qemu(port=None, disk=None):
    """Start QEMU with optional port and disk image.
    
    Args:
        port: Serial TCP port (default BASE_PORT)
        disk: Disk image path (default "disk.img")
    
    Returns:
        subprocess.Popen handle
    """
    if port is None:
        port = BASE_PORT
    if disk is None:
        disk = "disk.img"
    
    cmd = [
        QEMU, "-kernel", "build/tinyos.bin", "-m", "64",
        "-display", "none",
        "-drive", f"file={disk},format=raw,if=ide",
        "-netdev", "user,id=net0",
        "-device", "ne2k_pci,netdev=net0",
        "-serial", f"tcp:localhost:{port},server,wait",
        "-snapshot",  # Prevent disk image conflicts in parallel runs
    ]
    DETACHED = 0x00000008 | 0x00000200
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                            creationflags=DETACHED)


def connect(port=None):
    """Connect to QEMU serial at given port."""
    if port is None:
        port = BASE_PORT
    for _ in range(15):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(10)
            s.connect(("localhost", port))
            return s
        except:
            time.sleep(1)
    return None


def recv_all(sock, timeout=5):
    sock.settimeout(timeout)
    data = b""
    while True:
        try:
            chunk = sock.recv(8192)
            if not chunk:
                break
            data += chunk
        except:
            break
    return data.decode("utf-8", errors="replace")


def send_cmd(sock, cmd, wait=5):
    recv_all(sock, timeout=0.5)
    sock.sendall((cmd + "\n").encode())
    time.sleep(wait)
    return recv_all(sock, timeout=3)


def should_run(tags, selected):
    """Check if any tag in tags is selected. selected=None means run all."""
    if selected is None:
        return True
    return any(t in selected for t in tags)


def run_module_single(mod, worker_id, selected):
    """Run a single test case module in its own QEMU instance.
    
    Args:
        mod: Test case module (must have TAG, run, DISPLAY_NAMES)
        worker_id: Integer worker ID (0..N-1)
        selected: List of selected test tags/items, or None for all
    
    Returns:
        (tag, need_restart, results_dict, logs_dict)
    """
    port = BASE_PORT + worker_id + 1
    results = {}
    logs = {}

    os.chdir(PROJECT_DIR)
    proc = start_qemu(port=port)
    sock = connect(port=port)
    if not sock:
        print(f"[{mod.TAG}] FAIL: Connect failed on port {port}")
        proc.kill()
        return mod.TAG, False, results, logs

    print(f"[{mod.TAG}] OK (port {port})")
    time.sleep(8)

    need_restart = mod.run(sock, selected, results, logs)

    sock.close()
    proc.kill()
    try:
        proc.wait(timeout=5)
    except:
        pass

    return mod.TAG, need_restart, results, logs


def generate_report(results, logs, passed, failed, total):
    lines = []
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    lines.append("# TinyOS Feature Test Report")
    lines.append("")
    lines.append(f"**Date**: {now}")
    lines.append(f"**RAM**: 64MB")
    lines.append(f"**Result**: **{passed}/{total} passed**")
    lines.append("")

    lines.append("## Summary")
    lines.append("")
    lines.append("| # | Test | Result | Details |")
    lines.append("|---|------|--------|---------|")
    for i, (name, ok) in enumerate(results.items(), 1):
        status = "PASS" if ok else "FAIL"
        detail = logs.get(name, "")
        if not ok:
            detail = "See Failed Tests section"
        lines.append(f"| {i} | {name} | {status} | {detail} |")
    lines.append("")

    failed_items = [(n, o) for n, o in results.items() if not o]
    if failed_items:
        lines.append("## Failed Tests")
        lines.append("")
        for name, _ in failed_items:
            lines.append(f"### {name}")
            lines.append("")
            raw = logs.get(f"{name}_raw", "")
            if raw:
                lines.append("```")
                lines.append(raw)
                lines.append("```")
            lines.append("")
            analysis = logs.get(f"{name}_analysis", "")
            if analysis:
                lines.append(f"**Analysis**: {analysis}")
                lines.append("")

    lines.append("## Full Raw Output Log")
    lines.append("")
    for name, ok in results.items():
        status = "PASS" if ok else "FAIL"
        lines.append(f"### {name} [{status}]")
        lines.append("")
        raw = logs.get(f"{name}_raw", "")
        if raw:
            lines.append("```")
            lines.append(raw)
            lines.append("```")
        lines.append("")

    return "\n".join(lines)


def save_failures(results):
    failed = [n for n, ok in results.items() if not ok]
    with open(FAILURE_FILE, "w") as f:
        for name in failed:
            f.write(name + "\n")


def load_failures():
    if not os.path.exists(FAILURE_FILE):
        print("[ERROR] No failure record found. Run all tests first.")
        return None
    with open(FAILURE_FILE) as f:
        return [name.strip() for name in f if name.strip()]


def save_report(report_text):
    report_path = os.path.join(os.path.dirname(__file__), "test_report.md")
    with open(report_path, "w", encoding="utf-8") as f:
        f.write(report_text)
    print(f"\nReport saved: {report_path}")


def print_summary(results):
    p = f = 0
    for ok in results.values():
        p += ok
        f += not ok
    print("\n" + "=" * 60)
    print("RESULTS")
    print("=" * 60)
    for name, ok in results.items():
        print(f"  {'[PASS]' if ok else '[FAIL]'} {name}")
    print(f"\n{p}/{len(results)} passed")
    return p, f