PREFIX ?= /usr/local/bin
ETCPREFIX ?= /etc/waschstreamer
SYSTEMDPREFIX ?= /etc/systemd/system

FILES = waschstreamer.py

SYSTEMDFILES = waschstreamer.service
CONFIGFILES = config.conf

install:
	mkdir -p $(PREFIX)
	cp $(FILES) $(PREFIX)
	chmod +x $(PREFIX)/waschstreamer.py
	cp $(SYSTEMDFILES) $(SYSTEMDPREFIX)
	sed -i -e "s#PREFIX#$(PREFIX)#g" $(SYSTEMDPREFIX)/$(SYSTEMDFILES)

	mkdir -p $(ETCPREFIX)
	cp $(CONFIGFILES) $(ETCPREFIX)

	useradd -c "Wascheninfreiheit user" -M -s /sbin/nologin waschfreiheit || true
	usermod waschfreiheit -a -G gpio
	usermod waschfreiheit -a -G dialout

uninstall:
	rm -r $(PREFIX)/waschfreiheit
	rm -r $(SYSTEMDPREFIX)/$(SYSTEMDFILES)
	userdel waschfreiheit

etcuninstall: uninstall
	rm -r $(ETCPREFIX)
