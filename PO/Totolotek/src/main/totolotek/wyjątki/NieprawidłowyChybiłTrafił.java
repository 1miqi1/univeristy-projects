package main.totolotek.wyjątki;

public class NieprawidłowyChybiłTrafił extends Exception {
    public NieprawidłowyChybiłTrafił(String message) {
        super("Źle wypełniony ChybiłTrafił: " + message);
    }
}
