"""Pipeline tests: help | more, etc."""
import common
import time

TAG = "pipeline"
DISPLAY_NAMES = ["pipeline", "help_more", "help_more_page"]


def run(sock, selected, results, logs):
    if not common.should_run([TAG], selected):
        return

    print("\n=== Test: help | more ===")
    common.recv_all(sock, timeout=0.5)
    sock.sendall(b"help | more\n")
    time.sleep(8)
    out = common.recv_all(sock, timeout=5)
    logs["help_more_raw"] = out.strip()

    # Check that help output appears (commands listed)
    has_cmds = any(cmd in out for cmd in ["echo", "cat", "ls", "help"])
    # Check that --More-- prompt appears
    has_prompt = "--More--" in out
    results["help_more"] = has_cmds and has_prompt
    print(f"  Command list: {has_cmds}")
    print(f"  --More-- prompt: {has_prompt}")

    if has_cmds and has_prompt:
        print(f"  help | more: PASS")
    else:
        print(f"  help | more: FAIL (cmds={has_cmds}, prompt={has_prompt})")

    # --- Interactive test: SPACE should page forward ---
    if not common.should_run(["help_more_page"], selected):
        return

    print("\n=== Test: help | more + SPACE ===")
    # Send SPACE to page forward
    sock.sendall(b" ")
    time.sleep(4)
    page2 = common.recv_all(sock, timeout=3)
    logs["help_more_page_raw"] = page2.strip()

    # Page 2 should show new help commands (different from page 1)
    has_page2_cmds = any(cmd in page2 for cmd in ["date", "rand", "ps", "kill", "arp", "webserver", "ping"])
    # Send 'q' to quit
    sock.sendall(b"q")
    time.sleep(2)
    after_q = common.recv_all(sock, timeout=2)
    shell_back = "TinyOS>" in after_q

    results["help_more_page"] = has_page2_cmds or (len(page2.strip()) > 100 and shell_back)
    print(f"  Page 2 output: {len(page2)} bytes, new cmds: {has_page2_cmds}")
    print(f"  Shell returned after 'q': {shell_back}")

    if results["help_more_page"]:
        print(f"  help | more + SPACE: PASS")
    else:
        print(f"  help | more + SPACE: FAIL")
