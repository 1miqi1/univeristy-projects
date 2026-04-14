package main.tablice.Interfejsy;

public interface Wektor extends Tablica {
    boolean dajOrientację();
    double daj(int indeks);
    void ustaw(double wartość, int indeks);

    @Override
    Wektor kopia();
    @Override
    Wektor wycinek(int... indeksy);

    @Override
    Wektor suma(Skalar s);
    @Override
    Wektor suma(Wektor w);
    @Override
    Macierz suma(Macierz m);

    @Override
    Wektor iloczyn(Skalar s);
    @Override
    Tablica iloczyn(Wektor w);
    @Override
    Wektor iloczyn(Macierz m);

    @Override
    Wektor negacja();
}
