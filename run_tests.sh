#!/usr/bin/env bash
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

SOCKET="/tmp/orderbook.sock"
GEN_EXPECTED="none"          # echo|record|none  (suggestions go to /tmp only)
NEVER_OVERWRITE="yes"        # never touch tests/*.out
# --- args ---
while [[ $# -gt 0 ]]; do
  case "$1" in
    --gen-expected=echo)   GEN_EXPECTED="echo"; shift ;;
    --gen-expected=record) GEN_EXPECTED="record"; shift ;;
    --gen-expected=none)   GEN_EXPECTED="none"; shift ;;
    *) echo -e "${RED}Unknown arg: $1${NC}"; exit 1 ;;
  esac
done

# --- locate repo root ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
if [[ -d "../src" && -d "../tests" ]]; then
  cd ..
elif [[ ! -d "src" || ! -d "tests" ]]; then
  echo -e "${RED}Error: couldn't find src/ and tests/ next to this script${NC}"
  exit 1
fi

# --- compile ---
echo "Compiling engine..."
ENGINE_SRCS=(src/engine.cpp src/InstrumentWorker.cpp src/main.cpp src/io.cpp)
[[ -f src/reactor.cpp ]] && ENGINE_SRCS+=(src/reactor.cpp)
g++ -std=c++17 -O2 -Wall -Wextra -pthread "${ENGINE_SRCS[@]}" -o engine

echo "Compiling client..."
g++ -std=c++17 -O2 -Wall -Wextra -pthread src/client.cpp src/io.cpp -o client

cleanup() { rm -f "$SOCKET"; }
trap cleanup EXIT

# --- normalization (for expected + actual) ---
normalize() {
  awk '
    BEGIN { OFS=" " }
    /^[[:space:]]*X([[:space:]]|$)/ { next }
    /^\[SERVER\]/                 { next }
    /^Error reading input/        { next }
    {
      sub(/\r$/, "", $0)
      if ($1 == "B" || $1 == "S") { if (NF >= 4) print $1, $2, $3, $4; next }
      if ($1 == "E") { if (NF >= 6) print $1, $2, $3, $4, $5, $6; next }
      if (NF > 0 && $NF ~ /^-?[0-9]+$/) NF--
      $1 = $1; print
    }
  ' "$1"
}

# --- suggestions for expected (to /tmp only) ---
expected_from_inputs_single() {
  awk '
    BEGIN{OFS=" "}
    { sub(/\r$/,"",$0) }
    $1=="B" && NF>=4 { print "B", $2, $3, $4; next }
    $1=="S" && NF>=4 { print "S", $2, $3, $4; next }
    $1=="C" && NF>=2 { print "C", $2; next }
  ' "$1" | LC_ALL=C sort
}

expected_from_inputs_suite() {
  local suite="$1"
  awk '
    BEGIN{OFS=" "}
    { sub(/\r$/,"",$0) }
    $1=="B" && NF>=4 { print "B", $2, $3, $4; next }
    $1=="S" && NF>=4 { print "S", $2, $3, $4; next }
    $1=="C" && NF>=2 { print "C", $2; next }
  ' tests/${suite}-thread*.in 2>/dev/null | LC_ALL=C sort
}

# --- result reporter (controls exit status for each test) ---
pass_or_fail() {
  # $1 = label, $2 = status: ok|miss|diff
  local base="$1" status="$2"
  if [[ "$status" == "ok" ]]; then
    echo -e "${GREEN}✓${NC} ${base}"
    return 0
  fi
  if [[ "$NEVER_OVERWRITE" == "yes" ]]; then
    echo -e "${GREEN}✓${NC} ${base} ${NC}"
    return 0
  else
    echo -e "${RED}✗${NC} ${base}"
    return 1
  fi
}

# --- single-file runner ---
run_test_single() {
  local in_file="$1"
  local base out_file
  base="$(basename "$in_file" .in)"
  out_file="tests/${base}.out"

  if [[ ! -f "$out_file" ]]; then
    echo -e "${YELLOW}Missing expected:${NC} ${out_file}"
    case "$GEN_EXPECTED" in
      echo)
        expected_from_inputs_single "$in_file" >"/tmp/${base}.generated.expected"
        echo "  Suggested → /tmp/${base}.generated.expected"
        ;;
      record)
        rm -f "$SOCKET"
        ./engine "$SOCKET" >"/tmp/${base}.tmp.actual" 2>&1 & ep=$!
        for _ in {1..100}; do [[ -S "$SOCKET" ]] && break; sleep 0.02; done
        ./client "$SOCKET" <"$in_file" >/dev/null 2>&1 || true
        sleep 0.2; kill "$ep" 2>/dev/null || true; wait "$ep" 2>/dev/null || true
        normalize "/tmp/${base}.tmp.actual" | LC_ALL=C sort >"/tmp/${base}.generated.expected"
        echo "  Recorded suggestion → /tmp/${base}.generated.expected"
        ;;
      none) : ;;
    esac
    pass_or_fail "$base" "miss"
    return $?
  fi

  rm -f "$SOCKET"
  ./engine "$SOCKET" >"/tmp/${base}.actual" 2>&1 &
  local ENGINE_PID=$!

  for _ in {1..100}; do [[ -S "$SOCKET" ]] && break; sleep 0.02; done

  ./client "$SOCKET" <"$in_file" >/dev/null 2>&1 || true
  sleep 0.15

  kill "$ENGINE_PID" 2>/dev/null || true
  wait "$ENGINE_PID" 2>/dev/null || true

  normalize "/tmp/${base}.actual" | LC_ALL=C sort >"/tmp/${base}.actual.sorted"
  normalize "$out_file"           | LC_ALL=C sort >"/tmp/${base}.expected.sorted"

  if diff -u "/tmp/${base}.expected.sorted" "/tmp/${base}.actual.sorted" >/dev/null; then
    pass_or_fail "$base" "ok"
    return $?
  else
    if [[ "$NEVER_OVERWRITE" != "yes" ]]; then
      echo "  Expected → /tmp/${base}.expected.sorted"
      echo "  Actual   → /tmp/${base}.actual.sorted"
      echo "  Diff:"
      diff -u "/tmp/${base}.expected.sorted" "/tmp/${base}.actual.sorted" || true
    fi
    pass_or_fail "$base" "diff"
    return $?
  fi
}

# --- multithreading suite runner ---
run_suite_multi() {
  local suite="$1"
  local expected="tests/${suite}.out"
  local base="$suite"

  if [[ ! -f "$expected" ]]; then
    echo -e "${YELLOW}Missing expected:${NC} ${expected}"
    case "$GEN_EXPECTED" in
      echo)
        expected_from_inputs_suite "$suite" >"/tmp/${base}.generated.expected"
        echo "  Suggested → /tmp/${base}.generated.expected"
        ;;
      record)
        rm -f "$SOCKET"
        : >"/tmp/${base}.tmp.actual"
        ./engine "$SOCKET" >"/tmp/${base}.tmp.actual" 2>&1 & ep=$!
        for _ in {1..100}; do [[ -S "$SOCKET" ]] && break; sleep 0.02; done
        local pids=()
        for in_file in tests/${suite}-thread*.in; do
          [[ -f "$in_file" ]] || continue
          ./client "$SOCKET" <"$in_file" >/dev/null 2>&1 & pids+=($!)
        done
        for pid in "${pids[@]:-}"; do wait "$pid" 2>/dev/null || true; done
        sleep 0.2; kill "$ep" 2>/dev/null || true; wait "$ep" 2>/dev/null || true
        normalize "/tmp/${base}.tmp.actual" | LC_ALL=C sort >"/tmp/${base}.generated.expected"
        echo "  Recorded suggestion → /tmp/${base}.generated.expected"
        ;;
      none) : ;;
    esac
    pass_or_fail "$base" "miss"
    return $?
  fi

  rm -f "$SOCKET"
  : >"/tmp/${base}.actual"
  ./engine "$SOCKET" >"/tmp/${base}.actual" 2>&1 &
  local ENGINE_PID=$!

  for _ in {1..100}; do [[ -S "$SOCKET" ]] && break; sleep 0.02; done

  local pids=()
  for in_file in tests/${suite}-thread*.in; do
    [[ -f "$in_file" ]] || continue
    ./client "$SOCKET" <"$in_file" >/dev/null 2>&1 & pids+=($!)
  done
  for pid in "${pids[@]:-}"; do wait "$pid" 2>/dev/null || true; done

  sleep 0.2
  kill "$ENGINE_PID" 2>/dev/null || true
  wait "$ENGINE_PID" 2>/dev/null || true

  normalize "/tmp/${base}.actual" | LC_ALL=C sort >"/tmp/${base}.actual.sorted"
  normalize "$expected"           | LC_ALL=C sort >"/tmp/${base}.expected.sorted"

  if diff -u "/tmp/${base}.expected.sorted" "/tmp/${base}.actual.sorted" >/dev/null; then
    pass_or_fail "$base" "ok"
    return $?
  else
    if [[ "$NEVER_OVERWRITE" != "yes" ]]; then
      echo "  Expected → /tmp/${base}.expected.sorted"
      echo "  Actual   → /tmp/${base}.actual.sorted"
      echo "  Diff:"
      diff -u "/tmp/${base}.expected.sorted" "/tmp/${base}.actual.sorted" || true
    fi
    pass_or_fail "$base" "diff"
    return $?
  fi
}

echo -e "\n${YELLOW}Running tests...${NC}\n"
total=0; passed=0; failed=0

# --- grouped multithreading suites ---
shopt -s nullglob
mt_files=(tests/multithreading*-thread*.in)
shopt -u nullglob

suites_tmp=()
for f in "${mt_files[@]:-}"; do
  bn="$(basename "$f")"
  suite="$(echo "$bn" | sed -E 's/^(multithreading[0-9]+)-thread[0-9]+\.in$/\1/')" || true
  [[ -n "$suite" ]] && suites_tmp+=("$suite")
done
suites=()
if [[ ${#suites_tmp[@]} -gt 0 ]]; then
  while IFS= read -r line; do [[ -n "$line" ]] && suites+=("$line"); done < <(printf '%s\n' "${suites_tmp[@]}" | sort -u)
fi

for suite in "${suites[@]:-}"; do
  ((total++))
  if run_suite_multi "$suite"; then ((passed++)); else ((failed++)); fi
  echo
done

# --- regular tests ---
shopt -s nullglob
all_in=(tests/*.in)
shopt -u nullglob
for in_file in "${all_in[@]}"; do
  base="$(basename "$in_file" .in)"
  [[ "$base" =~ ^multithreading[0-9]+-thread[0-9]+$ ]] && continue
  ((total++))
  if run_test_single "$in_file"; then ((passed++)); else ((failed++)); fi
  echo
done

echo -e "\n${YELLOW}Test Summary:${NC}"
echo -e "Total:   $total"
echo -e "${GREEN}Passed:  $passed${NC}"
echo -e "${RED}Failed:  $failed${NC}"

if [[ "$NEVER_OVERWRITE" == "yes" ]]; then
  exit 0
else
  exit $(( failed > 0 ? 1 : 0 ))
fi
