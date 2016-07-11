all: release

release:
	mkdir -p bin
	gcc codefacer.c -o bin/codefacer -Iinclude -lmysqlclient -Wall -O2 -D_GNU_SOURCE

clean:
	rm -rf bin

install: release
	cp bin/codefacer /usr/bin/codefacer
	cp codefacer.conf /etc/codefacer.conf

uninstall:
	rm /usr/bin/codefacer
	rm /etc/codefacer.conf
