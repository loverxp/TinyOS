"""User program tests: hello, filetest."""
import common

TAG = "user"
DISPLAY_NAMES = ["hello", "filetest"]


def run(sock, selected, results, logs):
    # ---- hello ----
    if common.should_run(["hello"], selected):
        print("\n=== Test: hello ===")
        out = common.send_cmd(sock, "hello", 8)
        print(out.strip()[:500])
        logs["hello_raw"] = out.strip()
        results["hello"] = "finished" in out.lower() or "Hello" in out

        print("\n=== meminfo (after hello) ===")
        out2 = common.send_cmd(sock, "meminfo", 8)
        print(out2.strip()[:500])
        logs["meminfo_after_raw"] = out2.strip()

    # ---- filetest ----
    if common.should_run(["filetest"], selected):
        print("\n=== Test: filetest ===")
        out = common.send_cmd(sock, "filetest", 12)
        print(out.strip()[:2000])
        logs["filetest_raw"] = out.strip()
        crashed = "Page Fault" in out or "EXCEPTION" in out or "halted" in out
        has_alloc_err = "Failed to allocate" in out
        has_tests = "Test" in out
        results["filetest"] = has_tests and not crashed and not has_alloc_err

        analysis = []
        if crashed:
            analysis.append("Crashed (Page Fault/Exception)")
        if has_alloc_err:
            analysis.append("User stack allocation failed")
        if not has_tests:
            analysis.append("No test markers ('Test') found in output")
        logs["filetest_analysis"] = "; ".join(analysis) if analysis else "Check raw log"

        if crashed or has_alloc_err:
            print(f"\n  [crash={crashed}, alloc_err={has_alloc_err}]")
            return True  # signal caller to restart QEMU
    return False