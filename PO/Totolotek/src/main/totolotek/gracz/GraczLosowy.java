package main.totolotek.gracz;

import main.totolotek.pomocniczeklasy.Losowe;
import main.totolotek.system.CentralaTotolotka;
import main.totolotek.system.ChybiłTrafił;
import main.totolotek.wyjątki.NieprawidłowyChybiłTrafił;
import main.totolotek.wyjątki.OszustwoGracza;
import main.totolotek.wyjątki.ZłaKolektura;

public class GraczLosowy extends Gracz {
    public GraczLosowy(String imię, String nazwisko, String pesel) {
        super(imię, nazwisko, pesel, 0);
        środki = Losowe.losowyNumer((long)1e8-1);
    }

    @Override
    public void działajKup(){
        int numerKolektury = Math.toIntExact(Losowe.losowyNumer(CentralaTotolotka.getInstance().liczbaKolektur()));
        int liczbaKuponów = Math.toIntExact(Losowe.losowyNumer(100));
        try {
            for(int i = 0; i < liczbaKuponów; i++){
                ChybiłTrafił c = new ChybiłTrafił(Math.toIntExact(Losowe.losowyNumer(10)),
                        Math.toIntExact(Losowe.losowyNumer(8)));
                if(!czyStarczy(c.cenaWygenerowaniaKuponu())){
                    break;
                }else{
                    kupKupon(c, numerKolektury);
                }
            }
        } catch (ZłaKolektura | NieprawidłowyChybiłTrafił e) {
            System.out.println("Błąd systemu przy kupowaniu przez gracza losowego: " + e.getMessage());
            System.exit(0);
        }
    }
}
