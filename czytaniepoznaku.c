#include <stdio.h>
#include <stdlib.h>

int main() {
    // 1. Otwieramy plik w trybie tylko do odczytu ("r" - read)
    FILE *file = fopen("dane.txt", "r");
    
    // ZAWSZE sprawdzaj, czy plik w ogóle istnieje, 
    // inaczej program wywali Segmentation Fault przy próbie czytania!
    if (file == NULL) {
        perror("Błąd otwarcia pliku");
        return EXIT_FAILURE;
    }

    int c; // Zmienna int na znak + EOF

    // 2. Ta sama pętla - zamiast stdin podajemy nasz wskaźnik 'file'
    while ((c = fgetc(file)) != EOF) {
        // Przykład: zliczanie spacji w pliku
        if (c == ' ') {
            // Zrobiłeś coś ze znakiem, np. inkrementacja licznika
        }
        
        // Albo po prostu wypisujemy znak na ekran
        putchar(c); 
    }

    // 3. ZAWSZE zamykaj plik po zakończeniu pracy, 
    // żeby nie marnować deskryptorów w systemie operacyjnym!
    fclose(file);

    return EXIT_SUCCESS;
}