#!/usr/bin/env python3
"""Check for things that should probably change before benchmarking.

This script checks for things that should "probably" change before
benchmarking code and prints commands you can execute to rememdy them.

The "probably" is quoted because not all performance testing should aim to
eliminate sources of variance, as this isn't the way most code is run.
Larger performance suites that run over long periods of time, or that do a
lot of I/O, should probably *not* employ the suggestsions made by this
script.

Note that on my AMD Ryzen 9 5900X 12-Core processor I found it was quite
important to disable "boost" mode, as each core will boost to a different
maximum clock rate, so timings were not stable run to run.  Alternatively,
I found that pinning the benchmark to a fixed CPU was a different way to
avoid the same source of variance (easily done using the "taskset" command
in the "util-linux" package maintained with the Linux kernel).

I found that switching to the "performance" CPU scaling governor doesn't
really have much of an impact for CPU intensive benchmarks, especially if
the benchmark system used "warms up" before it starts timing.

Disabling hyperthreading seemd to, anecdotally, reduce the frequency of
benchmark runs that were atypically slow, presumably by preventing one core
from running the benchmark and some other workload concurrently.

I have not yet explored using https://github.com/lpechacek/cpuset to
allocate specific sets of CPUs and/or NUMA memory to benchmarks.

Credit and inspiration to https://llvm.org/docs/Benchmarking.html.
"""
import os
from typing import List, Set

SYSFS_CPU = "/sys/devices/system/cpu"
INTEL_NO_TURBO_PATH = "intel_pstate/no_turbo"
AMD_BOOST_PATH = "cpufreq/boost"


def SysfsCpuPath(cpu: int, rest: str = None) -> str:
    path = os.path.join(SYSFS_CPU, f"cpu{cpu}")
    if rest:
        path = os.path.join(path, rest)
    return path


# ParseCpusetLine parses files of the format:
# (<int>-<int>|int)(,(<int>-<int>|int))+
def ParseCpusetLine(line: str) -> List[int]:
    cpus = []
    ranges = line.strip().split(",")
    for r in ranges:
        if "-" in r:
            first, last = r.split("-")
            for i in range(int(first), int(last) + 1):
                cpus.append(i)
        else:
            cpus.append(int(r))
    return cpus


def ParseBool(line: str) -> bool:
    return int(line.strip()) != 0


def Readline(path: str) -> str:
    with open(path) as f:
        return f.readline().strip()


def CpuIsOnline(cpu: int) -> bool:
    if cpu == 0:
        return True
    return ParseBool(Readline(SysfsCpuPath(cpu, "online")))


printed_messages = set()


def ShouldBe(value, path, message):
    try:
        if Readline(path) != value:
            if message not in printed_messages:
                printed_messages.add(message)
                print(f"# {message}")
            print(f"echo {value} | sudo tee {path}")
    except FileNotFoundError:
        pass


# Find every CPU present on the system.
cpus = ParseCpusetLine(Readline(os.path.join(SYSFS_CPU, "present")))

# Check that "hyperthreading" is disabled.
all_siblings: Set[int] = set()
for cpu in cpus:
    if CpuIsOnline(cpu):
        path = SysfsCpuPath(cpu, "topology/thread_siblings_list")
        siblings = ParseCpusetLine(Readline(path))
        all_siblings.update(siblings[1:])
for sibling in all_siblings:
    path = os.path.join(SysfsCpuPath(sibling), "online")
    ShouldBe("0", path, "Disable thread siblings (aka Hyperthreading)")

# Check that turboboost is disabled.  See
# https://easyperf.net/blog/2019/08/02/Perf-measurement-environment-on-Linux
# and https://www.kernel.org/doc/Documentation/cpu-freq/boost.txt
ShouldBe("1", os.path.join(SYSFS_CPU, INTEL_NO_TURBO_PATH), "Disable Intel Turbo Boost")
ShouldBe("0", os.path.join(SYSFS_CPU, AMD_BOOST_PATH), "Disable AMD Boost")

# Check that the scaling governor is "performance" on all CPUs.
for cpu in cpus:
    if CpuIsOnline(cpu) and cpu not in all_siblings:
        ShouldBe(
            "performance",
            SysfsCpuPath(cpu, "cpufreq/scaling_governor"),
            "Use the performance governor",
        )
