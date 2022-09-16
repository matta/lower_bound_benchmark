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


def PrintCrappyHistogram(a, bins=50, width=80):
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


def BaseFilename(results_dir, benchmark, attempt):
    return os.path.join(results_dir, benchmark.name.replace("/", ".")) + f".{attempt}"


def RunOne(results_dir, benchmark: Benchmark) -> None:
    print(f"Running {benchmark.name} and saving to {results_dir}")

    all_results: List[Dict[str, Union[str, int, float, bool]]] = []
    repetitions = 20
    for attempt in itertools.count(0):
        args = benchmark.args + [
            f"--benchmark_out={benchmark_out}",
            f"--benchmark_repetitions={repetitions}",
        ]
        # print(shlex.join(args))
        repetitions = round(repetitions * 1.2)
        if repetitions > 100:
            repetitions = 100
        console = subprocess.check_output(
            args, stderr=subprocess.STDOUT, encoding="utf-8"
        )

        base_out = BaseFilename(results_dir, benchmark, attempt)
        with open(base_out + ".out", "w", encoding="utf-8") as f:
            f.write(console)

        json_text: str
        with open(benchmark_out, "r", encoding="utf-8") as f:
            json_text = f.read()

        with open(base_out + ".json", "w", encoding="utf-8") as f:
            f.write(json_text)

        results = json.loads(json_text)
        benchmarks = results["benchmarks"]
        all_results.extend([b for b in benchmarks if b.get("run_type") == "iteration"])

        elapsed: List[float] = []
        for b in all_results:
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
        if cv <= 0.01:
            break
        if interval_pct <= 0.01:
            break
        PrintCrappyHistogram(elapsed)


def Run(results_dir, benchmark_args) -> None:
    benchmarks = Benchmarks(benchmark_args)
    for benchmark in benchmarks:
        RunOne(results_dir, benchmark)


Run(results_dir, benchmark_args)
