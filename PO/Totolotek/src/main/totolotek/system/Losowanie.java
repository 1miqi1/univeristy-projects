package main.totolotek.system;

import main.totolotek.pomocniczeklasy.PomoczniczeFunkcje;
import main.totolotek.wyjątki.BłądWyboruLosowania;

import java.util.ArrayList;


class Losowanie {
    private final int numerLosowania;
    private long pulaNagród;
    private long[] pulaStopnia;
    private long[] liczbaWygranych;
    private Zakład zwycięskaSzóstka;
    private ArrayList<Zakład> zakłady;
    private boolean czyLosowanieOdbyte = false;

    private static final long[] MINIMALNE = new long[]{0, (long) 2e8, 0, 3600, 2400 };

    Losowanie(int numerLosowania) {
        this.numerLosowania = numerLosowania;
        this.pulaNagród = 0;
        this.pulaStopnia = new long[]{0,0,0,0,0};
        this.liczbaWygranych = new long[]{0,0,0,0,0};
        this.zakłady = new ArrayList<>();
    }

    void dodajZakłady(ArrayList<Zakład> z) throws BłądWyboruLosowania {
        if(czyLosowanieOdbyte){
            throw new BłądWyboruLosowania("nie można dodac zakładów");
        }
        zakłady.addAll(z);
    }

    void zwiększPulę(long wartość) throws BłądWyboruLosowania {
        if(czyLosowanieOdbyte){
            throw new BłądWyboruLosowania("nie można zwiększyś puli");
        }
        pulaNagród += wartość;
    }

    long[] pulaStopnia() throws BłądWyboruLosowania {
        if(!czyLosowanieOdbyte){
            throw new BłądWyboruLosowania(numerLosowania);
        }
        return pulaStopnia;
    }

    Zakład zwycięskaSzóstka() throws BłądWyboruLosowania {
        if(!czyLosowanieOdbyte){
            throw new BłądWyboruLosowania(numerLosowania);
        }
        return zwycięskaSzóstka;
    }

    void kumulacja(long a){
        pulaStopnia[1] += a;
    }

    long wygrana(int stopień) throws BłądWyboruLosowania {
        if(!czyLosowanieOdbyte){
            throw new BłądWyboruLosowania(numerLosowania);
        }
        return liczbaWygranych[stopień] == 0 ? 0 : pulaStopnia[stopień]/liczbaWygranych[stopień];
    }

    long wygrana(Zakład z) throws BłądWyboruLosowania {
        return wygrana(z.stopieńWygranej(zwycięskaSzóstka));
    }

    void losuj() throws BłądWyboruLosowania {
        if(czyLosowanieOdbyte){
            throw new BłądWyboruLosowania("nie można drugi raz losować");
        }
        zwycięskaSzóstka = new Zakład();
        wyznaczNagrody();
        czyLosowanieOdbyte = true;
        for(int i = 2; i <= 3; i++){
            if(liczbaWygranych[i] == 0){
                CentralaTotolotka.getInstance().zwiększPulęNastępnegoLosowania(pulaStopnia[i]);
            }
        }
        System.out.println(this);
    }

    private void wyznaczNagrody() throws BłądWyboruLosowania {
        if(czyLosowanieOdbyte){
            throw new BłądWyboruLosowania("nie można drugi wyznaczać nagród");
        }
        for(Zakład z : zakłady){
            liczbaWygranych[z.stopieńWygranej(zwycięskaSzóstka)]++;
        }
        long pozostałeNagrody = pulaNagród;
        pulaStopnia[1] += (pulaNagród * 44) / 100;
        if(liczbaWygranych[1] == 0){
            CentralaTotolotka.getInstance().kumulacja(pulaStopnia[1]);
        }
        pozostałeNagrody -= pulaStopnia[1];
        pulaStopnia[1] = Long.max(pulaStopnia[1], MINIMALNE[1] );

        pulaStopnia[2] = (pulaNagród * 8) / 100;
        pozostałeNagrody -= pulaStopnia[2];

        pulaStopnia[3] = Long.max(pozostałeNagrody, MINIMALNE[3] * liczbaWygranych[3] );

        pulaStopnia[4] = MINIMALNE[4] * liczbaWygranych[4];
    }

    void sprawozdanie() throws BłądWyboruLosowania {
        if(!czyLosowanieOdbyte){
            throw new BłądWyboruLosowania(numerLosowania);
        }
        System.out.println("Losowanie nr " + numerLosowania );

        for(int i = 1; i <= 4 ; i++){
            StringBuilder sb = new StringBuilder();
            sb.append("Pula " + i + " Liczba trafień: " + liczbaWygranych[i] + " Pula: " +
                    PomoczniczeFunkcje.złote(pulaStopnia[i]));
            if( liczbaWygranych[i] != 0 ){
                sb.append(" Kwota Wygrana: " + PomoczniczeFunkcje.złote(wygrana(i)));
            }
            System.out.println(sb.toString());
        }
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Losowanie nr " + numerLosowania + "\n");
        if(czyLosowanieOdbyte){
            sb.append("Wynik: " + zwycięskaSzóstka);
        }
        return sb.toString();
    }
}
