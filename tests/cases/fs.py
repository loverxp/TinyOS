"""Filesystem tests: partitions, ls, cat, write+cat, more."""
import common

TAG = "fs"
DISPLAY_NAMES = ["partitions", "ls", "cat", "write+cat", "more", "more_file"]


def run(sock, selected, results, logs):
    # ---- partitions ----
    if common.should_run(["partitions"], selected):
        print("\n=== Test: partitions ===")
        out = common.send_cmd(sock, "partitions", 4)
        print(out.strip()[:400])
        logs["partitions_raw"] = out.strip()
        results["partitions"] = "partition" in out.lower() or "MBR" in out

    # ---- ls ----
    if common.should_run(["ls"], selected):
        print("\n=== Test: ls ===")
        out = common.send_cmd(sock, "ls", 4)
        print(out.strip()[:400])
        logs["ls_raw"] = out.strip()
        results["ls"] = "README" in out or "item" in out

    # ---- cat ----
    if common.should_run(["cat"], selected):
        print("\n=== Test: cat ===")
        out = common.send_cmd(sock, "cat readme.txt", 4)
        print(out.strip()[:300])
        logs["cat_raw"] = out.strip()
        results["cat"] = "Welcome" in out or "TinyOS" in out

    # ---- write+cat ----
    if common.should_run(["write"], selected):
        print("\n=== Test: write+cat ===")
        out = common.send_cmd(sock, "write t2.txt pmem_ok", 3)
        out2 = common.send_cmd(sock, "cat t2.txt", 4)
        print(out2.strip()[:200])
        logs["write+cat_raw"] = f"write output:\n{out.strip()}\ncat output:\n{out2.strip()}"
        results["write+cat"] = "pmem_ok" in out2

    # ---- more (file mode - no keyboard input needed) ----
    if common.should_run(["more"], selected):
        print("\n=== Test: more ===")
        out = common.send_cmd(sock, "more", 3)
        print(out.strip()[:200])
        logs["more_raw"] = out.strip()
        results["more"] = "Usage" in out and "filename" in out

        # Also test file mode (small file, no paging needed)
        out2 = common.send_cmd(sock, "more readme.txt", 4)
        logs["more_file_raw"] = out2.strip()
        results["more_file"] = "Welcome" in out2 or "TinyOS" in out2
        if results.get("more_file"):
            print(f"  more file mode: PASS (readme.txt displayed)")
        else:
            print(f"  more file mode: FAIL")