package main.totolotek.wyjątki;

public class NieprawidłowyBlankiet extends Exception {
    public NieprawidłowyBlankiet(String message) {
        super("Źle wypełniony Blankiet: " + message);
    }
}
