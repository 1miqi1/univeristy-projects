package main.totolotek.wyjątki;

public class BłędnyZakład extends RuntimeException {
    public BłędnyZakład(String message) {
        super(message);
    }
}
