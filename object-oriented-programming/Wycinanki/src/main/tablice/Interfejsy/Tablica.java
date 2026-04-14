package main.tablice.Interfejsy;

public interface Tablica {
    int wymiar();
    int liczba_elementów();
    int[] kształt();
    double daj(int... indeksy);
    void ustaw(double x, int... indeksy);
    Tablica kopia();
    Tablica wycinek(int... indeksy);

    @Override
    String toString();

    @Override
    boolean equals(Object obj);

    Tablica suma(Skalar s);
    Tablica suma(Wektor w);
    Tablica suma(Macierz m);

    void dodaj(Skalar s);
    void dodaj(Wektor w);
    void dodaj(Macierz m);

    Tablica iloczyn(Skalar s);
    Tablica iloczyn(Wektor w);
    Tablica iloczyn(Macierz m);

    void przemnóż(Skalar s);
    void przemnóż(Wektor w);
    void przemnóż(Macierz m);


    void przypisz(Skalar s);
    void przypisz(Wektor w);
    void przypisz(Macierz m);

    Tablica negacja();
    void zaneguj();
    void transponuj();

}