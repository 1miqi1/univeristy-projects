#!/bin/bash
# Autor: Olaf Targowski
# Przeznaczenie: sprawdzenie poprawnoĹci zadania 3 z AKSO (freverse)
# Ta wersja nie sprawdza odpornoĹci kodu na bĹÄdy.
set -euo pipefail
[ "$#" = "1" ] || { echo "SposĂłb uĹźycia: test.sh ./freverse"; exit 123; }
program="$1"

if [ -f bajty ] && [[ $(wc -c < bajty) -gt 4294967296 ]] \
    && [[ $(wc -c < bajtyrev) -gt 4294967296 ]]; then
    echo "UĹźywam wczeĹniejÂ wygenerowanych losowych bajtĂłw z plikĂłw 'bajty' i 'bajtyrev'."
else
    echo "GenerujÄ losowe bajty..."
    echo "sus mogus" > bajty
    dd if=/dev/urandom of=bajty oflag=append conv=notrunc bs=1M count=4096 status=progress
    echo "123567890" >> bajty
    echo "Wygenerowano."
    echo "Odwracam rzeczone bajty..."
    perl -e '
        binmode(STDIN);
        binmode(STDOUT);
        my $data;
        {
            local $/;
            $data = <STDIN>;
        }
        print STDOUT scalar reverse $data;
    ' < bajty > bajtyrev
    echo "OdwrĂłcono."
fi

rozmiar_calosci=$(wc -c < bajty)

# 2 argumenty:
# $1 - rozmiar pliku w bajtach.
# $2 - "jo" wtw ma wiÄcej gadaÄ.
testuj(){
    head -c $1 bajty > test
    echo -ne "TestujÄ na $1 bajtach:\r"
    if [ "$2" = "jo" ]; then
        echo
        time "$program" test || { echo -e "\nBĹÄD WYKONANIA!!1!11!"; exit 37; }
    else
        "$program" test || { echo -e "\nBĹÄD WYKONANIA!!1!11!"; exit 38; }
    fi
    # CZEMU to Ĺcierwo exituje z 1 jak wynik to zero??????????
    prefiks=$(expr $rozmiar_calosci - $1 || true)
    if ! cmp -si 0:$prefiks test bajtyrev; then
        echo -e "\nZĹA ODPOWIEDĹš!!1!11!"
        cmp -li 0:$prefiks test -
        exit 69;
    fi
    rm -f test
}

for rozmiar in $(seq 0 1024); do
    testuj $rozmiar niejo
done

for wykladnik in $(seq 12 2 32); do
    rozmiar=1
    for i in $(seq 1 $wykladnik); do
        rozmiar=$(expr $rozmiar '*' 2)
    done
    jo="niejo"
    [[ $wykladnik -gt 27 ]] && jo="jo"
    testuj $rozmiar $jo
    rozmiar=$(expr $rozmiar + $(printf "%llu" 0x$(openssl rand -hex 1)))
    # Za chwilÄ na peĹnym pliku ztestujemy jak wykl=32.
    if [[ $wykladnik -lt 32 ]]; then
        testuj $rozmiar $jo
    fi
done

testuj $rozmiar_calosci jo