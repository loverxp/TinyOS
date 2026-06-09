"""Scheduler and signal tests: schedtest, kill."""
import time
import common

TAG = "sched"
DISPLAY_NAMES = ["schedtest", "kill"]


def run(sock, selected, results, logs):
    # ---- schedtest ----
    if common.should_run(["schedtest"], selected):
        print("\n=== Test: schedtest ===")
        out = common.send_cmd(sock, "schedtest 10", 3)
        print(out.strip()[:500])
        logs["schedtest_raw"] = out.strip()
        results["schedtest"] = "task" in out.lower() or "pid" in out.lower()

    # ---- kill ----
    if common.should_run(["kill"], selected):
        print("\n=== Test: kill ===")
        time.sleep(1)
        out = common.send_cmd(sock, "kill 1 9", 4)
        print(out.strip()[:500])
        logs["kill_raw"] = out.strip()
        results["kill"] = (
            "signal" in out.lower()
            or "sent" in out.lower()
            or "kill" in out.lower()
            or "pid" in out.lower()
        )
        time.sleep(10)
        common.recv_all(sock, timeout=2)