Linijka 2: jakie zdarzenia chcesz obserwowaćcev.events = EPOLLIN;Pole events to bitmaska mówiąca: "obudź mnie, gdy stanie się to lub to". Najczęstsze flagi:
EPOLLIN — "są dane do czytania" (lub w przypadku gniazda nasłuchującego: "jest klient do zaakceptowania")
EPOLLOUT — "można pisać bez blokowania" (rzadko używane przy małych wiadomościach)
EPOLLERR — wystąpił błąd na deskryptorze (zawsze obserwowany, nie trzeba ustawiać)
EPOLLHUP — druga strona zamknęła połączenie (zawsze obserwowany)
EPOLLET — tryb edge-triggered (omawialiśmy, zostań przy domyślnym LT)
