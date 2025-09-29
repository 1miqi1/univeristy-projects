#!/bin/bash
# Autor: Olaf Targowski
# Przeznaczenie: pomiar czasu zadania 3 z AKSO (freverse)
set -euo pipefail
[ "$#" = "1" ] || { echo "SposĂłb uĹźycia: test_pomiar.sh ./freverse"; exit 123; }
program="$1"

if [ -f bajty ] && [[ $(wc -c < bajty) -gt 4294967296 ]]; then
    echo "UĹźywam wczeĹniejÂ wygenerowanych losowych bajtĂłw z pliku 'bajty'."
    echo "JeĹźeli testowany program jest bĹÄdny, a uĹźyto w tym samym folderze test.sh,"
    echo "to naleĹźy usunÄÄ pliki 'bajty' i 'bajtyrev'."
else
    echo "GenerujÄ losowe bajty..."
    echo "sus mogus" > bajty
    dd if=/dev/urandom of=bajty oflag=append conv=notrunc bs=1M count=4096 status=progress
    echo "123567890" >> bajty
    echo "Wygenerowano."
fi

wyr=""
ile=4
for i in $(seq 1 $ile); do
    /bin/time -o _res123 -f %e $program bajty
    real=$(cat _res123)
    wyr="$wyr $real + "
    echo $real
    rm -f _res123
done
wyr="scale=2;($wyr 0) / $ile"
sr=$(echo $wyr | bc)
echo "Ĺredni czas dla pliku o rozmiarze 4GB z hakiem: $sr"