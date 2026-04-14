package main.tablice.Klasy;

import main.tablice.Interfejsy.*;

public class WektorDane implements Wektor {
    private boolean orientacja; // true = poziomy, false = pionowy 
    private final Skalar[] tablica;

    // konstruktor na bazie tablicy:
    public WektorDane(double[] tablica, boolean orientacja){
        Walidator.czyPustyObiekt(tablica,"tablica");
        this.tablica = new Skalar[tablica.length];
        for(int i=0; i<tablica.length; i++){
            this.tablica[i] = new SkalarDane(tablica[i]);
        }
        this.orientacja = orientacja;
    }

    // konstruktor na bazie skalarów:
    protected WektorDane(Skalar[] tablica, boolean orientacja){
        this.tablica = tablica;
        this.orientacja = orientacja;
    }

    public boolean dajOrientację(){
        return this.orientacja;
    }

    @Override
    public int wymiar() {
        return 1;
    }

    @Override
    public int liczba_elementów(){
        return tablica.length;
    }


    @Override
    public int[] kształt() {
        return new int[]{this.liczba_elementów()};
    }

    public double daj(int indeks){
        if(indeks > this.liczba_elementów() - 1 || indeks < 0){
            throw new ZłyIndeks("Zły zakres indeksu przy funkcji daj dla wektora");
        }
        return this.tablica[indeks].daj();
    }

    @Override
    public double daj(int... indeksy){
        Walidator.czyPustyObiekt(indeksy,"indeks");
        if( indeksy.length != 1 ){
            throw new BłędnaDługość("Błędna długość indeksu w funkcji daj dla wektora");
        }
        return this.daj(indeksy[0]);

    }

    public void ustaw(double wartość, int indeks){
        if(indeks > this.liczba_elementów() - 1 || indeks < 0){
            throw new ZłyIndeks("Zły zakres indeksu w funkcji ustaw dla wektora");
        }
        this.tablica[indeks].ustaw(wartość);
    }

    @Override
    public void ustaw(double wartość, int... indeksy){
        Walidator.czyPustyObiekt(indeksy,"indeks");
        if( indeksy.length != 1 ){
            throw new BłędnaDługość("Błędna długość indeksu w funkcji ustaw dla wektora");
        }
        this.ustaw(wartość,indeksy[0]);
    }


    @Override
    public Wektor kopia(){
        double[] tablica = new double[this.liczba_elementów()];
        for (int i = 0; i < this.liczba_elementów(); i++) {
            tablica[i] = this.daj(i);
        }
        return new WektorDane(tablica,this.orientacja);
    }

    @Override
    public Wektor wycinek(int... indeksy){
        Walidator.czyPustyObiekt(indeksy, "indeks");
        if( indeksy.length != 2 ){
            throw new BłędnaDługość("Błędna długość wycinku wektora");
        }
        int a = indeksy[0];
        int b = indeksy[1];

        if( a > b || a < 0 || b > this.liczba_elementów() - 1 ){
            throw new BłędneZakresy("Błędny zakres indeksów wycinka wektora");
        }

        Skalar[] tablica = new Skalar[b - a + 1];
        for(int i = 0; i < b-a+1; i++){
            tablica[i] = this.tablica[i+a].wycinek();
        }
        return new WektorDane(tablica, this.orientacja);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();

        for (int i = 0; i < this.liczba_elementów(); i++) {
            sb.append(this.tablica[i].toString());
            sb.append(' ');
        }

        sb.append('\n');
        sb.append("Orientacja: ");
        sb.append(this.orientacja ? "Pozioma" : "Pionowa");

        return sb.toString().trim();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (obj == null || getClass() != obj.getClass()) return false;

        WektorDane other = (WektorDane) obj;

        if (this.orientacja != other.orientacja) return false;
        if (this.liczba_elementów() != other.liczba_elementów()) return false;

        
        for (int i = 0; i < this.liczba_elementów(); i++) {
            if (this.daj(i) != other.daj(i)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public Wektor suma(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        Wektor k = this.kopia();
        k.dodaj(s);
        return k;
    }

    @Override
    public Wektor suma(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        Wektor k = this.kopia();
        k.dodaj(w);
        return k;
    }

    @Override
    public Macierz suma(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        return m.suma(this);
    }


    //DODAJ
    @Override
    public void dodaj(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        for (int i = 0; i < this.tablica.length; i++) {
            this.ustaw(s.daj() + this.daj(i), i);
        }
    }

    public void dodaj(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        if (w.dajOrientację() != this.orientacja) {
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy dodawaniu wektora do wektora");
        }
        if(this.liczba_elementów() != w.liczba_elementów()){
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy dodawaniu wektora do macierzy");
        }
        for (int i = 0; i < this.liczba_elementów() ; i++) {
            this.ustaw(w.daj(i) + this.daj(i), i);
        }
    }

    public void dodaj(Macierz m){
        throw new NiezgodnośćRozmiarów("Nie można dodać macierzy do wektora");
    }

    @Override
    public Wektor iloczyn(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        Wektor k = this.kopia();
        k.przemnóż(s);
        return k;
    }

    @Override
    public Tablica iloczyn(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        if( w.dajOrientację() == true && this.dajOrientację() == false ){
            double[][] ans = new double[w.liczba_elementów()][this.liczba_elementów()];
            for(int i = 0; i < w.liczba_elementów(); i++){
                for(int j=0; j<this.liczba_elementów();j++){
                    ans[i][j] = w.daj(j) * this.daj(i);
                }
            }
            return new MacierzDane(ans);
        }
        if(this.liczba_elementów() != w.liczba_elementów()){
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy mnozeniu wektora przez wektor");
        }
        double ans  = 0;
        for(int i = 0; i < this.liczba_elementów(); i++){
            ans+= w.daj(i) * this.daj(i);
        }
        if( this.orientacja == w.dajOrientację() ){
            return new SkalarDane(ans);
        }else{
            return new MacierzDane(new double[][]{{ans}});
        }
    }

    @Override
    public Wektor iloczyn(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        return m.iloczyn(this);
    }

    @Override
    public void przemnóż(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        for (int i = 0; i < this.liczba_elementów(); i++) {
            ustaw(s.daj() * this.daj(i), i);
        }
    }

    @Override
    public void przemnóż(Wektor w){
        throw new NiezgodnośćRozmiarów("Nie można przemnożyć wektora przez wektor");
    }

    @Override
    public void przemnóż(Macierz m){
        throw new NiezgodnośćRozmiarów("Nie można przemnożyć wektora przez macierz");
    }

    @Override
    public void przypisz(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        for(int i = 0; i < this.liczba_elementów(); i++){
            this.tablica[i].ustaw(s.daj());
        }
    }

    @Override
    public void przypisz(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        if(this.liczba_elementów() != w.liczba_elementów()){
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy przypisywaniu wektora do wektora");
        }
        for (int i = 0; i < this.liczba_elementów(); i++) {
            this.ustaw(w.daj(i), i);
        }
        this.orientacja = w.dajOrientację();

    }

    @Override
    public void przypisz (Macierz m){
        throw new NiezgodnośćRozmiarów("Nie można przypisać macierzy do wektora");
    }

    @Override
    public Wektor negacja () {
        Wektor k = this.kopia();
        k.zaneguj();
        return k;
    }

    @Override
    public void zaneguj () {
        for (int i = 0; i < this.liczba_elementów(); i++) {
            this.tablica[i].zaneguj();
        }
    }

    @Override
    public void transponuj () {
        orientacja = !orientacja;
    }
}