package main.totolotek.system;

import java.util.*;

import main.totolotek.pomocniczeklasy.Stałe;
import main.totolotek.wyjątki.BłędnyZakład;

public class Zakład {
    private final TreeSet<Integer> liczby;

    public Zakład(int[] t){
        liczby = new TreeSet<>();
        for (int i = 0; i < t.length; i++) {
            if(t[i] <= 0 || t[i] > Stałe.LICZBANUMERÓW){
                throw new BłędnyZakład("Błędne liczby przy tworzeniu zakładu");
            }
            liczby.add(t[i]);
        }
        if(liczby.size() != 6){
            throw new BłędnyZakład("Błędne liczby przy tworzeniu zakładu");
        }
    }

    Zakład() {
        List<Integer> wszystkie = new ArrayList<>();
        for (int i = 1; i <= Stałe.LICZBANUMERÓW; i++) {
            wszystkie.add(i);
        }
        Collections.shuffle(wszystkie);
        liczby = new TreeSet<>(wszystkie.subList(0, Stałe.DŁUGOŚĆZAKŁADU));
    }

    int[] liczby() {
        int[] r = new int[6];
        int j = 0;
        for(int i : liczby){
            r[j] = i;
            j++;
        }
        return r;
    }

    public int stopieńWygranej(Zakład z) {
        TreeSet<Integer> przecięcie = new TreeSet<>(liczby);
        przecięcie.retainAll(z.liczby);
        int stopień = 7 - przecięcie.size();
        return stopień <= 4 ? stopień : 0;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        for (Integer i : liczby) {
            sb.append(String.format("%2d ", i));
        }
        return sb.toString();
    }
}
