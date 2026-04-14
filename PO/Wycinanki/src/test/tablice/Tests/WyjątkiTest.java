package test.tablice.Tests;

import org.junit.jupiter.api.Test;
import main.tablice.Klasy.*;
import main.tablice.Interfejsy.*;
import static org.junit.jupiter.api.Assertions.assertThrows;

public class WyjątkiTest {

    @Test
    public void testWalidatorCzyPustyObiektDlaSkalarWektorMacierz() {
        SkalarDane skalar = new SkalarDane(3.0);
        WektorDane wektor = new WektorDane(new double[]{1.0, 2.0, 3.0}, true);
        MacierzDane macierz = new MacierzDane(new double[][]{{1.0, 2.0}, {3.0, 4.0}});

        // SkalarDane
        assertThrows(PustyObiekt.class, () -> skalar.daj((int[]) null));
        assertThrows(PustyObiekt.class, () -> skalar.ustaw(1.0, (int[]) null));
        assertThrows(PustyObiekt.class, () -> skalar.wycinek((int[]) null));
        assertThrows(PustyObiekt.class, () -> skalar.suma((Skalar) null));
        assertThrows(PustyObiekt.class, () -> skalar.suma((Wektor) null));
        assertThrows(PustyObiekt.class, () -> skalar.suma((Macierz) null));
        assertThrows(PustyObiekt.class, () -> skalar.dodaj((Skalar) null));
        assertThrows(PustyObiekt.class, () -> skalar.iloczyn((Skalar) null));
        assertThrows(PustyObiekt.class, () -> skalar.iloczyn((Wektor) null));
        assertThrows(PustyObiekt.class, () -> skalar.iloczyn((Macierz) null));
        assertThrows(PustyObiekt.class, () -> skalar.przemnóż((Skalar) null));
        assertThrows(PustyObiekt.class, () -> skalar.przypisz((Skalar) null));

        // WektorDane
        assertThrows(PustyObiekt.class, () -> new WektorDane((double[]) null, true));
        assertThrows(PustyObiekt.class, () -> wektor.daj((int[]) null));
        assertThrows(PustyObiekt.class, () -> wektor.ustaw(1.0, (int[]) null));
        assertThrows(PustyObiekt.class, () -> wektor.wycinek((int[]) null));
        assertThrows(PustyObiekt.class, () -> wektor.suma((Skalar) null));
        assertThrows(PustyObiekt.class, () -> wektor.suma((Wektor) null));
        assertThrows(PustyObiekt.class, () -> wektor.suma((Macierz) null));
        assertThrows(PustyObiekt.class, () -> wektor.dodaj((Skalar) null));
        assertThrows(PustyObiekt.class, () -> wektor.dodaj((Wektor) null));
        assertThrows(PustyObiekt.class, () -> wektor.iloczyn((Skalar) null));
        assertThrows(PustyObiekt.class, () -> wektor.iloczyn((Wektor) null));
        assertThrows(PustyObiekt.class, () -> wektor.iloczyn((Macierz) null));
        assertThrows(PustyObiekt.class, () -> wektor.przemnóż((Skalar) null));
        assertThrows(PustyObiekt.class, () -> wektor.przypisz((Skalar) null));
        assertThrows(PustyObiekt.class, () -> wektor.przypisz((Wektor) null));

        // MacierzDane
        assertThrows(PustyObiekt.class, () -> new MacierzDane((double[][]) null));
        assertThrows(PustyObiekt.class, () -> new MacierzDane(new double[][]{null, {1.0}}));
        assertThrows(PustyObiekt.class, () -> macierz.daj((int[]) null));
        assertThrows(PustyObiekt.class, () -> macierz.ustaw(5.0, (int[]) null));
        assertThrows(PustyObiekt.class, () -> macierz.suma((Skalar) null));
        assertThrows(PustyObiekt.class, () -> macierz.suma((Wektor) null));
        assertThrows(PustyObiekt.class, () -> macierz.suma((Macierz) null));
        assertThrows(PustyObiekt.class, () -> macierz.dodaj((Skalar) null));
        assertThrows(PustyObiekt.class, () -> macierz.dodaj((Wektor) null));
        assertThrows(PustyObiekt.class, () -> macierz.dodaj((Macierz) null));
        assertThrows(PustyObiekt.class, () -> macierz.iloczyn((Skalar) null));
        assertThrows(PustyObiekt.class, () -> macierz.iloczyn((Wektor) null));
        assertThrows(PustyObiekt.class, () -> macierz.iloczyn((Macierz) null));
        assertThrows(PustyObiekt.class, () -> macierz.przemnóż((Skalar) null));
        assertThrows(PustyObiekt.class, () -> macierz.przemnóż((Wektor) null));
        assertThrows(PustyObiekt.class, () -> macierz.przemnóż((Macierz) null));
        assertThrows(PustyObiekt.class, () -> macierz.przypisz((Skalar) null));
        assertThrows(PustyObiekt.class, () -> macierz.przypisz((Wektor) null));
        assertThrows(PustyObiekt.class, () -> macierz.przypisz((Macierz) null));
    }

    @Test
    void wyjątkiOperacjiWłasnychSkalaru() {
        SkalarDane skalar = new SkalarDane(3.0);
        assertThrows(BłędnaDługość.class, () -> skalar.daj(0));
        assertThrows(BłędnaDługość.class, () -> skalar.ustaw(5.0, 0));
        assertThrows(BłędnaDługość.class, () -> skalar.wycinek(0));
    }

    @Test
    void wyjątkiOperacjiWłasnychWektoru() {
        WektorDane wektor = new WektorDane(new double[]{1.0}, true);
        assertThrows(BłędnaDługość.class, () -> wektor.daj());
        assertThrows(BłędnaDługość.class, () -> wektor.daj(0, 1));
        assertThrows(ZłyIndeks.class, () -> wektor.daj(1));

        assertThrows(BłędnaDługość.class, () -> wektor.ustaw(0,1, 1));
        assertThrows(ZłyIndeks.class, () -> wektor.ustaw(3.0, -1));
        assertThrows(ZłyIndeks.class, () -> wektor.ustaw(3.0, 1));

        assertThrows(BłędnaDługość.class, () -> wektor.wycinek(0));
        assertThrows(BłędnaDługość.class, () -> wektor.wycinek(0, 1, 2));

        assertThrows(BłędneZakresy.class, () -> wektor.wycinek(1, 0));
        assertThrows(BłędneZakresy.class, () -> wektor.wycinek(-1, 1));
        assertThrows(BłędneZakresy.class, () -> wektor.wycinek(0, 2));
    }

    @Test
    void wyjątkiOperacjiWłasnychMacierzy() {
        Macierz macierz = new MacierzDane(new double[][]{
                {1}, {2}, {3},
                {4}, {5}, {6}
        });
        assertThrows(BłędnaDługość.class, () -> macierz.daj());
        assertThrows(BłędnaDługość.class, () -> macierz.daj(0, 1,3));
        assertThrows(BłędnaDługość.class, () -> macierz.daj(0));
        assertThrows(ZłyIndeks.class, () -> macierz.daj(0,5));
        assertThrows(ZłyIndeks.class, () -> macierz.daj(-1,0));

        assertThrows(BłędnaDługość.class, () -> macierz.ustaw(0));
        assertThrows(BłędnaDługość.class, () -> macierz.ustaw(0,0, 1,3));
        assertThrows(BłędnaDługość.class, () -> macierz.ustaw(0));
        assertThrows(ZłyIndeks.class, () -> macierz.ustaw(0,5,4));
        assertThrows(ZłyIndeks.class, () -> macierz.ustaw(0,-1,0));

        assertThrows(BłędnaDługość.class, () -> macierz.wycinek(0));
        assertThrows(BłędnaDługość.class, () -> macierz.wycinek(0, 1, 2));

        assertThrows(BłędneZakresy.class, () -> macierz.wycinek(1,0, 0,1));
        assertThrows(BłędneZakresy.class, () -> macierz.wycinek(-1,0, 1, 0));
        assertThrows(BłędneZakresy.class, () -> macierz.wycinek(0,20, 2,2));
    }

    @Test
    void wyjątkiOperacjiMacierzSkalar() {
        SkalarDane skalar = new SkalarDane(3.0);
        Macierz macierz = new MacierzDane(new double[][]{{1.0}});

        assertThrows(NiezgodnośćRozmiarów.class, () -> skalar.dodaj(macierz));
        assertThrows(NiezgodnośćRozmiarów.class, () -> skalar.przemnóż(macierz));
        assertThrows(NiezgodnośćRozmiarów.class, () -> skalar.przypisz(macierz));
    }

    @Test
    void wyjątkiOperacjiWektorSkalar() {
        SkalarDane skalar = new SkalarDane(3.0);
        Wektor wektor = new WektorDane(new double[]{1.0}, true);
        assertThrows(NiezgodnośćRozmiarów.class, () -> skalar.dodaj(wektor));
        assertThrows(NiezgodnośćRozmiarów.class, () -> skalar.przemnóż(wektor));
        assertThrows(NiezgodnośćRozmiarów.class, () -> skalar.przypisz(wektor));
    }

    @Test
    void wyjątkiOperacjiMacierzWektor() {
        Macierz macierz = new MacierzDane(new double[][]{
                {1}, {2}, {3},
                {4}, {5}, {6}
        });
        Wektor wektor = new WektorDane(new double[]{1,2,3,4}, true);

        assertThrows(NiezgodnośćRozmiarów.class, () -> wektor.suma(macierz));
        assertThrows(NiezgodnośćRozmiarów.class, () -> wektor.iloczyn(macierz));
        assertThrows(NiezgodnośćRozmiarów.class, () -> wektor.dodaj(macierz));
        assertThrows(NiezgodnośćRozmiarów.class, () -> wektor.przemnóż(macierz));
        assertThrows(NiezgodnośćRozmiarów.class, () -> wektor.przypisz(macierz));

        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.dodaj(wektor));
        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.suma(wektor));
        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.przypisz(wektor));
        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.iloczyn(wektor));
        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.przemnóż(wektor));
    }

    @Test
    void wyjątkiOperacjiMacierzMacierz() {
        Macierz macierz = new MacierzDane(new double[][]{
                {1, 2, 3,},
                {4, 5, 6}
        });
        Macierz macierz2 = new MacierzDane(new double[][]{
                {1, 2},
                {3, 4},
                {5, 5}
        });
        Macierz macierz3 = new MacierzDane(new double[][]{
                {1},
        });

        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.suma(macierz2));
        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.dodaj(macierz2));
        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.przemnóż(macierz2));
        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.przypisz(macierz2));
        assertThrows(NiezgodnośćRozmiarów.class, () -> macierz.iloczyn(macierz3));
    }
}
