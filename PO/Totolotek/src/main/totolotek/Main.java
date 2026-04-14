package main.totolotek;

import main.totolotek.gracz.Gracz;
import main.totolotek.pomocniczeklasy.Losowe;
import main.totolotek.system.BudżetPaństwa;
import main.totolotek.system.CentralaTotolotka;
import main.totolotek.system.Kolektura;
import main.totolotek.wyjątki.BłądWyboruLosowania;

import java.util.ArrayList;

public class Main {
    public static void main(String[] args) throws BłądWyboruLosowania {
        ArrayList<Kolektura> kolektury = new ArrayList<>();
        ArrayList<Gracz> gracze = new ArrayList<>();
        for(int i = 0; i < 10; i++){
            kolektury.add(CentralaTotolotka.getInstance().utwórzKolekturę());
        }
        for(int i = 0; i < 200; i++){
            gracze.add(Losowe.losowyPoprawnyGraczLosowy());
            gracze.add(Losowe.losowyPoprawnyGraczStałoliczbowy());
            gracze.add(Losowe.losowyPoprawnyGraczStałoblankietowy());
            gracze.add(Losowe.losowyPoprawnyGraczMinimalista());
        }
        for(int i = 0; i < 20; i++){
            for(Gracz g : gracze){
                g.działajKup();
            }
            CentralaTotolotka.getInstance().przeprowadźLosowanie();
            for(Gracz g : gracze){
                g.działajOdbierz();
            }
        }

        for(int i = 0; i < 20; i++){
            CentralaTotolotka.getInstance().sprawozdanie(i+1);
        };
        BudżetPaństwa.getInstance().wypiszPodatek();
        BudżetPaństwa.getInstance().wypiszSubsydia();
    }
}
