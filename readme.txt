Serban Sorohan - 321CC 



Pentru rezolvare am plecat de la scheletul din laborator, implementare
facand-o in limbajul c++. De asemenea, fisierul Makefile e cel din laborator
usor modificat.
Voi explica detaliile de implementare pentru fiecare sectiune in parte:




1. helpers.h
	In header am declarat mai multe macro-uri ajutatoare, pentru diferite
	dimensiuni ale bufferelor si pentru diferite mesaje.
	De asemenea, am declarat mai multe structuri:

		- structura "Packet": reprezinta structura pe care il are un pachet
		primit de la un client UDP. Contine campurile: "topic", "tip_date" si
		"continut" care au aceeasi reprezentare ca in enunt.

		- structura "Topic_packet": reprezinta structura care este trimisa
		de server catre clientii TCP. Contine campurile: "payload", "scr_addr"
		si "addrlen". "payload" reprezinta o structura "Packet" ce contine
		mesajul propriu-zis, "src_addr" contine adresa si portul clientului UDP
		care a trimis mesajul initial, iar campul "addrlen" reprezinta 
		dimensiunea campului "src_addr".

		- structura "Integer": reprezinta tipul payload-ului "numar intreg fara
		semn" din enunt. Contine campurile: "sign", "number". "sign" reprezinta
		un octet de semn, iar "number" reprezinta numarul intreg.

		- structura "Real_positive": reprezinta tipul payload-ului "numar real
		pozitiv cu 2 zecimale". Contine campul: "real" ce reprezinta modulul
		numarului inmultit cu 100.

		- structura "Real": reprezinta tipul payload-ului "numar real".
		Contine campurile: "sign", "number", "power". "sign" reprezinta octetul
		de semn, "number" modului numarului obtinut prin alipirea partii intregi
		de partea zecimala, iar "power" reprezinta modulul puterii negative a 
		lui 10 cu care trebuie inmultit modulul pentru a obtine numarul
		original.

	La structuri am adaugat si "__attribute__((packed))" din cauza problemelor
	de padding care apar.




2. Subscriber (subscriber.cpp)
	Clientul trimite ID-ul aferent catre server imediat dupa se s-a stabilit
	conexiunea. 
	Multiplexarea se face intre STDIN si socket-ul pentru
	conexiune la server.

	Pentru STDIN:
		- Se accepta doar comanda "exit" de la tastatura, orice alt input este
		trimis catre server, urmand sa se primeasca difererite mesaje in 
		functie de comanda.

		- La comanda "exit" se inchide socket-ul catre server si se termina
		functionarea programului

	Pentru conexiunea la server:
		- Daca se primeste "exit" de la server, inseamna ca serverul a fost
		inchis, si prin urmare trebuie sa se inchida si clientul
		- Daca se primeste "closed" de la server, inseamna ca ID-ul este
		deja conectat de pe un alt socket. In acest caz, se inchide programul.
		- Daca se primeste "invalid" inseamna ca a fost trimisa o comanda
		invalida catre server. Se afiseaza un mesaj corespunzator
		- Daca se primeste "subscribed x", unde x este numele unui topic,
		inseamna ca abonarea la topic (x) s-a realizat cu succes.
		- Daca se primeste "unsubscribed x", unde x este numele unui topic,
		inseamna ca dezabonarea la topic (x) s-a realizat cu succes.
		- Daca se primeste "A problem has occured!", inseamna ca nu a fost 
		respectat formatul parametrilor pentru comenzile "subscribe" si
		"unsubscribe"

		- Daca nu se primeste nici un mesaj din cele mentionate, inseamna
		ca se primeste un mesaj de tip "Topic_packet".
		- Pentru aceste mesaje, se afiseaza mai intai IP-ul si portul in
		functie de IPv4 sau IPv6, urmand sa se afiseze continutul pachetului
		conform specificatiilor din enunt.

		- Se asteapta primirea unui numar minim de octeti de la recv, lucru 
		ce se face succesiv pentru fiecare potential mesaj primit.




3. Server (server.cpp)
	In cadrul server-ului am folosit mai multe unordered_map-uri:
		- "TCPconnections": reprezinta un map in care se tin minte ID-uri
		conectate la un moment de timp. Structura este de forma:
		Key = ID_CLIENT; Value = 0 (deconectat)/sockfd (conectat)

		- "sockID": reprezinta opusul lui "TCPconnections", se tin minte
		care socket ii corespunde carui ID. Structura este de forma:
		Key = sockfd; Value = ID_CLIENT

		- "topic_subs": reprezinta un map in care se tin minte ce clienti
		sunt abonati la un topic. Structura este de forma:
		Key = topic; Value = set<int> (contine toti sockfd abonati)
		Structura set reprezinta un arbore rosu-negru care nu accepta
		duplicate, deci un client nu se poate abona la acelasi topic de 
		mai multe ori.

		- "clients_queue": reprezinta un map in care se pastreaza pentru
		fiecare client o coada cu mesajele ce trebuie trimise pentru
		abonamentele de tip S&F = 1. Structura este de forma:
		Key = ID_CLIENT; Value = Coada de Topic_packet

		- "store_and_forward": reprezinta un map in care se pastreaza
		pentru fiecare client, tipul S&F al fiecarui topic la care acesta
		este abonat. Structura este de forma:
		Key = ID_CLIENT; Value = unordered_map
		Key = topic; Value = 0/1

	Multiplexarea are loc intre STDIN, socket de ascultat (listen) conexiuni
	TCP, socket de primit mesaje UDP si socket de primit mesaje TCP

	Pentru socket de ascultat (listen) conexiuni TCP:
		- Se accepta conexiunea de la client, urmand sa se primeasca ID-ul
		acestuia. Daca ID-ul este logat in momentul de fata, se trimite un
		mesaj si se inchide socket-ul. Altfel, se tine minte la ce socket
		e conectat ID-ul si la ce ID e conectat socket-ul ("TCPconnections"
		si "sockID"). Se trimit catre client toate mesaje din coada, daca
		exista. In final, se afiseaza un mesaj corespunzator.

	Pentru socket-ul de primit mesaje UDP:
		- Se primeste mesajul UDP si se copiaza continut si adresala IP si
		portul de unde a venit mesajul intr-o structura de tip "Topic_packet".
		Se extrage topicul din pachet, iar pentru fiecare client abonat,
		se trimite pachet-ul format sau se pune in coada daca client-ul nu
		este conectat si are optiunea S&F = 1.

	Pentru STDIN:
		- Daca se primeste "exit", se inchid toate conexiunile cu clientii
		TCP si UDP, urmand sa se inchide serverul.
		- Orice alta comanda este ignorata.

	Pentru clientii TCP:
		- In cazul in care clientul s-a deconectat, "TCPconnections" este
		setat pe 0, iar "sockID" este setat pe "".
		- Se trateaza numai comenzile de tipul "subscribe"/"unsubcribe", 
		pentru orice alt tip de comanda se trimite inapoi un mesaj specificand
		"invalid command".

		- Pentru "subscribe"/"unsubcribe" sunt tratate cazurile in care 
		formatul parametrilor nu respecta formatul mentionat in enunt, in
		acest caz se trimite inapoi un mesaj de tipul "A problem has occured!".

		- Pentru "subscribe", se adauga in "topic_subs" ID-ul clientului 
		proaspat abonat si in "store_and_forward" tipul S&F al abonamentului.

		- Pentru "unsubscribe", se scoate din "topic_subs" ID-ul clientului
		pentru topicul la care se doreste dezabonarea.

		- Cazul in care se incearca "subscribe" la un topic la care este 
		deja abonat sau "unsubscribe" la un topic la care nu mai este
		abonat nu se trateaza intr-un mod special deoarece se foloseste o
		structura de date care nu permite mai multe intrari de acelasi tip
		(set). Daca se primeste "subscribe", doar se 
		modifica valoarea S&F, clientul deja aflandu-se in lista de 
		subscribers pentru topicul specificat. Daca se primeste 
		"unsubscribe", nu o sa se gaseasca in arbore valoare clientului
		si deci nu o sa aiba ce sa se stearga. Mesajele primite de client
		sunt aceleasi pentru ca si in cazul in care comenzile sunt 
		folosite in mod corect, deoarece m-am gandit ca pe client nu il
		intereseaza daca a fost abonat ulterior sau nu la topic, el 
		dorind sa stie doar daca a avut succes comanda.


	Complexitati:
		- Pentru introducerea/stergerea unui ID - O(1) (average)
		- Pentru introducerea/stergerea unui client la un topic  - O(lgn),
		n=nr de abonati la topic
		- Pentru trimiterea tutoror mesajelor din coada - O(m), m=nr mesaje
		- Pentru S&F - O(1) (average)



	Pentru testele facute de mine, aplicatia nu a ajuns niciodata intr-o 
stare de eroare, iar toata memoria a fost eliberata. Sper ca am acoperit toate
potentialele cazuri de provocat erori.
