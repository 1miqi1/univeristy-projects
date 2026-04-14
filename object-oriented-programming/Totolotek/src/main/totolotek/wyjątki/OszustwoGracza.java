package main.totolotek.wyjątki;

public class OszustwoGracza extends Exception {
    public OszustwoGracza(String message) {
        super("Gracz przyłapany na oszustwie: " + message);
    }
}
