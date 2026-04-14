package main.tablice.Klasy;

import main.tablice.Interfejsy.*;

public class MacierzDane implements Macierz {

    Skalar[][] tablica;

    // konstruktor na podstawie tablicy wartości
    public MacierzDane(double[][] tablica) {
        Walidator.czyPustyObiekt(tablica,"tablica");
        if( tablica.length == 0 ){
            throw new BłędnaDługość("Błędna długość tablicy macierzy");
        }
        if( tablica[0] == null ){
            throw new PustyObiekt("Błedny argument tablicy macierzy");
        }
        this.tablica = new Skalar[tablica.length][tablica[0].length];
        for(int i = 0 ; i < tablica.length; i++){
            if(tablica[i] == null){
                throw new PustyObiekt("Błedny argument tablicy macierzy");
            }
            if(tablica[i].length != tablica[0].length || tablica[i].length == 0){
                throw new NiezgodnośćRozmiarów("Błędny rozmiar tablicy w inicjalizacji macierzy");
            }
            for(int j = 0; j < tablica[0].length; j++){
                this.tablica[i][j] = new SkalarDane(tablica[i][j]);
            }
        }
    }

    // konstruktor na podstawie tablicy skalarów
    private MacierzDane(Skalar[][] tablica){
        this.tablica = tablica;
    }

    @Override
    public int wymiar() {
        return 2;
    }

    @Override
    public int liczba_elementów() {
        return this.kształt()[0]*this.kształt()[1];
    }

    @Override
    public int[] kształt() {
        return new int[]{this.tablica.length, this.tablica[0].length};
    }

    public double daj(int a,int b){
        if(a < 0 || b < 0 || a > this.kształt()[0] - 1 || b > this.kształt()[1] - 1){
            throw new ZłyIndeks("Zły zakres indeksu w funkcji daj dla macierzy");
        }
        return this.tablica[a][b].daj();
    }

    @Override
    public double daj(int... indeksy){
        Walidator.czyPustyObiekt(indeksy,"indeksy");
        if( indeksy.length != 2 ){
            throw new BłędnaDługość("Błędna długość indeksu w funkcji daj dla macierzy");
        }
        return this.daj(indeksy[0],indeksy[1]);
    }

    @Override
    public void ustaw(double wartość, int a, int b){
        if(a < 0 || b < 0 || a > this.kształt()[0] - 1 || b > this.kształt()[1] - 1){
            throw new ZłyIndeks("Zły zakres indeksu w funkcji ustaw dla macierzy");
        }
        this.tablica[a][b].ustaw(wartość);
    }

    @Override
    public void ustaw(double wartość, int... indeksy){
        Walidator.czyPustyObiekt(indeksy,"indeks");
        if( indeksy.length != 2 ){
            throw new BłędnaDługość("Błędna długość indeksu w funkcji ustaw dla macierzy");
        }
        this.ustaw(wartość,indeksy[0],indeksy[1]);
    }

    public Wektor wiersz(int indeks){
        if(indeks > this.kształt()[0] - 1 || indeks < 0){
            throw new ZłyIndeks("Zły zakres indeksu w funkcji wiersz");
        }
        Skalar[] ans = new SkalarDane[this.kształt()[1]];
        for (int i = 0; i < this.kształt()[1]; i++) {
            ans[i] = this.tablica[indeks][i];
        }
        return new WektorDane(ans, true);
    }

    public Wektor kolumna(int indeks){
        if(indeks > this.kształt()[1] - 1 || indeks < 0){
            throw new ZłyIndeks("Zły zakres indeksu w funkcji kolumna");
        }
        Skalar[] ans = new SkalarDane[this.kształt()[0]];
        for(int i = 0 ; i<this.kształt()[0]; i++){
            ans[i] = this.tablica[i][indeks];
        }
        return new WektorDane(ans,false);
    }

    @Override
    public Macierz kopia(){
        Skalar[][] tablica = new SkalarDane[this.kształt()[0]][this.kształt()[1]];
        for(int i = 0; i< this.kształt()[0]; i++){
            for(int j = 0 ; j < this.kształt()[1]; j++) {
                tablica[i][j] = this.tablica[i][j].kopia();
            }
        }
        return new MacierzDane(tablica);
    }

    @Override
    public Macierz wycinek(int... indeksy) throws BłędneZakresy {
        if(indeksy.length != 4){
            throw new BłędnaDługość("Błędna długość wycinku macierzy");
        }

        int a1 = indeksy[0];
        int b1 = indeksy[1];
        int a2 = indeksy[2];
        int b2 = indeksy[3];

        if( a1 > b1 || a1 < 0 || b1 > this.kształt()[0] - 1
        || a2 > b2 || a2 < 0 || b2 > this.kształt()[1] - 1){
            throw new BłędneZakresy("Błędny zakres indeksów wycinku wycinek");
        }

        Skalar[][] tablica = new SkalarDane[b1 - a1 + 1][b2 - a2 + 1];

        for (int i = 0; i <= b1 - a1; i++) {
            for (int j = 0; j <= b2 - a2; j++) {
                tablica[i][j] = this.tablica[i + a1][j + a2].wycinek();
            }
        }
        return new MacierzDane(tablica);
    }

    @Override
    public String toString(){
        StringBuilder sb = new StringBuilder();
        for(int i = 0 ; i< this.kształt()[0]; i++){
            for(int j=0;j<this.kształt()[1];j++){
                sb.append(this.tablica[i][j].toString()).append(" ");
            }
            if(i != this.kształt()[0]-1){
                sb.append('\n');
            }
        }
        return sb.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (obj == null || getClass() != obj.getClass()) return false;

        Macierz m = (Macierz) obj;
        if (this.kształt()[0] != m.kształt()[0] || this.kształt()[1] != m.kształt()[1]) {
            return false;
        }
        for (int i = 0; i < this.kształt()[0]; i++) {
            if (!this.wiersz(i).equals(m.wiersz(i))) {
                return false;
            }
        }
        return true;
    }

    @Override
    public Macierz suma(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        Macierz m = this.kopia();
        m.dodaj(s);
        return m;
    }

    @Override
    public Macierz suma(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        Macierz m = this.kopia();
        m.dodaj(w);
        return m;
    }

    @Override
    public Macierz suma(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        Macierz k = this.kopia();
        k.dodaj(m);
        return k;
    }

    @Override
    public void dodaj(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        for (int i = 0; i < this.kształt()[0]; i++) {
            this.wiersz(i).dodaj(s);
        }
    }

    @Override
    public void dodaj(Wektor w) {
        Walidator.czyPustyObiekt(w, "Wektor");
        boolean orientacja = w.dajOrientację();
        int n = orientacja ? this.kształt()[1] : this.kształt()[0];
        int m = !orientacja ? this.kształt()[1] : this.kształt()[0];

        if(n != w.liczba_elementów()){
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy dodawaniu wektora do macierzy");
        }

        for (int i = 0; i < m; i++) {
            (orientacja ? this.wiersz(i) : this.kolumna(i)).dodaj(w);
        }
    }


    @Override
    public void dodaj(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        if(this.kształt()[0] != m.kształt()[0] || this.kształt()[1] != m.kształt()[1]) {
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy dodawaniu macierzy do macierzy");
        }
        for(int i = 0 ; i < this.kształt()[0]; i++){
            this.wiersz(i).dodaj(m.wiersz(i));
        }
    }

    @Override
    public Macierz iloczyn(Skalar s) throws ZłyIndeks {
        Walidator.czyPustyObiekt(s,"Skalar");
        Macierz m = this.kopia();
        m.przemnóż(s);
        return m;
    }

    @Override
    public Wektor iloczyn(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        w.transponuj();
        int n = w.dajOrientację() ? this.kształt()[1] : this.kształt()[0];
        int m = !w.dajOrientację() ? this.kształt()[1] : this.kształt()[0];
        if(n != w.liczba_elementów()){
            throw new NiezgodnośćRozmiarów("Niezgodność rozmiarów przy mnożeniu macierzy przez wektor");
        }
        Skalar[] tablica = new SkalarDane[m];

        for (int i = 0; i < m; i++) {
            tablica[i] = (Skalar) w.iloczyn(w.dajOrientację() ? this.wiersz(i) : this.kolumna(i));
        }

        w.transponuj();
        return new WektorDane(tablica, w.dajOrientację());
    }


    @Override
    public Macierz iloczyn(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        if(this.kształt()[1]!=m.kształt()[0]){
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy mnożeniu macierzy przez macierz");
        }
        Macierz k = new MacierzDane(new double[this.kształt()[0]][m.kształt()[1]]);
        for(int i = 0 ; i < m.kształt()[1];i++){
            k.kolumna(i).przypisz(this.iloczyn(m.kolumna(i)));
        }
        return k;
    }

    @Override
    public void przemnóż(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        for(int i = 0; i< this.kształt()[0]; i++){
            this.wiersz(i).przemnóż(s);
        }
    }

    @Override
    public void przemnóż(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        throw new NiezgodnośćRozmiarów("Nie można przemnożyć macierz przez wektor");
    }

    @Override
    public void przemnóż(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        if(this.kształt()[1] != m.kształt()[1]){
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy przemnażaniu macierzy");
        }
        this.przypisz(this.iloczyn(m));
    }

    @Override
    public void przypisz(Skalar s){
        Walidator.czyPustyObiekt(s,"Skalar");
        for(int i = 0; i< this.kształt()[0]; i++){
            this.wiersz(i).przypisz(s);
        }
        
    }

    @Override
    public void przypisz(Wektor w){
        Walidator.czyPustyObiekt(w,"Wektor");
        int n = w.dajOrientację() ? this.kształt()[1] : this.kształt()[0];
        int m = !w.dajOrientację() ? this.kształt()[1] : this.kształt()[0];
        if(n != w.liczba_elementów()){
            throw new NiezgodnośćRozmiarów("Niezgodnośc rozmiarów przy przypisywaniu wektora do macierzy");
        }
        for (int i = 0; i < m; i++) {
            (w.dajOrientację() ? this.wiersz(i) : this.kolumna(i)).przypisz(w);
        }

    }

    @Override
    public void przypisz(Macierz m){
        Walidator.czyPustyObiekt(m,"Macierz");
        if(this.kształt()[0] != m.kształt()[0] || this.kształt()[1] != m.kształt()[1]){
            throw new NiezgodnośćRozmiarów("Niezgodność rozmiarów przy przypisywaniu macierzy do macierzy");
        }
        for(int i=0; i < this.kształt()[0]; i++){
            this.wiersz(i).przypisz(m.wiersz(i));
        }
    }

    @Override
    public Macierz negacja(){
        Macierz m = this.kopia();
        m.zaneguj();
        return m;
    }

    @Override
    public void transponuj() {
        Skalar[][] tablica = new SkalarDane[this.kształt()[1]][this.kształt()[0]];
        for(int i = 0; i<this.kształt()[0]; i++){
            for(int j = 0; j < this.kształt()[1]; j++){
                tablica[j][i] = this.tablica[i][j];
            }
        }
        this.tablica = tablica;
    }

    @Override
    public void zaneguj() throws ZłyIndeks {
        for(int i = 0; i < this.kształt()[0]; i++){
            this.wiersz(i).zaneguj();
        }
    }
}