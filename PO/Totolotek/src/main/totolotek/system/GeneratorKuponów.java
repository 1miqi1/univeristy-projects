package main.totolotek.system;

public abstract class GeneratorKuponów {
    abstract Kupon wygenerujKupon(int numerLosowania, int numerKolektury, String identyfikator);
    public long cenaWygenerowaniaKuponu() {
        return wygenerujKupon(0,0,"").cena();
    }
}
