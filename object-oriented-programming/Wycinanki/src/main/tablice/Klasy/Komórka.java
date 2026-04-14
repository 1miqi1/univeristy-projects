package main.tablice.Klasy;

public class Komórka {
    private double wartosc;

    public Komórka(double wartosc) {
        this.wartosc = wartosc;
    }

    public double daj() {
        return wartosc;
    }

    public void ustaw(double wartosc) {
        this.wartosc = wartosc;
    }

    @Override
    public String toString() {
        return Double.toString(this.wartosc);
    }
}

