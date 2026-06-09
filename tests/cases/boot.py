"""Boot initialization checks."""
import common

TAG = "boot"
DISPLAY_NAMES = ["MBR init", "DMA init", "VFS init", "Signal init", "FAT16 init"]


def run(sock, selected, results, logs):
    if not common.should_run([TAG], selected):
        return

    print("--- Boot Checks ---")
    boot = common.recv_all(sock, timeout=15)

    checks = {
        "MBR init": "[MBR]" in boot,
        "DMA init": "[OK] ATA DMA mode enabled" in boot,
        "VFS init": "[OK] VFS initialized" in boot,
        "Signal init": "[OK] Signal" in boot,
        "FAT16 init": "[FAT16] Init OK" in boot,
    }
    for k, v in checks.items():
        results[k] = v
        logs[f"{k}_raw"] = boot[:1000]
        print(f"  {'[PASS]' if v else '[FAIL]'} {k}")