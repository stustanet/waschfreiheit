	libsvn1 1.9.5-1+deb9u2
	linux-compiler-gcc-6-x86 4.16.16-2~bpo9+1
	linux-headers-4.16.0-0.bpo.2-amd64 4.16.16-2~bpo9+1
	linux-headers-4.16.0-0.bpo.2-common 4.16.16-2~bpo9+1
	linux-image-4.16.0-0.bpo.2-amd64 4.16.16-2~bpo9+1
	linux-kbuild-4.16 4.16.16-2~bpo9+1all:

PREFIX ?= /usr/local/bin
ETCPREFIX ?= /etc/waschfreiheit
SYSTEMDPREFIX ?= /etc/systemd/system
DEFAULTTTY ?= /dev/ttyUSB0

FILES = __init__.py\
    __main__.py\
    debuginterface.py\
    machinemanager.py\
    sensor.py\
    uplink.py\
    wasch.py\

SYSTEMDFILES = wascheninfreiheit.service
CONFIGFILES = nodes.json sensor_normal.json

MAIN = $(PREFIX)/waschfreiheit/__main__.py

install:
	python3 ./testsetup.py
	mkdir -p $(PREFIX)/waschfreiheit
	cp $(FILES) $(PREFIX)/waschfreiheit
	chmod +x $(PREFIX)/waschfreiheit/__main__.py
	sed -i -e "s/DEBUG=True #SEDMARK/DEBUG=False/g" "$(MAIN)"
	sed -i -e "s#ETCPREFIX#$(ETCPREFIX)#g" "$(MAIN)"
	sed -i -e "s#DEFAULTTTY#$(DEFAULTTTY)#g" "$(MAIN)"
	cp $(SYSTEMDFILES) $(SYSTEMDPREFIX)
	sed -i -e "s#PREFIX#$(PREFIX)/waschfreiheit#g" $(SYSTEMDPREFIX)/$(SYSTEMDFILES)

	mkdir -p $(ETCPREFIX)
	cp $(CONFIGFILES) $(ETCPREFIX)

	useradd -c "Wascheninfreiheit user" -M -s /sbin/nologin waschfreiheit

uninstall:
	rm -r $(PREFIX)/waschfreiheit
	rm -r $(SYSTEMDPREFIX)/$(SYSTEMDFILES)
	userdel waschfreiheit

etcuninstall: uninstall
	rm -r $(ETCPREFIX)
