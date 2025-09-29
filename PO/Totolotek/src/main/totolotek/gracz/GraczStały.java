package main.totolotek.gracz;

import main.totolotek.system.*;
import main.totolotek.wyjątki.NieprawidłowyGracz;
import main.totolotek.wyjątki.OszustwoGracza;
import main.totolotek.wyjątki.ZłaKolektura;

public abstract class GraczStały extends Gracz {
    private int akkKolektura;
    private int[] ulubioneKolektury;
    protected GeneratorKuponów ulubionyGenerator;

    // CO GDY ZLE KOLEKTURY
    public GraczStały(String imię, String nazwisko, String pesel, long środki,  int[] ulubioneKolektury) throws NieprawidłowyGracz {
        super(imię, nazwisko, pesel, środki);
        akkKolektura = 0;
        this.ulubioneKolektury = ulubioneKolektury;
        for(int i = 0; i < ulubioneKolektury.length; i++) {
            if(ulubioneKolektury[i] < 1 || ulubioneKolektury[i] > CentralaTotolotka.getInstance().liczbaKolektur()){
                throw new NieprawidłowyGracz("Nieprawidłowy zbiór ulubionych kolektur");
            }
        }
    }

    public void działajKup(){
        try {
            if(this.czyMożna(ulubionyGenerator)){
                this.kupKupon(ulubionyGenerator,ulubioneKolektury[akkKolektura]);
                akkKolektura++;
                akkKolektura %= ulubioneKolektury.length;
            }
        }catch (ZłaKolektura e){
            System.out.println("Błąd systemu przy działajKup" + e.getMessage());
            System.exit(0);
        }
    }
}
