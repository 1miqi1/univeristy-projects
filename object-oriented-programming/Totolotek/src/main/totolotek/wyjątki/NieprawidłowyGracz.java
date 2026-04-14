package main.totolotek.wyjątki;

public class NieprawidłowyGracz extends RuntimeException {
    public NieprawidłowyGracz(String message) {
        super(message);
    }
}
