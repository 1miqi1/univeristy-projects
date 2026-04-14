package main.totolotek.gracz;

import java.util.*;

import main.totolotek.system.Blankiet;
import main.totolotek.system.CentralaTotolotka;
import main.totolotek.system.GeneratorKuponów;
import main.totolotek.system.Kupon;
import main.totolotek.wyjątki.NieprawidłowyBlankiet;
import main.totolotek.wyjątki.NieprawidłowyGracz;

public class GraczStałoliczbowy extends GraczStały{

    public GraczStałoliczbowy(String imię, String nazwisko, String pesel, long środki, int[] ulubioneKolektury, int[] liczby)
            throws NieprawidłowyGracz{
        super(imię,nazwisko,pesel,środki,ulubioneKolektury);
        if (liczby == null || liczby.length != 6) {
            throw new NieprawidłowyGracz("Błąd przy tworzeniu stałoliczbowego: błędne podane liczby");
        }

        Set<Integer> unikalne = new HashSet<>();

        for (int liczba : liczby) {
            // Sprawdzenie zakresu
            if (liczba < 1 || liczba > 49) {
                throw new NieprawidłowyGracz("Błąd przy tworzeniu stałoliczbowego: błędne podane liczby");
            }
            // Sprawdzenie unikalności
            if (!unikalne.add(liczba)) {
                throw new NieprawidłowyGracz("Błąd przy tworzeniu stałoliczbowego: błędne podane liczby");
            }
        }
        try {
            ulubionyGenerator = new Blankiet(new int[][]{liczby,{},{},{},{},{},{},{}},
                    new boolean[] {false,true,true,true,true,true,true,true},
                    new int[]{10});
        }catch (NieprawidłowyBlankiet e){
            System.out.println("Błąd systemu przy tworzeniu gracza stałoliczbowego" + e.getMessage());
            System.exit(1);
        }
    }

    @Override
    public boolean czyMożna(GeneratorKuponów gk) {
        for(Kupon k : zakupioneKupony.keySet()){
            if(k.ostatnieLosowanie() >= CentralaTotolotka.getInstance().numerKolejnegoLosowanie()){
                return false;
            }
        }
        return czyStarczy(gk.cenaWygenerowaniaKuponu());
    }
}
