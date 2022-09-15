host=$(hostname)
declare -A outfiles
outfiles[gcc]=results/${host}-gcc.json
outfiles[clang]=results/${host}-clang.json
declare -A logfiles
logfiles[gcc]=results/${host}-gcc.log
logfiles[clang]=results/${host}-clang.log
