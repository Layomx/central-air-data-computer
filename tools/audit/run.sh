#!/usr/bin/env bash
# Reproduce the Phase 1 audit (docs/audit-phase1.md) against the IMMUTABLE tag.
# Usage: tools/audit/run.sh            (from anywhere inside the repository)
# Env:   AUDIT_TAG (default v1.0.0-phase1), AUDIT_OUT (default /tmp/cadc-audit), CC (default gcc)
# Exit:  0 if every check is REPRODUCED, 1 if any is NOT-REPRODUCED, 2 on setup error.
# Nothing in the repository is modified; everything runs on a copy under $AUDIT_OUT.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(git -C "$HERE" rev-parse --show-toplevel) || exit 2
TAG=${AUDIT_TAG:-v1.0.0-phase1}
OUT=${AUDIT_OUT:-/tmp/cadc-audit}
CC=${CC:-gcc}

git -C "$REPO" rev-parse -q --verify "$TAG^{commit}" >/dev/null || { echo "tag $TAG not found; run: git fetch --tags" >&2; exit 2; }
COMMIT=$(git -C "$REPO" rev-parse "$TAG^{commit}")
rm -rf "$OUT"; mkdir -p "$OUT/bin" "$OUT/tree" || exit 2
git -C "$REPO" archive "$TAG" | tar -x -C "$OUT/tree" || exit 2
TREE=$OUT/tree
RES=$OUT/results.txt; : > "$RES"

{ echo "audited tag:   $TAG ($COMMIT)"
  echo "date (UTC):    $(date -u +%FT%TZ)"
  echo "compiler:      $($CC --version | head -1)"
  echo "make:          $(make --version | head -1)"
  echo "libc:          $(ldd --version 2>&1 | head -1)"
  echo "machine:       $(uname -srm)"; } | tee "$OUT/environment.txt"
echo

chk() { # chk ID EXIT_STATUS(0=reproduced) detail...
    local id=$1 ok=$2; shift 2
    local s="REPRODUCED"; [ "$ok" -eq 0 ] || s="NOT-REPRODUCED"
    printf 'CHECK %-5s %-14s %s\n' "$id" "$s" "$*" | tee -a "$RES"
}
skip() { printf 'SKIP  %-5s %s\n' "$1" "$2" | tee -a "$RES"; }

# ---------------------------------------------------------------- C experiments
for e in atmosphere airdata cadc; do
    $CC -std=c11 -O1 -g -Wall -Wextra -I"$HERE" -I"$TREE/src" "$HERE/exp_$e.c" "$TREE"/src/*.c -lm -o "$OUT/bin/exp_$e" || exit 2
done
"$OUT/bin/exp_atmosphere" "$TREE/data/isa_reference.txt" | tee -a "$RES"
"$OUT/bin/exp_airdata" | tee -a "$RES"
timeout 300 "$OUT/bin/exp_cadc" | tee -a "$RES"

# ---------------------------------------------------------------- build product copy
cp -r "$TREE" "$OUT/build"
( cd "$OUT/build" && make -s all >"$OUT/build.log" 2>&1 )
W=$(grep -ci warning "$OUT/build.log")
[ "$W" -eq 0 ]; chk OK-05 $? "make all with project flags: $W compiler warnings"

# ---------------------------------------------------------------- F-02: test blocks on stdin
F=$OUT/stdin.fifo; mkfifo "$F"; exec 3<>"$F"
( cd "$OUT/build" && timeout 10 ./tests/test_atmosphere <&3 >/dev/null 2>&1 ); RC=$?
exec 3>&-; rm -f "$F"
[ "$RC" -eq 124 ]; chk F-02 $? "test_atmosphere with an open, empty stdin: exit $RC (124 = still blocked after 10 s; scanf at tests/test_atmosphere.c)"

# ---------------------------------------------------------------- F-01: tests cannot fail
tests_run() { # dir -> sets T_RC, T_TOK
    ( cd "$1" && rm -f tests/test_atmosphere tests/test_sensors tests/test_airdata tests/test_cadc tests/test_logger \
        && timeout 150 make -s tests </dev/null >"$1.out" 2>&1 ); T_RC=$?
    T_TOK=$(grep -ciE 'ERROR|ERRO |INCORRECTO|FALLO|BUG' "$1.out")
}
cp -r "$TREE" "$OUT/mut_base"; tests_run "$OUT/mut_base"; BASE_TOK=$T_TOK
[ "$T_RC" -eq 0 ]; chk F-01 $? "unmodified tree: make tests exit $T_RC, $BASE_TOK lines with a failure word (labels, not failures)"
mutant() { # name detect(yes|no) file sed-expr
    local name=$1 want=$2 file=$3 expr=$4 d="$OUT/mut_$1"
    cp -r "$TREE" "$d"; sed -i "$expr" "$d/$file"
    if cmp -s "$d/$file" "$TREE/$file"; then chk F-01 1 "$name: mutation NOT applied"; return; fi
    tests_run "$d"; local delta=$((T_TOK - BASE_TOK)) got=no; [ "$delta" -gt 0 ] && got=yes
    [ "$T_RC" -eq 0 ] && [ "$got" = "$want" ]; chk F-01 $? "$name: make tests exit $T_RC (never fails); new failure-word lines: $delta (documented visible: $want)"
}
mutant M1 no  src/airdata.c    's/5\.25587611/5.5/'
mutant M2 no  src/airdata.c    's/tas_ms \* sqrt(sigma)/tas_ms * sigma/'
mutant M3 yes src/cadc.c       's/50\.0, F14_WING_MAX_DEG/50.0, 60.0/'
mutant M4 yes src/airdata.c    's/if (Pt < Ps || Ps <= 0.0 || T <= 0.0)/if (0)/'
mutant M5 no  src/atmosphere.h 's/ISA_LAPSE_RATE 0\.0065/ISA_LAPSE_RATE 0.0070/'
mutant M6 no  src/atmosphere.h 's/AIR_GAMMA 1\.4/AIR_GAMMA 1.3/'

# ---------------------------------------------------------------- F-01: coverage
if command -v gcov >/dev/null 2>&1; then
    C=$OUT/cov; mkdir -p "$C/output"; cp -r "$TREE/src" "$TREE/tests" "$C/"; cd "$C" || exit 2
    CF="-std=c11 -O0 -g --coverage -Isrc"
    for f in atmosphere sensors airdata cadc logger; do $CC $CF -c src/$f.c -o $f.o; done
    for t in atmosphere sensors airdata cadc logger; do $CC $CF -c tests/test_$t.c -o t_$t.o 2>/dev/null; done
    ln_() { t=$1; shift; $CC --coverage "$@" t_$t.o -o run_$t -lm; }
    ln_ atmosphere atmosphere.o; ln_ sensors atmosphere.o sensors.o; ln_ airdata atmosphere.o sensors.o airdata.o
    ln_ cadc atmosphere.o sensors.o airdata.o cadc.o logger.o; ln_ logger atmosphere.o sensors.o airdata.o cadc.o logger.o
    for t in atmosphere sensors airdata cadc logger; do timeout 100 ./run_$t </dev/null >/dev/null 2>&1; done
    MINL=100; MAXT=0; DET=""
    for f in atmosphere sensors airdata cadc logger; do
        r=$(gcov -b -c $f.o 2>/dev/null | awk -v F="$f.c" '$0 ~ "^File .src/"F"." {p=1;next} p&&/Lines executed/{split($2,a,":");gsub("%","",a[2]);l=a[2];next} p&&/Taken at least once/{split($0,b,":");split(b[2],c,"%");print l, c[1]; exit}')
        L=${r% *}; T=${r#* }; DET="$DET $f.c lines=${L}% branches-taken=${T}%;"
        MINL=$(awk -v a="$MINL" -v b="$L" 'BEGIN{print (b<a)?b:a}'); MAXT=$(awk -v a="$MAXT" -v b="$T" 'BEGIN{print (b>a)?b:a}')
    done
    awk -v l="$MINL" -v t="$MAXT" 'BEGIN{exit !(l>=70 && t<=80)}'; chk F-01 $? "coverage of the 5 tests (lines high, branches low; no assertions behind it):$DET"
    cd "$HERE" || exit 2
else skip F-01 "gcov not installed: coverage not measured"; fi

# ---------------------------------------------------------------- OK: sanitizers, strict warnings, analyzer
S=$OUT/san; cp -r "$TREE" "$S"; cd "$S" || exit 2
SF="-std=c11 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Isrc"
REP=0
for t in atmosphere sensors airdata cadc logger; do
    case $t in atmosphere) s="src/atmosphere.c";; sensors) s="src/atmosphere.c src/sensors.c";; airdata) s="src/atmosphere.c src/sensors.c src/airdata.c";; *) s="src/atmosphere.c src/sensors.c src/airdata.c src/cadc.c src/logger.c";; esac
    $CC $SF $s tests/test_$t.c -o tests/san_$t -lm 2>/dev/null
    timeout 100 ./tests/san_$t </dev/null >"$OUT/san_$t.log" 2>&1; REP=$((REP + $(grep -cE 'runtime error|AddressSanitizer|LeakSanitizer' "$OUT/san_$t.log")))
done
$CC $SF src/*.c main.c -I. -o san_main -lm 2>/dev/null
for a in "" "--profile intercept --noise --dt 0.5" "--profile custom --alt 9000 --mach 0.85 --panel" "--profile custom --alt 25000 --mach 4 --noise" "--profile custom --alt -500 --mach -1" "--dt 0.001 --quiet --no-csv"; do
    timeout 150 ./san_main $a --output "$OUT/san.csv" </dev/null >"$OUT/san_main.log" 2>&1; REP=$((REP + $(grep -cE 'runtime error|AddressSanitizer|LeakSanitizer' "$OUT/san_main.log")))
done
[ "$REP" -eq 0 ]; chk OK-06 $? "AddressSanitizer+UBSan on 5 tests and 6 CLI runs: $REP reports (absence of reports is not proof of absence of defects)"
$CC -std=c11 -fsyntax-only -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wdouble-promotion -Wformat=2 -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wfloat-equal -Wundef -Wnull-dereference -I. src/*.c main.c 2>"$OUT/strict.log"
N=$(grep -c warning "$OUT/strict.log"); [ "$N" -eq 0 ]; chk OK-07 $? "strict warning set on src/ and main.c: $N warnings"
N=0; for f in src/*.c main.c; do N=$((N + $($CC -std=c11 -fanalyzer -O0 -c -I. "$f" -o /dev/null 2>&1 | grep -c warning))); done
[ "$N" -eq 0 ]; chk OK-07 $? "-fanalyzer on src/ and main.c: $N warnings"

# ---------------------------------------------------------------- CLI: F-05, F-12, R-3
cd "$OUT/build" || exit 2
cli() { timeout 60 ./cadc_f14 "$@" --no-csv --quiet </dev/null 2>&1; echo "rc=$?"; }
o=$(cli --profile custom --alt abc --mach xyz); echo "$o" | grep -q "Ciclos totales: 61" && echo "$o" | grep -q "rc=0"; chk F-12 $? "--alt abc --mach xyz accepted silently (parsed as 0, 61 cycles, exit 0)"
o=$(cli --dt nan); echo "$o" | grep -q "Ciclos totales: 0" && echo "$o" | grep -q "rc=0"; chk F-12 $? "--dt nan accepted: 0 cycles, exit 0"
a=$(./cadc_f14 --profile intercept --noise --seed abc --quiet --output "$OUT/s1.csv" </dev/null 2>&1 >/dev/null; cat "$OUT/s1.csv" | sha256sum | cut -c1-16)
sleep 2; b=$(./cadc_f14 --profile intercept --noise --seed abc --quiet --output "$OUT/s2.csv" </dev/null 2>&1 >/dev/null; cat "$OUT/s2.csv" | sha256sum | cut -c1-16)
[ "$a" != "$b" ]; chk F-12 $? "--seed abc (atoi -> 0 -> time(NULL)): two runs 2 s apart give different CSVs ($a vs $b)"
o=$(cli --profile custom --alt 9000 --mach nan); echo "$o" | grep -q "supersonicos: 61 (100.0%)" && echo "$o" | grep -q "invalidos: 0 "; chk F-05 $? "--mach nan: 100% supersonic cycles and 0 invalid cycles"
./cadc_f14 --profile intercept --noise --seed 42 --quiet --output "$OUT/r1.csv" </dev/null >/dev/null 2>&1; ./cadc_f14 --profile intercept --noise --seed 42 --quiet --output "$OUT/r2.csv" </dev/null >/dev/null 2>&1
cmp -s "$OUT/r1.csv" "$OUT/r2.csv"; chk OK-08 $? "same seed, two runs on this machine/toolchain: byte-identical CSV (cross-platform behaviour NOT verified)"

# ---------------------------------------------------------------- F-13: repository hygiene
cd "$OUT" || exit 2
H=$OUT/hyg; mkdir -p "$H/tests"; ( cd "$H" && git init -q . && cp "$TREE/.gitignore" . && touch tests/test_atmosphere tests/test_sensors tests/test_airdata tests/test_cadc tests/test_logger )
NI=0; for t in atmosphere sensors airdata cadc logger; do ( cd "$H" && git check-ignore -q tests/test_$t ) || NI=$((NI + 1)); done
[ "$NI" -eq 5 ]; chk F-13 $? ".gitignore covers $((5 - NI)) of 5 test binaries"
CR=0; for f in "$TREE"/src/*.[ch] "$TREE"/tests/*.c; do grep -q $'\r' "$f" && CR=$((CR + 1)); done
LFONLY=0; for f in "$TREE/main.c" "$TREE/Makefile" "$TREE/.gitignore"; do grep -q $'\r' "$f" || LFONLY=$((LFONLY + 1)); done
[ "$CR" -eq 15 ] && [ "$LFONLY" -eq 3 ] && [ ! -e "$TREE/.gitattributes" ]; chk F-13 $? "line endings: $CR of 15 files in src/ and tests/ use CRLF; main.c, Makefile, .gitignore LF-only: $LFONLY of 3; no .gitattributes"
UN=0; for c in AIRDATA_MACH_MIN F14_CAS_LAND_KT F14_WING_CARRIER_DEG F14_ALT_MAX_M LOGGER_BUFFER_SIZE; do [ "$(cat "$TREE"/src/*.[ch] "$TREE"/main.c "$TREE"/tests/*.c | grep -c "$c")" -eq 1 ] && UN=$((UN + 1)); done
[ "$UN" -eq 5 ]; chk F-13 $? "constants referenced only by their definition: $UN of 5"
W1=$(cat "$TREE"/src/*.c "$TREE"/main.c | grep -n 'update_rate_hz' | grep -vc 'update_rate_hz *=')
W2=$(cat "$TREE"/src/*.c "$TREE"/main.c | grep -n '\.seed' | grep -vc '\.seed *=')
[ "$W1" -eq 0 ] && [ "$W2" -eq 0 ]; chk F-12 $? "update_rate_hz and NoiseConfig.seed are written but never read (reads found: $W1, $W2)"

# ---------------------------------------------------------------- summary
echo
NR=$(grep -c 'NOT-REPRODUCED' "$RES"); OKN=$(grep -c ' REPRODUCED ' "$RES"); SK=$(grep -c '^SKIP' "$RES")
echo "SUMMARY: $OKN reproduced, $NR not reproduced, $SK skipped. Details: $RES, $OUT/environment.txt"
echo "Not executable here (manual review items in docs/audit-phase1.md): F-11 boundary blur (true_* fields in SensorReading), F-10 monitor placement, R-3 cross-platform reproducibility."
[ "$NR" -eq 0 ] && exit 0 || exit 1
