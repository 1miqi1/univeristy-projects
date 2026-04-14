package main.tablice.Interfejsy;


public interface Skalar extends Tablica {
    double daj();
    void ustaw(double wartość);

    @Override
    Skalar kopia();

    @Override
    Skalar wycinek(int... indeksy);

    @Override
    Skalar suma(Skalar s);
    @Override
    Wektor suma(Wektor w);
    @Override
    Macierz suma(Macierz m);

    @Override
    Skalar iloczyn(Skalar s);
    @Override
    Wektor iloczyn(Wektor w);
    @Override
    Macierz iloczyn(Macierz m);

    @Override
    Skalar negacja();
}
