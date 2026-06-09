"""Help command test: verify all built-in commands are listed."""
import common

TAG = "help"
DISPLAY_NAMES = ["help"]


def run(sock, selected, results, logs):
    if not common.should_run([TAG], selected):
        return

    print("\n=== Test: help ===")
    out = common.send_cmd(sock, "help", 8)
    print(out.strip()[:1500])
    logs["help_raw"] = out.strip()
    results["help"] = "echo" in out and "cat" in out and "ls" in out and "dino" in out and "more" in out and "si" in out and "tinyhttpd" in out and "jobs" in out and "sh" in out

    if not results["help"]:
        missing = []
        if "echo" not in out:
            missing.append("echo")
        if "cat" not in out:
            missing.append("cat")
        if "ls" not in out:
            missing.append("ls")
        if "dino" not in out:
            missing.append("dino")
        if "more" not in out:
            missing.append("more")
        if "si" not in out:
            missing.append("si")
        if "tinyhttpd" not in out:
            missing.append("tinyhttpd")
        if "jobs" not in out:
            missing.append("jobs")
        if "sh" not in out:
            missing.append("sh")
        logs["help_analysis"] = f"Missing from help output: {', '.join(missing)}"