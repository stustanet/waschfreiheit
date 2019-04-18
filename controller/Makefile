PREFIX ?= /usr/local/bin
ETCPREFIX ?= /etc/waschfreiheit
SYSTEMDPREFIX ?= /etc/systemd/system

FILES = basenode.py\
    debuginterface.py\
    main.py\
    master.py\
    waschnode.py\
    configuration.py\
    exceptions.py\
    manhattannode.py\
    message.py\
    uplink.py


SYSTEMDFILES = waschcontroller.service
CONFIGDIR = conf

MAIN = $(PREFIX)/waschfreiheit/__main__.py

install:
	mkdir -p $(PREFIX)/waschfreiheit
	cp $(FILES) $(PREFIX)/waschfreiheit
	chmod +x $(PREFIX)/waschfreiheit/main.py
	cp $(SYSTEMDFILES) $(SYSTEMDPREFIX)
	sed -i -e "s#PREFIX#$(PREFIX)/waschfreiheit#g" $(SYSTEMDPREFIX)/$(SYSTEMDFILES)

	mkdir -p $(ETCPREFIX)
	cp -r $(CONFIGDIR)/* $(ETCPREFIX)

	useradd -c "Wascheninfreiheit user" -M -s /sbin/nologin waschfreiheit || true

	mkdir -p /var/log/wasch/
	chown waschfreiheit:waschfreiheit /var/log/wasch

uninstall:
	rm -r $(PREFIX)/waschfreiheit
	rm -r $(SYSTEMDPREFIX)/$(SYSTEMDFILES)
	userdel waschfreiheit

etcuninstall: uninstall
	rm -r $(ETCPREFIX)
