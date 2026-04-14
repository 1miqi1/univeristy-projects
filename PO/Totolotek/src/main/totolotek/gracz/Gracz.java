package main.totolotek.gracz;

import main.totolotek.system.*;
import main.totolotek.wyjątki.BłądWyboruLosowania;
import main.totolotek.wyjątki.OszustwoGracza;
import main.totolotek.wyjątki.ZłaKolektura;

import java.util.ArrayList;
import java.util.HashMap;

public abstract class Gracz {
    protected final String imię;
    protected final String nazwisko;
    protected final String pesel;
    protected boolean czyAktywny;
    protected long środki;
    protected HashMap<Kupon,Boolean> zakupioneKupony;

    protected Gracz(String imię, String nazwisko, String pesel, long środki) {
        this.imię = imię;
        this.nazwisko = nazwisko;
        this.pesel = pesel;
        this.czyAktywny = false;
        zakupioneKupony = new HashMap();
        this.środki = środki;
    }

    public boolean czyAktywny(){
        return czyAktywny;
    }

    public boolean czyStarczy(long wartość){
        return środki >= wartość;
    }

    public boolean czyMożna(GeneratorKuponów gk){
        return czyStarczy(gk.cenaWygenerowaniaKuponu());
    }

    public long zapłać(long cena) throws OszustwoGracza {
        if(cena > środki){
            throw new OszustwoGracza("Brak Środków");
        }
        środki -= cena;
        return cena;
    }

    public void weźKupon(Kupon kupon){
        zakupioneKupony.put(kupon,true);
    }

    public void wykorzystajKupon(Kupon k){
        if(zakupioneKupony.containsKey(k) && zakupioneKupony.get(k)){
            try {
                środki += k.gdzieWydany().wypłać(k);
                zakupioneKupony.put(k,false);
            } catch (OszustwoGracza | BłądWyboruLosowania e) {
                System.out.println("Błąd systemu przy wykorzystwaniu kuponu" + e.getMessage());
            }
        }
    }

    public void kupKupon(GeneratorKuponów gk, int numerKolektury) throws ZłaKolektura {
        this.czyAktywny = true;
        try{
            if(czyStarczy(gk.cenaWygenerowaniaKuponu())){
                Kolektura k = CentralaTotolotka.getInstance().dajKolekturę(numerKolektury);
                k.sprzedajKupon(gk,this);
            }
        }catch (OszustwoGracza | BłądWyboruLosowania e){
            System.out.println("Błąd system" + e.getMessage());
            System.out.println(e.getMessage());
        }
        this.czyAktywny = false;
    }

    public void działajOdbierz(){
        for(Kupon k : zakupioneKupony.keySet()){
            if(k.ostatnieLosowanie() < CentralaTotolotka.getInstance().numerKolejnegoLosowanie()){
                wykorzystajKupon(k);
            }
        }
    }

    public abstract void działajKup();

    public void napiszOSobie(){
        System.out.println("Imię i Nazwisko: " + imię + " " + nazwisko + " Pesel: " + pesel + "Środki: " + środki);
    }

    public long środki(){
        return środki;
    }

    public long liczbaKupionychKuponów(){
        return zakupioneKupony.size();
    }

    public void wypiszIdentyfikatory(){
        if(zakupioneKupony.keySet().size() > 0){
            System.out.println("Identyfikatory zakupionych kuponów");
            for(Kupon k : zakupioneKupony.keySet()){
                System.out.println(k.id());
            }
        }
        else{
            System.out.println("Brak zakupionych kuponów");
        }
    }
}
