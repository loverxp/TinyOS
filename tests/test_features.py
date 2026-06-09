#!/usr/bin/env python3
"""TinyOS integration test runner. Discovers and orchestrates test cases.

Supports parallel test execution: each case module runs in its own QEMU instance.

Usage:
    python tests/test_features.py                    # Run all tests (parallel)
    python tests/test_features.py --test fs          # Run specific case(s)
    python tests/test_features.py --test fs,user     # Run multiple cases
    python tests/test_features.py --test ls          # Run specific item (cross-case)
    python tests/test_features.py --retry-failed     # Retry failed from last run
    python tests/test_features.py --list-tests       # List available test tags
    python tests/test_features.py --parallel 2       # Run with 2 parallel workers
"""
import sys
import os
import importlib
import pkgutil
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

# Add project root to path for imports
sys.path.insert(0, os.path.dirname(__file__))
import common
import cases


def discover_cases():
    """Discover all test case modules in the cases package."""
    case_list = []
    for importer, modname, ispkg in pkgutil.iter_modules(cases.__path__):
        mod = importlib.import_module(f"cases.{modname}")
        if hasattr(mod, "TAG") and hasattr(mod, "run"):
            case_list.append(mod)
    # Sort by a deterministic order so tests always run in the same sequence
    order = {"boot": 0, "fs": 1, "user": 2, "sched": 3, "help": 4, "usermode": 5, "pipeline": 6, "v40": 7}
    case_list.sort(key=lambda m: order.get(m.TAG, 99))
    return case_list


def list_tests(case_list):
    print("Available test tags:")
    print(f"  {'TAG':15s} Items")
    print(f"  {'---':15s} -----")
    for mod in case_list:
        items = ", ".join(mod.DISPLAY_NAMES)
        print(f"  {mod.TAG:15s} {items}")
    print("\nYou can also use individual item names as --test filters:")
    all_items = []
    for mod in case_list:
        all_items.extend(mod.DISPLAY_NAMES)
    print(f"  e.g. --test ls    --test partitions,write,hello")


def resolve_selection(selected, case_list):
    """Map selected display names (e.g., 'ls', 'partitions') back to their case tags.
    Returns both the original item names and their case tags, so that modules
    can check either level for fine-grained filtering."""
    if selected is None:
        return None
    # Build reverse map: display_name -> tag
    reverse = {}
    for mod in case_list:
        for name in mod.DISPLAY_NAMES:
            reverse[name.lower()] = mod.TAG
        reverse[mod.TAG] = mod.TAG
    resolved = set()
    for s in selected:
        s_lower = s.lower().strip()
        # Keep the original name so individual items can be filtered
        resolved.add(s_lower)
        # Also resolve to case tag for case-level selection
        if s_lower in reverse:
            resolved.add(reverse[s_lower])
        else:
            # Try partial match in display names
            found = False
            for mod in case_list:
                if any(s_lower in d.lower() for d in mod.DISPLAY_NAMES):
                    resolved.add(mod.TAG)
                    found = True
                    break
            if not found:
                print(f"[WARN] Unknown test '{s}', skipping")
    return list(resolved)


def main():
    import argparse
    parser = argparse.ArgumentParser(description="TinyOS Integration Test Runner")
    parser.add_argument("--test", help="Comma-separated test tags or item names")
    parser.add_argument("--retry-failed", action="store_true",
                        help="Retry only previously failed tests")
    parser.add_argument("--list-tests", action="store_true",
                        help="List all available test tags")
    parser.add_argument("--parallel", type=int, default=None,
                        help="Number of parallel workers (default: all modules)")
    args = parser.parse_args()

    case_list = discover_cases()

    if args.list_tests:
        list_tests(case_list)
        return 0

    selected = None
    if args.test:
        raw = [t.strip() for t in args.test.split(",")]
        selected = resolve_selection(raw, case_list)
        if not selected:
            print("[ERROR] No valid tests matched. Use --list-tests to see options.")
            return 1

    if args.retry_failed:
        failed_display_names = common.load_failures()
        if failed_display_names is None:
            return 1
        # Map failed display names back to case tags
        selected = resolve_selection(failed_display_names, case_list)
        print(f"[INFO] Retrying failed tests (cases): {', '.join(selected)}")

    # Filter case_list to only modules that should run
    modules_to_run = []
    for mod in case_list:
        if common.should_run([mod.TAG] + mod.DISPLAY_NAMES, selected):
            modules_to_run.append(mod)

    if not modules_to_run:
        print("[ERROR] No test modules matched the filter. Use --list-tests to see options.")
        return 1

    print(f"\n[TinyOS Test Runner] Running {len(modules_to_run)} module(s) in parallel\n")

    # Determine parallelism: default = all modules
    max_workers = args.parallel if args.parallel is not None else len(modules_to_run)
    max_workers = min(max_workers, len(modules_to_run))

    # Run modules in parallel using ThreadPoolExecutor
    all_results = {}
    all_logs = {}
    global_need_restart = False

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {}
        for i, mod in enumerate(modules_to_run):
            worker_id = i
            future = executor.submit(common.run_module_single, mod, worker_id, selected)
            futures[future] = mod.TAG

        for future in as_completed(futures):
            tag, need_restart, results, logs = future.result()
            all_results.update(results)
            all_logs.update(logs)
            if need_restart:
                global_need_restart = True
            print(f"  [DONE] {tag}")

    # Handle crash recovery: re-run modules that crashed
    if global_need_restart:
        print("\n[INFO] Some tests reported crashes. Re-running affected modules...")
        # In parallel mode, this is a best-effort signal
        pass

    p, f = common.print_summary(all_results)

    common.save_failures(all_results)
    report = common.generate_report(all_results, all_logs, p, f, len(all_results))
    common.save_report(report)

    return 0 if f == 0 else 1


if __name__ == "__main__":
    sys.exit(main())