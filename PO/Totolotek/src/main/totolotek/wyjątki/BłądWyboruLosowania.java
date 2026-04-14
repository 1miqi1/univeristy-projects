package main.totolotek.wyjątki;

public class BłądWyboruLosowania extends Exception {
    public BłądWyboruLosowania(int numer) {
        super("Losowanie o numerze: " + numer + " się jeszcze nie odbyło");
    }
    public BłądWyboruLosowania(String message) {
        super("Losowanie się już odbyło " + message);
    }
}
