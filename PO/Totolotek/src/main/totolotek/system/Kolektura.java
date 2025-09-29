package main.totolotek.system;

import java.util.HashMap;

import main.totolotek.wyjątki.*;
import main.totolotek.pomocniczeklasy.*;
import main.totolotek.gracz.*;

public class Kolektura {
    private final int numer;
    private final HashMap<Kupon, Boolean> sprzedaneKupony;

    Kolektura(int numer) {
        this.numer = numer;
        this.sprzedaneKupony = new HashMap<>();
    }

    public long wypłać(Kupon k) throws OszustwoGracza, BłądWyboruLosowania {
        if (!sprzedaneKupony.containsKey(k)) {
            throw new OszustwoGracza(" kupon nie został sprzedany w tej kolekturze");
        } else if (sprzedaneKupony.get(k)) {
            throw new OszustwoGracza("Ten kupon jest wykorzystany");
        }
        sprzedaneKupony.put(k, true);
        return CentralaTotolotka.getInstance().wypłać(k);
    }

    public void sprzedajKupon(GeneratorKuponów gk, Gracz g) throws OszustwoGracza, BłądWyboruLosowania {
        if(!g.czyAktywny()){
            throw new OszustwoGracza("Wykryto włamanie na konto innego gracza");
        }
        long cena = gk.cenaWygenerowaniaKuponu();
        if(!g.czyStarczy(cena)){
            throw new OszustwoGracza("Nie wsytarczająca liczba środków na zakup kuponu");
        }

        Kupon k = gk.wygenerujKupon(
                CentralaTotolotka.getInstance().numerKolejnegoLosowanie(),
                numer,
                this.wygenerujId()
        );
        CentralaTotolotka.getInstance().wprowadźKupon(k);
        sprzedaneKupony.put(k, false);
        g.weźKupon(k);
        g.zapłać(cena);
    }

    private String wygenerujId() {
        CentralaTotolotka centrala = CentralaTotolotka.getInstance();
        StringBuilder sb = new StringBuilder();
        sb.append(centrala.liczbaSprzedanychKuponów() + 1);
        sb.append("-");
        sb.append(numer);
        sb.append("-");
        long losowyZnacznik = Losowe.losowyNumer((long) 1e8);
        sb.append(losowyZnacznik);
        sb.append("-");

        int sumaKontrolna = 0;
        sumaKontrolna += PomoczniczeFunkcje.sumaCyfr(numer);
        sumaKontrolna += PomoczniczeFunkcje.sumaCyfr(losowyZnacznik);
        sumaKontrolna += PomoczniczeFunkcje.sumaCyfr(centrala.liczbaSprzedanychKuponów() + 1);
        sumaKontrolna %= 100;
        sb.append(sumaKontrolna);
        return sb.toString();
    }
}
