package main.tablice.Klasy;

// Klasa sprawdzająca czy argument funkcji jest null/pusty
public class Walidator {
    public static void czyPustyObiekt(Object obj, String nazwa) {
        if (obj == null) {
            throw new PustyObiekt(" Obiekt" + nazwa + " nie może być NULL");
        }
    }
}
