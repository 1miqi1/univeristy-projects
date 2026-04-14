package test.tablice.Tests;


import main.tablice.Interfejsy.Macierz;
import main.tablice.Interfejsy.Skalar;
import main.tablice.Interfejsy.Wektor;
import org.junit.jupiter.api.Test;
import main.tablice.Klasy.*;

import static org.junit.jupiter.api.Assertions.assertEquals;

public class WłasneTesty {

    @Test
    void testToString(){
        Skalar skalar = new SkalarDane(3.5);
        String result1 = skalar.toString();
        String expected1 = "3.5";
        assertEquals(expected1, result1);

        Wektor wektor = new WektorDane(new double[]{1.0, 2.5, 3.3}, true); // true = Pozioma
        String result2 = wektor.toString();
        String expected2 = "1.0 2.5 3.3 \nOrientacja: Pozioma";
        assertEquals(expected2, result2);

        Macierz macierz = new MacierzDane(new double[][]{
                {1.0, 2.0, 3.0},
                {4.0, 5.0, 6.0},
        });
        String result3 = macierz.toString();
        String expected3 = "1.0 2.0 3.0 \n4.0 5.0 6.0 ";
        assertEquals(expected3, result3);
    }

    @Test
    void testTranspozycji(){
        Skalar skalar1 = new SkalarDane(3.5);
        skalar1.transponuj();
        assertEquals(new SkalarDane(3.5),skalar1);

        Wektor wektor1 = new WektorDane(new double[]{1.0, 2.0, 3.0}, true);
        wektor1.transponuj();
        assertEquals(new WektorDane(new double[]{1.0, 2.0, 3.0}, false),wektor1);

        Macierz macierz1 = new MacierzDane(new double[][]{
                {1.0, 2.0},
                {4.0, 5.0},
                {7.0, 8.0}
        });
        Macierz oczekiwany = new MacierzDane(new double[][]{
                {1.0, 4.0, 7.0},
                {2.0, 5.0, 8.0},
        });
        macierz1.transponuj();
        assertEquals(oczekiwany,macierz1);
    }

    @Test
    void testWycinkówIKopiiSkalarowych() {
        Skalar skalar1 = new SkalarDane(3.5);
        Skalar skalar2 = skalar1.wycinek();
        Skalar skalar3 = skalar1.kopia();
        skalar1.ustaw(5);
        assertEquals(new SkalarDane(5), skalar1);
        assertEquals(new SkalarDane(5), skalar2);
        assertEquals(new SkalarDane(3.5), skalar3);

        skalar2.ustaw(7);
        assertEquals(new SkalarDane(7), skalar1);
        assertEquals(new SkalarDane(3.5), skalar3);
    }

    @Test
    void testWycinkówIKopiiWektorowych() throws ZłyIndeks {
        Wektor wektor1 = new WektorDane(new double[]{1.0, 2.0, 3.0}, true);
        Wektor wektor2 = wektor1.wycinek(1,2);
        Wektor wektor3 = wektor1.kopia();

        wektor2.ustaw(5.0,0);
        assertEquals(new WektorDane(new double[]{1.0, 5.0, 3.0}, true), wektor1);

        wektor1.ustaw(3.0,0);
        assertEquals(new WektorDane(new double[]{5.0, 3.0}, true), wektor2);

        wektor1.ustaw(4.0,1);
        assertEquals(new WektorDane(new double[]{4.0, 3.0}, true), wektor2);

        wektor1.transponuj();
        assertEquals(new WektorDane(new double[]{4.0, 3.0}, true), wektor2);

        assertEquals(new WektorDane(new double[]{1.0, 2.0, 3.0}, true), wektor3);
    }

    @Test
    void testWycinkówIKopiiMacierzowych(){
        Macierz macierz1 = new MacierzDane(new double[][]{
                {1.0, 2.0},
                {3.0, 4.0},
                {5.0, 6.0}
        });
        Macierz macierz2 = macierz1.wycinek(0,2,0,0);
        assertEquals(macierz2, new MacierzDane(new double[][]{
                {1.0},
                {3.0},
                {5.0}
        }));
        Macierz macierz3 = macierz1.kopia();

        macierz2.przemnóż(new SkalarDane(100));
        assertEquals(new MacierzDane(new double[][]{
                {100.0, 2.0},
                {300.0, 4.0},
                {500.0, 6.0}
        }), macierz1);

        macierz1.transponuj();
        assertEquals(new MacierzDane(new double[][]{
                {100.0},
                {300.0},
                {500.0}
        }), macierz2);
        assertEquals(new MacierzDane(new double[][]{
                {100.0, 300.0 , 500.0},
                { 2.0, 4.0, 6.0},
        }), macierz1);

        macierz2.transponuj();
        assertEquals(new MacierzDane(new double[][]{
                {100.0, 300.0 , 500.0},
                { 2.0, 4.0, 6.0},
        }), macierz1);

        assertEquals(new MacierzDane(new double[][]{
                {1.0, 2.0},
                {3.0, 4.0},
                {5.0, 6.0}
        }), macierz3);
    }

    @Test
    void testKolumnWiersz(){
        Macierz macierz1 = new MacierzDane(new double[][]{
                {1.0, 2.0},
                {3.0, 4.0},
                {5.0, 6.0}
        });

        Wektor wiersz0 = macierz1.wiersz(0);
        Wektor wiersz1 = macierz1.wiersz(1);
        Wektor wiersz2 = macierz1.wiersz(2);
        Wektor kolumna0 = macierz1.kolumna(0);
        Wektor kolumna1 = macierz1.kolumna(1);

        assertEquals(wiersz0, new WektorDane(new double[]{1.0, 2.0}, true));
        assertEquals(wiersz1, new WektorDane(new double[]{3.0, 4.0}, true));
        assertEquals(wiersz2, new WektorDane(new double[]{5.0, 6.0}, true));
        assertEquals(kolumna0, new WektorDane(new double[]{1.0, 3.0, 5.0}, false));
        assertEquals(kolumna0, new WektorDane(new double[]{1.0, 3.0, 5.0}, false));

        wiersz0.dodaj(new SkalarDane(5));

        assertEquals(macierz1, new MacierzDane(new double[][]{
                {6.0, 7.0},
                {3.0, 4.0},
                {5.0, 6.0}
        }));
    }

    @Test
    void testArytmetykiModyfikującejSkalarSkalar(){
        Skalar skalar1 = new SkalarDane(5.0);
        Skalar skalar2 = new SkalarDane(3.0);
        skalar1.przemnóż(skalar2);
        skalar1.dodaj(skalar2);
        assertEquals(new SkalarDane(18.0), skalar1);
    }

    @Test
    void testArytmetykiModyfikującejSkalarWektor(){
        for(boolean b: new boolean[]{true, false}) {
            Skalar skalar = new SkalarDane(3.0);
            Wektor wektor1 = new WektorDane(new double[]{1.0, 2.5}, b);
            wektor1.dodaj(skalar);
            assertEquals(new WektorDane(new double[]{4.0, 5.5}, b), wektor1);

            Wektor wektor2 = new WektorDane(new double[]{1.5, 2.25}, b);
            wektor2.przemnóż(new SkalarDane(4.0));
            assertEquals(new WektorDane(new double[]{6.0, 9.0}, b),
                    wektor2);
        }
    }

    @Test
    void testArytmetykiModyfikującejSkalarMacierz() {
        Skalar skalar = new SkalarDane(3.0);
        Skalar skalar2 = new SkalarDane(-3.0);

        Macierz macierz = new MacierzDane(new double[][]{
                {1.25, 3.0, -12.0},
                {-51.0, 8.0, 3.5}
        });
        macierz.przemnóż(skalar2);
        macierz.dodaj(skalar);

        Macierz oczekiwanyWynikDodawania = new MacierzDane(new double[][]{
                {-0.75, -6.0, 39.0},
                {156.0, -21.0, -7.5}
        });

        assertEquals(oczekiwanyWynikDodawania, macierz);
    }

    @Test
    void testArytmetykiModyfikującejWektorMacierzDodaj(){
        Wektor wektor = new WektorDane(new double[]{1.0, 2.0, 3.0}, true);
        Wektor wektor2 = new WektorDane(new double[]{10, 20}, false);
        Macierz macierz = new MacierzDane(new double[][]{
                {0, 0, 0},
                {0, 0, 0}
        });
        macierz.dodaj(wektor);
        macierz.dodaj(wektor2);
        Macierz oczekiwana = new MacierzDane(new double[][]{
                {11, 12, 13},
                {21, 22, 23}
        });
        assertEquals(oczekiwana, macierz);
    }

    @Test
    void testArytmetykiModyfikującejMacierzMacierzDodaj(){
        Macierz macierz1 = new MacierzDane(new double[][]{
                {1, 2, 3},
                {1, 2, 3}
        });
        Macierz macierz2 = new MacierzDane(new double[][]{
                {10, 20, 30},
                {10, 10, 10}
        });
        Macierz oczekiwana = new MacierzDane(new double[][]{
                {11, 22, 33},
                {11, 12, 13}
        });
        macierz1.dodaj(macierz2);
        assertEquals(oczekiwana,macierz1);
    }

    @Test
    void testArytmetykiModyfikującejMacierzMacierzPrzemnóż(){
        Macierz macierz1 = new MacierzDane(new double[][]{
                {100, 200, 300},
                {1, 2, 3}
        });
        Macierz macierz2 = new MacierzDane(new double[][]{
                {1, 2, 3},
                {4, 5, 6},
                {7, 8, 9}
        });
        macierz1.iloczyn(macierz2);
        Macierz oczekiwany = new MacierzDane(new double[][]{
                {3000, 3600, 4200},
                {30, 36, 42}
        });
    }

    @Test
    void testZaneguj() {
        Skalar skalar = new SkalarDane(17.0);
        skalar.zaneguj();
        assertEquals(new SkalarDane(-17.0), skalar);

        Wektor wektor = new WektorDane(new double[]{10.0, -45.0, 0.0, 29.0, -3.0}, true);
        wektor.zaneguj();
        assertEquals(new WektorDane(new double[]{-10.0, 45.0, 0.0, -29.0, 3.0}, true),
                wektor);

        Macierz macierz = new MacierzDane(new double[][]{
                {0.0, 0.5, -1.25},
                {11.0, -71.0, -33.5},
                {-2.0, -1.75, -99.0}
        });
        Macierz oczekiwany = new MacierzDane(new double[][]{
                {0.0, -0.5, 1.25},
                {-11.0, 71.0, 33.5},
                {2.0, 1.75, 99.0}
        });
        macierz.zaneguj();
        assertEquals(oczekiwany, macierz);
    }

    @Test
    void testPrzypisaniaSkalarów() {
        Skalar skalar1 = new SkalarDane(1.0);
        skalar1.przypisz(new SkalarDane(0.5));
        assertEquals(new SkalarDane(0.5), skalar1);

        Wektor wektor1 = new WektorDane(new double[]{1.0, 2.0, 3.0}, true);
        wektor1.przypisz(new SkalarDane(0.5));
        assertEquals(new WektorDane(new double[]{0.5, 0.5, 0.5}, true), wektor1);

        Macierz macierz1 = new MacierzDane(new double[][]{
                {1.0, 2.0},
                {-3.0, -4.0},
                {5.0, -6.0}
        });
        macierz1.przypisz(new SkalarDane(0.5));
        assertEquals(new MacierzDane(new double[][]{
                {0.5, 0.5},
                {0.5, 0.5},
                {0.5, 0.5}
        }), macierz1);
    }
}

