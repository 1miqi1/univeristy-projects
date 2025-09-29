package main.totolotek.system;

import main.totolotek.pomocniczeklasy.PomoczniczeFunkcje;
import main.totolotek.pomocniczeklasy.Stałe;
import main.totolotek.wyjątki.ZłaKolektura;

import java.util.ArrayList;

public class Kupon {
    private final ArrayList<Zakład> zakłady;
    private final int pierwszeLosowanie;
    private final int liczbaLosowań;
    private final int numerKolektury;
    private final String id;

    Kupon (ArrayList<Zakład> zakłady,int pierwszeLosowanie, int liczbaLosowań, int numerKolektury, String id) {
        if(zakłady == null) {
            System.out.println("BŁĄD");
        }
        this.zakłady = zakłady;
        this.pierwszeLosowanie = pierwszeLosowanie;
        this.liczbaLosowań = liczbaLosowań;
        this.numerKolektury = numerKolektury;
        this.id = id;
    }

    ArrayList<Zakład> zakłady(){
        return zakłady;
    }

    public int pierwszeLosowanie(){
        return pierwszeLosowanie;
    }

    public int ostatnieLosowanie(){
        return pierwszeLosowanie + liczbaLosowań - 1;
    }

    public int numerKolektury() {
        return numerKolektury;
    }

    public Kolektura gdzieWydany(){
        try{
            return CentralaTotolotka.getInstance().dajKolekturę(numerKolektury());
        }catch (ZłaKolektura e){
            System.out.println(e.getMessage());
            System.exit(0);
        }
        return null;
    }

    public int liczbaLosowań(){
        return liczbaLosowań;
    }

    public String id(){
        return id;
    }

    public long podatek(){
        return zakłady.size() * liczbaLosowań * Stałe.PODATEKODZAKŁDAU;
    }

    public long cena(){
        return zakłady.size() * liczbaLosowań * Stałe.CENAZAKŁADU;
    }

    @Override
    public int hashCode() {
        String[] części = id.split("-");
        return Integer.parseInt(części[1]);

    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();

        sb.append("KUPON NR ").append(id).append("\n");

        int i = 1;
        for (Zakład zakład : zakłady) {
            sb.append(String.format("%d: %s\n", i++, zakład));
        }

        sb.append(String.format("LICZBA LOSOWAŃ: %d\n", liczbaLosowań));
        sb.append("NUMERY LOSOWAŃ:\n");
        for (int j = 0; j < liczbaLosowań; j++) {
            sb.append(String.format("%d ", pierwszeLosowanie + j));
        }
        sb.append("\n");

        sb.append("CENA: ").append(PomoczniczeFunkcje.złote(this.cena()));

        return sb.toString();
    }
}
