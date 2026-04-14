package main.totolotek.system;

public class BudżetPaństwa {
    private static BudżetPaństwa instance;
    private long subsydia;
    private long podatki;

    private BudżetPaństwa() {
        subsydia = 0;
        podatki = 0;
    }

    public static synchronized BudżetPaństwa getInstance() {
        if (instance == null) {
            instance = new BudżetPaństwa();
        }
        return instance;
    }

    long przekażSubsydia(long wartość) {
        subsydia += wartość;
        return wartość;
    }

    void pobierzPodatek(long wartość){
        podatki += wartość;
    }

    public long dajSubsydia() {
        return subsydia;
    }

    public long dajPodatki() {
        return podatki;
    }

    public void wypiszSubsydia() {
        System.out.println("Wielkość pobranych subsydiów: " + subsydia);
    }

    public void wypiszPodatek() {
        System.out.println("Wielkość wpływów do budżetu: " + podatki);
    }
}
