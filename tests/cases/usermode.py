"""User-mode migration tests: verify commands run in Ring 3 correctly."""
import common

TAG = "usermode"
DISPLAY_NAMES = ["ls_user", "cat_user", "mkdir_rmdir", "write_rm", "pci_user",
                 "net_user", "arp_user", "netstat_user"]


def _ok(out):
    """Check user program ran without crashing: user_exit present, no exceptions."""
    crashed = "Page Fault" in out or "EXCEPTION" in out or "halted" in out
    exited = "user_exit" in out
    return exited and not crashed


def run(sock, selected, results, logs):
    restart = False

    # ---- ls (user-mode) ----
    if common.should_run(["ls_user"], selected):
        print("\n=== Test: ls (user-mode) ===")
        out = common.send_cmd(sock, "ls", 5)
        print(out.strip()[:400])
        logs["ls_user_raw"] = out.strip()
        results["ls_user"] = _ok(out)

    # ---- cat (user-mode) ----
    if common.should_run(["cat_user"], selected):
        print("\n=== Test: cat (user-mode) ===")
        out = common.send_cmd(sock, "cat readme.txt", 5)
        print(out.strip()[:300])
        logs["cat_user_raw"] = out.strip()
        results["cat_user"] = _ok(out)

    # ---- mkdir + rmdir (user-mode) ----
    if common.should_run(["mkdir_rmdir"], selected):
        print("\n=== Test: mkdir + rmdir (user-mode) ===")
        out_mk = common.send_cmd(sock, "mkdir USRTEST", 3)
        out_ls = common.send_cmd(sock, "ls", 5)
        out_rm = common.send_cmd(sock, "rmdir USRTEST", 3)
        out_ls2 = common.send_cmd(sock, "ls", 5)
        print(f"mkdir: {out_mk.strip()[:200]}\nrmdir: {out_rm.strip()[:200]}")
        logs["mkdir_rmdir_raw"] = f"mkdir:\n{out_mk}\nls:\n{out_ls}\nrmdir:\n{out_rm}\nls2:\n{out_ls2}"
        mk_ok = _ok(out_mk) and "Created directory" in out_mk
        rm_ok = _ok(out_rm) and "Removed directory" in out_rm
        results["mkdir_rmdir"] = mk_ok and rm_ok

    # ---- write + rm (user-mode) ----
    if common.should_run(["write_rm"], selected):
        print("\n=== Test: write + rm (user-mode) ===")
        out_wr = common.send_cmd(sock, "write umtest.txt ring3data", 4)
        out_cat = common.send_cmd(sock, "cat umtest.txt", 5)
        out_rm = common.send_cmd(sock, "rm umtest.txt", 3)
        print(f"write: {out_wr.strip()[:200]}\ncat: {out_cat.strip()[:200]}\nrm: {out_rm.strip()[:200]}")
        logs["write_rm_raw"] = f"write:\n{out_wr}\ncat:\n{out_cat}\nrm:\n{out_rm}"
        wr_ok = _ok(out_wr) and "Wrote" in out_wr
        cat_ok = _ok(out_cat)
        rm_ok = _ok(out_rm)
        results["write_rm"] = wr_ok and cat_ok and rm_ok

    # ---- pci (user-mode) ----
    if common.should_run(["pci_user"], selected):
        print("\n=== Test: pci (user-mode) ===")
        out = common.send_cmd(sock, "pci", 5)
        print(out.strip()[:500])
        logs["pci_user_raw"] = out.strip()
        results["pci_user"] = _ok(out)

    # ---- net (user-mode) ----
    if common.should_run(["net_user"], selected):
        print("\n=== Test: net (user-mode) ===")
        out = common.send_cmd(sock, "net", 5)
        print(out.strip()[:500])
        logs["net_user_raw"] = out.strip()
        results["net_user"] = _ok(out)

    # ---- arp (user-mode) ----
    if common.should_run(["arp_user"], selected):
        print("\n=== Test: arp (user-mode) ===")
        out = common.send_cmd(sock, "arp", 5)
        print(out.strip()[:500])
        logs["arp_user_raw"] = out.strip()
        results["arp_user"] = _ok(out)

    # ---- netstat (user-mode) ----
    if common.should_run(["netstat_user"], selected):
        print("\n=== Test: netstat (user-mode) ===")
        out = common.send_cmd(sock, "netstat", 5)
        print(out.strip()[:500])
        logs["netstat_user_raw"] = out.strip()
        results["netstat_user"] = _ok(out)

    return restart
