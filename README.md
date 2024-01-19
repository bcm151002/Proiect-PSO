# Proiect-PSO
## Cum să folosești aplicația
### Cum să pornești serverul
Pentru a alege modul de funcționare al serverului, folosește argumentul “-t” dacă vrei să rulezi codul prin intermediul threadurilor sau “-p” dacă vrei să rulezi cu procese.

### Cum să folosești programul
* Deschide pagina “index.html” de pe adresa ip de localhost, cu portul 8080.
* Pe pagina html vei vedea un formular care îți cere câteva date. Completează-l cu atenție, pentru că aceste date vor fi folosite mai târziu.
* După ce ai completat formularul, ai două opțiuni:
  - Apasă pe butonul Button Script pentru a trimite datele din formular către pagina “cgi-bin/ceva.sh”, care va executa scriptul “ceva.sh”. Acest script va returna un set de echo-uri, care vor fi afișate în partea de jos a noii pagini.
  - Apasă pe butonul Button Pagina pentru a afișa datele din formular în partea de jos a paginii curente, folosind metoda POST.
* Pagina “index2.html” are un buton nou, care implementează metoda PUT:
  - Completează din nou formularul cu date diferite, apoi apasă pe buton. Acesta va actualiza datele din pagină și le va afișa.
* Dacă te întorci pe pagina principală, vei găsi o secțiune “username” unde poți introduce un nume de utilizator. Dacă apesi pe butonul Button Script2, se va executa scriptul “ceva2.sh” care va prelua numele de utilizator, folosind o variabilă de mediu, și îl va afișa pe pagina “index3.html”.
