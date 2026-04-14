package main.tablice.Klasy;

import main.tablice.Interfejsy.*;

public class SkalarDane implements Skalar {
    private final Komórka komórka;

    // konstruktor na podtsawie wartości:
    public SkalarDane(double wartość) {
        this.komórka = new Komórka(wartość);
    }

    // konstruktor na podstawie komórki:
    private SkalarDane(Komórka komórka){
        this.komórka = komórka;
    }

    @Override
    public int wymiar() {
        return 0;
    }

    @Override
    public int liczba_elementów() {
        return 1;
    }

    @Override
    public int[] kształt() {
        return new int[]{};
    }

    public double daj(){
        return this.komórka.daj();
    }

    @Override
    public double daj(int... indeksy){
        Walidator.czyPustyObiekt(indeksy,"indeks");
        if( indeksy.length != 0 ){
            throw new BłędnaDługość("Błędna długość indeksu w funkcji daj dla skalara");
        }
        return this.daj();
    }

    public void ustaw(double wartość){
        this.komórka.ustaw(wartość);
    }

    @Override
    public void ustaw(double wartość, int... indeksy){
        Walidator.czyPustyObiekt(indeksy,"indeks");
        if( indeksy.length != 0 ){
            throw new BłędnaDługość("Błędna długość indeksu w funkcji ustaw dla skalara");
        }
        this.ustaw(wartość);
    }

    @Override
    public Skalar kopia(){
        return new SkalarDane(this.daj());
    }


    public Skalar wycinek(int... indeksy){
        Walidator.czyPustyObiekt(indeksy,"indeks");
        if( indeksy.length != 0 ){
            throw new BłędnaDługość("Błędna długość indeksu w funkcji wycinka dla skalara");
        }
        return new SkalarDane(this.komórka);
    }

    @Override
    public String toString() {
        return this.komórka.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (obj == null || getClass() != obj.getClass()) return false;
        SkalarDane other = (SkalarDane) obj;
        return this.daj() == other.daj();
    }

    @Override
    public Skalar suma(Skalar s) {
        Walidator.czyPustyObiekt(s,"Skalar");
        return new SkalarDane(s.daj() + this.daj());
    }

    @Override
    public Wektor suma(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        return w.suma(this);
    }

    @Override
    public Macierz suma(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        return m.suma(this);
    }

    @Override
    public void dodaj(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        this.ustaw(s.daj() + this.daj());
    }

    @Override
    public void dodaj(Wektor w) {
        throw new NiezgodnośćRozmiarów("Nie można dodać wektoru do skalara");
    }

    @Override
    public void dodaj(Macierz m){
        throw new NiezgodnośćRozmiarów("Nie można dodać macierzy do skalara");
    }

    @Override
    public Skalar iloczyn(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        return new SkalarDane(s.daj() * this.daj());
    }

    @Override
    public Wektor iloczyn(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        return w.iloczyn(this);
    }

    @Override
    public Macierz iloczyn(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        return m.iloczyn(this);
    }

    @Override
    public void przemnóż(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        this.ustaw(s.daj() * this.daj());
    }

    @Override
    public void przemnóż(Wektor w){
        throw new NiezgodnośćRozmiarów("Nie można przemnożyć skalara przez wektor");
    }

    @Override
    public void przemnóż(Macierz m){
        throw new NiezgodnośćRozmiarów("Nie można przemnożyć skalara przez macierz");
    }

    @Override
    public void przypisz(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        this.ustaw(s.daj());
    }

    @Override
    public void przypisz(Wektor w){
        throw new NiezgodnośćRozmiarów("Nie można przypisać wektoru do skalara");
    }

    @Override
    public void przypisz(Macierz m){
        throw new NiezgodnośćRozmiarów("Nie można przypisać macierzy do skalara");
    }

    @Override
    public Skalar negacja(){
        return new SkalarDane(-this.daj());
    }

    @Override
    public void zaneguj(){
        this.ustaw(-this.daj());
    }

    @Override
    public void transponuj() {}
}