package main.totolotek.system;

import main.totolotek.pomocniczeklasy.Stałe;
import main.totolotek.wyjątki.BłądWyboruLosowania;
import main.totolotek.wyjątki.ZłaKolektura;

import java.util.ArrayList;

public class CentralaTotolotka {
    private static CentralaTotolotka instance;
    private long budżetCentrali;
    private final ArrayList<Losowanie> losowania;
    private final ArrayList<Kolektura> kolektury;
    public int liczbaSprzedanychKuponów;
    public static final long WYSOKANAGRODA = 228000;
    public static final long NANagrody = 49;

    public static CentralaTotolotka getInstance() {
        if (instance == null) {
            instance = new CentralaTotolotka();
        }
        return instance;
    }

    private CentralaTotolotka() {
        this.budżetCentrali = 0;
        this.liczbaSprzedanychKuponów = 0;
        this.kolektury = new ArrayList<>();
        this.losowania = new ArrayList<>();
        for (int i = 0; i < Stałe.MAXLICZBALOSOWAŃ; i++) {
            losowania.add(new Losowanie(i + 1));
        }
    }

    // DODAWANIE KOLEKTUR DO SYSTEMU
    public Kolektura utwórzKolekturę(){
        Kolektura k = new Kolektura(kolektury.size()+1);
        kolektury.add(k);
        return k;
    }

    public Kolektura dajKolekturę(int numer) throws ZłaKolektura {
        if( numer > kolektury.size() || numer < 0){
            throw new ZłaKolektura("Nie istnieje kolekutra o danym numerze");
        }
        return kolektury.get(numer - 1);
    }

    public int liczbaKolektur(){
        return kolektury.size();
    }

    public int liczbaSprzedanychKuponów() {
        return liczbaSprzedanychKuponów;
    }

    // OBSŁUGA LOSOWAŃ + KUPONÓW
    public int numerKolejnegoLosowanie() {
        return losowania.size() - Stałe.MAXLICZBALOSOWAŃ + 1;
    }

    void zwiększPulęNastępnegoLosowania(long wartosc) throws BłądWyboruLosowania {
        losowania.get(numerKolejnegoLosowanie()-1).zwiększPulę(wartosc);
    }

    long wypłać(Kupon kupon) throws BłądWyboruLosowania {
        long wygrana = 0;
        long podatek = 0;

        ArrayList<Zakład> z = kupon.zakłady();

        int pierwsze = kupon.pierwszeLosowanie();
        int ostatnie = kupon.ostatnieLosowanie();
        for (int i = pierwsze; i <= Math.min(numerKolejnegoLosowanie(),ostatnie); i++) {
            Losowanie l = losowania.get(i - 1);
            for (Zakład zakład : z) {
                long w = l.wygrana(zakład);
                wygrana += w;
                if (w > WYSOKANAGRODA) {
                    podatek += w * 10 / 100;
                }
            }
        }

        BudżetPaństwa.getInstance().pobierzPodatek(podatek);
        if (wygrana > budżetCentrali) {
            budżetCentrali += BudżetPaństwa.getInstance().przekażSubsydia(wygrana - budżetCentrali);
        }
        budżetCentrali -= wygrana;
        return wygrana - podatek;
    }

    void wprowadźKupon(Kupon kupon) throws BłądWyboruLosowania {
        ArrayList<Zakład> z = kupon.zakłady();
        int n = kupon.liczbaLosowań();

        long cena = kupon.cena() - kupon.podatek();
        BudżetPaństwa.getInstance().pobierzPodatek(kupon.podatek());
        long wartośćNagród = cena * NANagrody / 100;
        budżetCentrali += cena - wartośćNagród;

        int pierwsze = kupon.pierwszeLosowanie();
        int ostatnie = kupon.ostatnieLosowanie();
        for (int i = pierwsze; i <= ostatnie; i++) {
            Losowanie losowanie = losowania.get(i - 1);
            losowanie.dodajZakłady(z);
            losowanie.zwiększPulę(wartośćNagród / n);
        }
        liczbaSprzedanychKuponów++;
    }

    public void przeprowadźLosowanie() throws BłądWyboruLosowania {
        Losowanie ak = losowania.get(this.numerKolejnegoLosowanie()-1);
        losowania.add(new Losowanie(this.losowania.size() + 1));
        ak.losuj();
    }

    void kumulacja(long a) {
        losowania.get(this.numerKolejnegoLosowanie()-1).kumulacja(a);
    }

    // Informacje publiczne:
    public void sprawozdanie(int numerLosowania) throws BłądWyboruLosowania {
        if(numerLosowania >= numerKolejnegoLosowanie()  || numerLosowania <= 0) {
            throw new BłądWyboruLosowania(numerLosowania);
        }
        losowania.get(numerLosowania - 1).sprawozdanie();
    }

    public void wypiszStańŚrodków() {
        System.out.println("Budżet Centrali wynosi: " + budżetCentrali);
    }

    public long wygranaStopnia(int numerLosowania, int stopień) throws BłądWyboruLosowania {
        if(numerLosowania >= numerKolejnegoLosowanie()  || numerLosowania <= 0) {
            throw new BłądWyboruLosowania(numerLosowania);
        }
        return losowania.get(numerLosowania - 1).wygrana(stopień);
    }

    public long pulaStopniaPierwszego(int numerLosowania) throws BłądWyboruLosowania {
        if(numerLosowania >= numerKolejnegoLosowanie()  || numerLosowania <= 0) {
            throw new BłądWyboruLosowania(numerLosowania);
        }
        return losowania.get(numerLosowania - 1).pulaStopnia()[1];
    }

    public int[] zwycięskaSzóstka(int numerLosowania) throws BłądWyboruLosowania {
        if(numerLosowania >= numerKolejnegoLosowanie()  || numerLosowania <= 0) {
            throw new BłądWyboruLosowania(numerLosowania);
        }
        return losowania.get(numerLosowania - 1).zwycięskaSzóstka().liczby();
    }
}
