package main.totolotek.gracz;

import main.totolotek.system.Blankiet;
import main.totolotek.system.CentralaTotolotka;
import main.totolotek.system.GeneratorKuponów;
import main.totolotek.system.Kupon;
import main.totolotek.wyjątki.NieprawidłowyGracz;

public class GraczStałoblankietowy extends GraczStały {
    private final int coIle;
    public GraczStałoblankietowy(String imię, String nazwisko, String pesel, long środki,  int[] ulubioneKolektury,
                                 Blankiet b, int coIle) throws NieprawidłowyGracz {
        super(imię,nazwisko,pesel,środki,ulubioneKolektury);
        this.ulubionyGenerator = b;
        if(coIle <= 0){
            throw new NieprawidłowyGracz("Błąd przy tworzeniu stałoblankietowego: " +
                    "źle podana liczba losowań, co którą SB kupuje kupon");
        }
        this.coIle = coIle;
    }

    @Override
    public boolean czyMożna(GeneratorKuponów gk){
        int ostatni = 0;
        for(Kupon k : zakupioneKupony.keySet()){
            ostatni = Math.max(k.pierwszeLosowanie(),ostatni);
        }
        if(CentralaTotolotka.getInstance().numerKolejnegoLosowanie() - ostatni != coIle){
            return false;
        }
        return czyStarczy(gk.cenaWygenerowaniaKuponu());
    }
}
