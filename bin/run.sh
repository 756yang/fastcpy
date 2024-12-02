#!/bin/bash


scp_dir=$(readlink -f "$0")
scp_dir=$(dirname "$scp_dir")

cd "$scp_dir"

./fastcpy_bench.exe --benchmark_repetitions=5 --benchmark_report_aggregates_only=true --benchmark_out_format=csv --benchmark_out=memcpy_bench.csv
