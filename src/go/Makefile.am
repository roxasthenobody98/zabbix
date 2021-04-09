## Process this file with automake to produce Makefile.in

EXTRA_DIST = .

BUILD_TIME=`date +%H:%M:%S`
BUILD_DATE=`date +"%b %_d %Y"`
GOOS=`go env GOOS`
GOARCH=`go env GOARCH`
PKG=zabbix.com/pkg/version

GOLDFLAGS = -X ${PKG}.titleMessage=zabbix_agent2
GOLDFLAGS += -X '${PKG}.compileDate=${BUILD_DATE}'
GOLDFLAGS += -X ${PKG}.compileTime=${BUILD_TIME}
GOLDFLAGS += -X ${PKG}.compileOs=${GOOS}
GOLDFLAGS += -X ${PKG}.compileArch=${GOARCH}
# GOLDFLAGS += -X ${PKG}.compileMode=daemon
GOLDFLAGS += -X main.confDefault=${AGENT2_CONFIG_FILE}

sbin_PROGRAMS = bin/zabbix_agent2$(EXEEXT)
noinst_PROGRAMS = bin/mock_server$(EXEEXT)

bin/zabbix_agent2$(EXEEXT): $(ZBX_GO_SOURCES)
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go build -ldflags="${GOLDFLAGS}" \
		-o bin ./cmd/zabbix_agent2

bin/mock_server$(EXEEXT): $(ZBX_GO_SOURCES)
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go build -ldflags="${GOLDFLAGS}" \
		-o bin ./cmd/mock_server

clean-local:
	go clean ./...

check:
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go test ./...

install-data-hook:
	$(MKDIR_P) "$(DESTDIR)$(AGENT2_CONFIG_PLUGINS_PATH)"
	test -f "$(DESTDIR)$(AGENT2_CONFIG_FILE)" || cp "conf/zabbix_agent2.conf" "$(DESTDIR)$(AGENT2_CONFIG_FILE)"
	for cfg in conf/zabbix_agent2.d/plugins.d/*.conf; do \
		test -f "$(DESTDIR)$(AGENT2_CONFIG_PLUGINS_PATH)/"`basename $$cfg` || \
		cp "$$cfg" "$(DESTDIR)$(AGENT2_CONFIG_PLUGINS_PATH)/"`basename $$cfg`; \
	done
