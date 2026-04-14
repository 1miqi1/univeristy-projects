package test.totolotek;

import main.totolotek.gracz.Gracz;
import main.totolotek.gracz.GraczMinimalista;
import main.totolotek.system.*;

import static org.junit.jupiter.api.Assertions.*;

import main.totolotek.wyjątki.BłądWyboruLosowania;
import main.totolotek.wyjątki.NieprawidłowyBlankiet;
import main.totolotek.wyjątki.NieprawidłowyChybiłTrafił;
import org.junit.jupiter.api.Test;

public class Testy {
    @Test
    void stopienWygranej() {
        Zakład wygrywajacy = new Zakład(new int[]{1, 2, 3, 4, 5, 6});

        // Testowane przypadki
        Zakład trafione6 = new Zakład(new int[]{1, 2, 3, 4, 5, 6}); // 6 trafień -> stopień 1
        Zakład trafione5 = new Zakład(new int[]{1, 2, 3, 4, 5, 10}); // 5 trafień -> stopień 2
        Zakład trafione4 = new Zakład(new int[]{1, 2, 3, 4, 11, 12}); // 4 trafienia -> stopień 3
        Zakład trafione3 = new Zakład(new int[]{1, 2, 3, 13, 14, 15}); // 3 trafienia -> stopień 4
        Zakład trafione2 = new Zakład(new int[]{1, 2, 20, 21, 22, 23}); // 2 trafienia -> stopień 0
        Zakład trafione0 = new Zakład(new int[]{10, 11, 12, 13, 14, 15}); // 0 trafień -> stopień 0

        assertEquals(1, wygrywajacy.stopieńWygranej(trafione6), "6 trafień powinno dać stopień 1.");
        assertEquals(2, wygrywajacy.stopieńWygranej(trafione5), "5 trafień powinno dać stopień 2.");
        assertEquals(3, wygrywajacy.stopieńWygranej(trafione4), "4 trafienia powinno dać stopień 3.");
        assertEquals(4, wygrywajacy.stopieńWygranej(trafione3), "3 trafienia powinno dać stopień 4.");
        assertEquals(0, wygrywajacy.stopieńWygranej(trafione2), "2 trafienia powinny dać stopień 0.");
        assertEquals(0, wygrywajacy.stopieńWygranej(trafione0), "0 trafień powinno dać stopień 0.");
    }

    @Test
    void TestGeneratoraKuponów() throws NieprawidłowyChybiłTrafił, NieprawidłowyBlankiet {
        ChybiłTrafił ch = new ChybiłTrafił(1,1);
        assertEquals(300, ch.cenaWygenerowaniaKuponu());

        int[][] zapis= new int[8][];
        boolean[] anuluj = new boolean[8];
        for (int i = 1; i <= 7; i++) {
            zapis[i] = new int[]{1};
            anuluj[i] = true;
        }
        anuluj[0] = false;
        zapis[0]= new int[]{1,2,3,4,5,6};
        Blankiet bk = new Blankiet(zapis,anuluj, new int[]{2,3,4});
        assertEquals(1200, bk.cenaWygenerowaniaKuponu());
    }

    @Test
    void testSystemu() throws BłądWyboruLosowania {
        CentralaTotolotka.getInstance().utwórzKolekturę();
        GraczMinimalista g  = new GraczMinimalista("A","B","C",300*5,1);
        g.działajKup();
        g.działajKup();
        g.działajKup();
        g.działajKup();
        g.działajKup();
        assertEquals(0,g.środki());
        g.działajKup();
        assertEquals(0,g.środki());
        assertEquals(5,g.liczbaKupionychKuponów());

        assertEquals(60*5,BudżetPaństwa.getInstance().dajPodatki());

        CentralaTotolotka.getInstance().przeprowadźLosowanie();
        long pula = 0;
        for(int i = 1; i <= 4; i++) {
            pula += CentralaTotolotka.getInstance().wygranaStopnia(1,i);
        }
        g.działajOdbierz();
        assertEquals(pula,g.środki());
    }
}
