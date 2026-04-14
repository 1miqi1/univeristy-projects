package main.totolotek.pomocniczeklasy;

import java.util.*;
import main.totolotek.gracz.*;
import main.totolotek.system.*;

public class Losowe {
    public static long losowyNumer(long dlugosc) {
        Random random = new Random();
        return 1 + random.nextLong(dlugosc);
    }

    public static String losowyNapis(int length) {
        String letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        Random rand = new Random();
        StringBuilder sb = new StringBuilder(length);

        for (int i = 0; i < length; i++) {
            int index = rand.nextInt(letters.length());
            sb.append(letters.charAt(index));
        }

        return sb.toString();
    }

    public static Blankiet prawidłowyLosowyBlankiet(){
        try {
            Random random = new Random();

            int liczbaZakładów = Stałe.MAXLICZBAZAKŁADÓW;
            int[][] liczby = new int[liczbaZakładów][];
            boolean[] anuluj = new boolean[liczbaZakładów];

            for (int i = 0; i < liczbaZakładów; i++) {
                // Losuj unikalne liczby w zakładzie
                Set<Integer> zestaw = new TreeSet<>();
                while (zestaw.size() < Stałe.DŁUGOŚĆZAKŁADU) {
                    int liczba = 1 + random.nextInt(Stałe.LICZBANUMERÓW);
                    zestaw.add(liczba);
                }

                int[] zakład = zestaw.stream().mapToInt(Integer::intValue).toArray();
                liczby[i] = zakład;

                anuluj[i] = random.nextBoolean();
            }

            int[] liczbaLosowań = new int[]{1+ random.nextInt(Stałe.MAXLICZBALOSOWAŃ)};
            return new Blankiet(liczby, anuluj, liczbaLosowań);
        }catch (Exception e) {
            System.out.println("Błąd systemu przy tworzeniu losowego blankietu" + e.getMessage());
            System.exit(0);
        }
        return null;
    }

    public static int[] losowaTablica(int k, int dlugosc) {
        if (k <= 0 || dlugosc <= 0) {
            throw new IllegalArgumentException("k i dlugosc muszą być większe od 0");
        }
        if (dlugosc > k) {
            throw new IllegalArgumentException("Nie można wygenerować " + dlugosc + " różnych liczb z zakresu 1-" + k);
        }

        Random rand = new Random();

        List<Integer> liczby = new ArrayList<>();
        for (int i = 1; i <= k; i++) {
            liczby.add(i);
        }

        Collections.shuffle(liczby, rand);

        int[] tablica = new int[dlugosc];
        for (int i = 0; i < dlugosc; i++) {
            tablica[i] = liczby.get(i);
        }

        return tablica;
    }

    public static GraczLosowy losowyPoprawnyGraczLosowy(){
        String imie = losowyNapis(5);
        String nazwisko = losowyNapis(5);
        String pesel = losowyNapis(5);
        return new GraczLosowy(imie,nazwisko,pesel);
    }

    public static GraczMinimalista losowyPoprawnyGraczMinimalista(){
        int k = CentralaTotolotka.getInstance().liczbaKolektur();
        int kolektura = Math.toIntExact(losowyNumer(k));
        String imie = losowyNapis(5);
        String nazwisko = losowyNapis(5);
        String pesel = losowyNapis(5);
        long środki = losowyNumer((long)2e6);
        return new GraczMinimalista(imie, nazwisko, pesel, środki ,kolektura);
    }

    public static GraczStałoliczbowy losowyPoprawnyGraczStałoliczbowy(){
        try{
            int k = CentralaTotolotka.getInstance().liczbaKolektur();
            String imie = losowyNapis(5);
            String nazwisko = losowyNapis(5);
            String pesel = losowyNapis(5);
            long środki = losowyNumer((long)2e6);
            int[] liczby = losowaTablica(49,6);
            return new GraczStałoliczbowy(imie, nazwisko, pesel, środki, losowaTablica(k,k/4), liczby);
        } catch (Exception e) {
            System.out.println("Błąd systemu " + e.getMessage());
            System.exit(0);
        }
        return null;
    }

    public static GraczStałoblankietowy losowyPoprawnyGraczStałoblankietowy() {
        try {
            int k = CentralaTotolotka.getInstance().liczbaKolektur();
            String imie = losowyNapis(5);
            String nazwisko = losowyNapis(5);
            String pesel = losowyNapis(5);
            long środki = losowyNumer((long) 2e6);
            int coIle = Math.toIntExact(1 + losowyNumer(5));
            return new GraczStałoblankietowy(imie, nazwisko, pesel, środki, losowaTablica(k, k / 4), prawidłowyLosowyBlankiet(), coIle);
        } catch (Exception e) {
            System.out.println("Błąd systemu przy tworzeniu GraczaSB " + e.getMessage());
            System.exit(0);
        }
        return null;
    }
}

