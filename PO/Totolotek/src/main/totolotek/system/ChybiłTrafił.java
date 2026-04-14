package main.totolotek.system;

import main.totolotek.pomocniczeklasy.Stałe;
import main.totolotek.wyjątki.NieprawidłowyChybiłTrafił;

import java.util.ArrayList;

public class ChybiłTrafił extends GeneratorKuponów{
    private int liczbaZakładów;
    private int liczbaLosowań;

    public ChybiłTrafił(int liczbaLosowań, int liczbaZakładów) throws NieprawidłowyChybiłTrafił{
        if(liczbaZakładów <= 0 || liczbaZakładów > Stałe.MAXLICZBAZAKŁADÓW ){
            throw new NieprawidłowyChybiłTrafił("niepoprawna liczba zakładów");
        }
        if(liczbaLosowań <= 0 || liczbaLosowań > Stałe.MAXLICZBALOSOWAŃ){
            throw new NieprawidłowyChybiłTrafił("niepoprawan liczba losowań");
        }
        this.liczbaLosowań = liczbaLosowań;
        this.liczbaZakładów = liczbaZakładów;
    }

    Kupon wygenerujKupon(int numerLosowania, int numerKolektury, String identyfikator) {
        ArrayList<Zakład> zakłady = new ArrayList<>();
        for(int i = 0; i < liczbaZakładów; i++){
            zakłady.add(new Zakład());
        }
        return new Kupon(zakłady, numerLosowania, liczbaLosowań, numerKolektury, identyfikator);
    }

    @Override
    public String toString(){
        StringBuilder sb = new StringBuilder();
        sb.append("ChybiłTrafił:\n");
        sb.append("Liczba zakładów: " + liczbaZakładów + ", Liczba losowań: " + liczbaLosowań);
        return sb.toString();
    }
}
