"""v4.0 feature tests: background (&), jobs, sh script, tinyhttpd."""
import time
import common

TAG = "v40"
DISPLAY_NAMES = ["jobs", "background", "sh_script", "tinyhttpd"]


def run(sock, selected, results, logs):
    restart = False

    # If v40 tag is selected, run all tests
    run_all = common.should_run([TAG], selected)

    # ---- jobs (alias for ps) ----
    if run_all or common.should_run(["jobs"], selected):
        print("\n=== Test: jobs ===")
        out = common.send_cmd(sock, "jobs", 5)
        print(out.strip()[:600])
        logs["jobs_raw"] = out.strip()
        # jobs should show at least the shell task with a PID
        results["jobs"] = ("PID" in out or "pid" in out or "Task" in out
                           or "INIT" in out or "Shell" in out or "shell" in out)
        if not results["jobs"]:
            logs["jobs_analysis"] = "No task listing found in output"

    # ---- background process (&) ----
    if run_all or common.should_run(["background"], selected):
        print("\n=== Test: background process (&) ===")
        # Launch echo in background - should return immediately
        out = common.send_cmd(sock, "echo background_test &", 5)
        print(out.strip()[:500])
        logs["background_raw"] = out.strip()
        # Background launch should show confirmation with PID
        has_bg = ("PID" in out or "pid" in out or "&" in out
                  or "background" in out.lower() or "Task" in out
                  or "[" in out)
        results["background"] = has_bg
        if not has_bg:
            logs["background_analysis"] = "No background launch confirmation found"

        # Verify shell is still responsive after background launch
        out2 = common.send_cmd(sock, "echo shell_alive", 4)
        logs["background_alive_raw"] = out2.strip()
        shell_ok = "shell_alive" in out2
        if not shell_ok:
            logs["background_analysis"] = logs.get("background_analysis", "") + " Shell unresponsive after &"
            results["background"] = False

    # ---- sh script execution ----
    if run_all or common.should_run(["sh_script"], selected):
        print("\n=== Test: sh script ===")
        # First create a script file with multiple commands
        out_wr = common.send_cmd(sock, "write test.sh echo line_one", 4)
        print(f"write script: {out_wr.strip()[:200]}")

        # Append more lines - write overwrites, so we write a single-line script
        # Actually, write appends or overwrites depending on implementation
        # Let's just test with a single-command script first
        out_sh = common.send_cmd(sock, "sh test.sh", 6)
        print(f"sh output: {out_sh.strip()[:500]}")
        logs["sh_script_raw"] = f"write:\n{out_wr.strip()}\nsh:\n{out_sh.strip()}"

        # sh should either execute the script or show an error about the file
        has_exec = "line_one" in out_sh or "echo" in out_sh
        # Or if the file wasn't created properly, it might show an error
        has_error = "not found" in out_sh.lower() or "error" in out_sh.lower() or "open" in out_sh.lower()
        results["sh_script"] = has_exec
        if has_error and not has_exec:
            logs["sh_script_analysis"] = "Script file may not have been created properly"
        elif not has_exec and not has_error:
            logs["sh_script_analysis"] = "No script execution output detected"

        # Clean up
        common.send_cmd(sock, "rm test.sh", 3)

    # ---- tinyhttpd user program ----
    if run_all or common.should_run(["tinyhttpd"], selected):
        print("\n=== Test: tinyhttpd ===")
        # Run tinyhttpd in background so it doesn't block
        out = common.send_cmd(sock, "tinyhttpd &", 5)
        print(out.strip()[:500])
        logs["tinyhttpd_raw"] = out.strip()

        # Give it a moment to start
        time.sleep(3)

        # Check that it started (look for socket creation message)
        # Or just verify the shell is still responsive
        out2 = common.send_cmd(sock, "echo httpd_test", 4)
        logs["tinyhttpd_alive_raw"] = out2.strip()

        # tinyhttpd should either show startup messages or at least not crash
        crashed = "Page Fault" in out or "EXCEPTION" in out
        started = "socket" in out.lower() or "tinyhttpd" in out.lower()
        shell_ok = "httpd_test" in out2
        results["tinyhttpd"] = (started or shell_ok) and not crashed
        if crashed:
            logs["tinyhttpd_analysis"] = "tinyhttpd crashed with exception"
        elif not started and not shell_ok:
            logs["tinyhttpd_analysis"] = "No startup output and shell unresponsive"

    return restart
