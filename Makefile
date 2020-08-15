CFLAGS = -Wall -g

# ID-ul unui client
ID = "Gigel"

# Portul pe care asculta serverul
PORT = 1337 

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

all: server subscriber

# Compileaza server.cpp
server: server.cpp

# Compileaza subscriber.cpp
subscriber: subscriber.cpp

.PHONY: clean run_server run_subcriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza subscriber
run_subcriber:
	./subscriber ${ID} ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
