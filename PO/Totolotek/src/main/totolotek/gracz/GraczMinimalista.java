package main.totolotek.gracz;

import main.totolotek.system.*;
import main.totolotek.wyjątki.NieprawidłowyChybiłTrafił;

public class GraczMinimalista extends GraczStały{
    public GraczMinimalista(String imię, String nazwisko, String pesel,long środki, int numKolektury){
        super(imię,nazwisko, pesel, środki, new int[]{numKolektury});
        try {
            this.ulubionyGenerator = new ChybiłTrafił(1,1);
        }catch (NieprawidłowyChybiłTrafił e){
            System.out.println("Błąd systemu przy tworzeniu gracza Minimalisty" + e.getMessage());
            System.exit(0);
        }
    }
}
