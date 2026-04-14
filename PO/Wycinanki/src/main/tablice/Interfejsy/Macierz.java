package main.tablice.Interfejsy;

public interface Macierz extends Tablica {
    double daj(int a, int b);
    void ustaw(double wartość, int a, int b);
    Wektor kolumna(int indeks);
    Wektor wiersz(int indeks);

    @Override
    Macierz kopia();

    @Override
    Macierz wycinek(int... indeksy);

    @Override
    Macierz suma(Skalar s);

    @Override
    Macierz suma(Wektor w);

    @Override
    Macierz suma(Macierz m);

    @Override
    Macierz iloczyn(Skalar s);

    @Override
    Wektor iloczyn(Wektor w);

    @Override
    Macierz iloczyn(Macierz m);

    @Override
    Macierz negacja();
}
