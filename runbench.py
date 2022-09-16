#!/usr/bin/env python3

import atexit
import itertools
import json
import numpy as np
import os
import pprint
import re
import scipy.stats as st
import shlex
import statistics
import subprocess
import sys
import tempfile

from typing import Any, Dict, List, Union, Type

HackyJSON = Union[Dict[str, Any], List[Any]]

results_dir = sys.argv[1]
benchmark_args = sys.argv[2:]


def TempName() -> str:
    handle, name = tempfile.mkstemp()
    os.close(handle)
    return name


benchmark_out = TempName()
atexit.register(os.remove, benchmark_out)


class Benchmark:
    def __init__(self, name: str, args: List[str]):
        self.name = name
        self.args = args


def Benchmarks(args: List[str]) -> List[Benchmark]:
    tests = subprocess.check_output(
        args + ["--benchmark_list_tests"], text=True
    ).splitlines()
    benchmarks: List[Benchmark] = []
    for test in tests:
        benchmark_filter = [f"--benchmark_filter=^{re.escape(test)}$"]
        benchmarks.append(Benchmark(test, args + benchmark_filter))
    return benchmarks


def PrintCrappyHistogram(a, bins=24, width=79):
    h, b = np.histogram(a, bins)

    for i in range(0, bins):
        print(
            "{:12.5f}  | {:{width}s} {}".format(
                b[i], "#" * int(width * h[i] / np.amax(h)), h[i], width=width
            )
        )
    print("{:12.5f}  |".format(b[bins]))


def MagicThing(a):
    return st.t.interval(0.95, len(a) - 1, loc=np.mean(a), scale=st.sem(a))


def ResultFilename(results_dir, benchmark, suffix):
    name = os.path.join(results_dir, benchmark.name.replace("/", "."))
    if suffix is not None:
        name = name + f".{suffix}"
    return name


def RunOne(results_dir, benchmark: Benchmark) -> None:
    print(f"Running {benchmark.name} and saving to {results_dir}")

    all_iterations: List[Dict[str, Union[str, int, float, bool]]] = []
    all_console = ""
    context = None
    repetitions = 10
    for attempt in itertools.count(0):
        args = benchmark.args + [
            f"--benchmark_out={benchmark_out}",
            f"--benchmark_repetitions={repetitions}",
        ]
        # print(shlex.join(args))
        console = subprocess.check_output(
            args, stderr=subprocess.STDOUT, encoding="utf-8"
        )
        all_console = console + all_console

        json_text: str
        with open(benchmark_out, "r", encoding="utf-8") as f:
            json_text = f.read()

        # with open(base_out + ".json", "w", encoding="utf-8") as f:
        #     f.write(json_text)

        results = json.loads(json_text)

        # Remember the first context dict so we can write that one out when
        # we're done.
        if context is None:
            context = results["context"]

        for b in results["benchmarks"]:
            if b.get("run_type") == "iteration":
                all_iterations.append(b)

        elapsed: List[float] = []
        for b in all_iterations:
            assert isinstance(b["real_time"], float)
            elapsed.append(b["real_time"])
        mean = statistics.mean(elapsed)
        median = statistics.median(elapsed)
        stdev = statistics.stdev(elapsed)
        cv = stdev / mean
        low95, high95 = MagicThing(elapsed)
        interval_pct = (high95 - low95) / mean
        print(
            f"  med {median:.4f} mean {mean:.4f} stdev {stdev:.4f} "
            f"cv {cv * 100:.3f}% conf {interval_pct*100:.2}% "
            f"samples {len(elapsed)}"
        )
        # if len(elapsed) > 100:
        #     PrintCrappyHistogram(elapsed)
        if len(elapsed) >= 100 and interval_pct <= 0.005:
            break

    name = ResultFilename(results_dir, benchmark, "json")
    result = {"context": context, "benchmarks": all_iterations}
    with open(name, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=4)

    name = ResultFilename(results_dir, benchmark, "console")
    with open(name, "w", encoding="utf-8") as f:
        f.write(all_console)


def Run(results_dir, benchmark_args) -> None:
    benchmarks = Benchmarks(benchmark_args)
    for benchmark in benchmarks:
        RunOne(results_dir, benchmark)


Run(results_dir, benchmark_args)
