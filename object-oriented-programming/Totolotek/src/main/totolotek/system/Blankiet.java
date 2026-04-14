package main.totolotek.system;

import main.totolotek.pomocniczeklasy.Stałe;
import main.totolotek.wyjątki.NieprawidłowyBlankiet;

import java.util.*;
import java.util.ArrayList;

public class Blankiet extends GeneratorKuponów {
    private final int[][] liczby;
    private final boolean[] anuluj;
    private final int[] liczbaLosowań;

    public Blankiet(int[][] liczby, boolean[] anuluj, int[] liczbaLosowań) throws NieprawidłowyBlankiet{
        if(liczby.length !=  Stałe.MAXLICZBAZAKŁADÓW) {
            throw new NieprawidłowyBlankiet("nieprawidłowa liczba zakładów");
        }
        if(anuluj.length != Stałe.MAXLICZBAZAKŁADÓW) {
            throw new NieprawidłowyBlankiet("nieprawidłowa liczba anuluj");
        }
        for(int i : liczbaLosowań){
            if( i > Stałe.MAXLICZBALOSOWAŃ || i <= 0){
                throw new NieprawidłowyBlankiet("nieprawidłowa liczba losowań");
            }
        }
        for(int i = 0 ; i < liczby.length ; i++){
            for(int j = 0; i < liczby[i].length ; i++ ){
                if(liczby[i][j] <=0 || liczby[i][j] > Stałe.LICZBANUMERÓW){
                    throw new NieprawidłowyBlankiet("nieprawidłowy numer");
                }
            }
        }
        this.liczby = liczby;
        this.anuluj = anuluj;
        this.liczbaLosowań = liczbaLosowań;
    }

    Kupon wygenerujKupon(int numerLosowania, int numerKolektury, String identyfikator) {
        ArrayList<Zakład> zakłady = new ArrayList<>();
        for(int i = 0; i < Stałe.MAXLICZBAZAKŁADÓW; i++){
            Set<Integer> unikalne = new HashSet<>();
            for (int j = 0; j < liczby[i].length ; j++) {
                unikalne.add(liczby[i][j]);
            }
            if(unikalne.size() == Stałe.DŁUGOŚĆZAKŁADU || !anuluj[i]){
                zakłady.add(new Zakład(liczby[i]));
            }
        }
        int n = 1;
        for(int i : liczbaLosowań){
            n = Math.max(n,i);
        }
        return new Kupon(zakłady,numerLosowania, n, numerKolektury, identyfikator);
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Blankiet:\n");
        for (int z = 0; z < liczby.length; z++) {
            sb.append(z + 1).append("\n");
            boolean[] zaznaczone = new boolean[Stałe.LICZBANUMERÓW + 1];
            for (int n : liczby[z]) {
                if (n >= 1 && n <= Stałe.LICZBANUMERÓW) {
                    zaznaczone[n] = true;
                }
            }

            for (int wiersz = 0; wiersz < 5; wiersz++) {
                for (int kol = 1; kol <= 10; kol++) {
                    int num = wiersz * 10 + kol;
                    if (num > Stałe.LICZBANUMERÓW) break;
                    if (!zaznaczone[num]) {
                        sb.append(String.format("[ %2d ] ", num));
                    } else {
                        sb.append("[ -- ] ");
                    }
                }
                sb.append("\n");
            }

            if (anuluj[z]) {
                sb.append("[ -- ] anuluj\n");
            } else {
                sb.append("[    ] anuluj\n");
            }
        }

        sb.append("Liczba losowań: ");
        for (int i = 1; i <= Stałe.MAXLICZBALOSOWAŃ; i++) {
            for (int k : liczbaLosowań) {
                if (i == k) {
                    sb.append("[ -- ] ");
                } else {
                    sb.append(String.format("[ %2d ] ", i));
                }
            }
        }
        sb.append("\n");
        return sb.toString();
    }
}
