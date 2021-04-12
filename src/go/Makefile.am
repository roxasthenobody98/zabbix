## Process this file with automake to produce Makefile.in

EXTRA_DIST = .

BUILD_TIME=`date +%H:%M:%S`
BUILD_DATE=`date +"%b %_d %Y"`
GOOS=`go env GOOS`
GOARCH=`go env GOARCH`
PKG=zabbix.com/pkg/version

GOLDFLAGS = -X '${PKG}.compileDate=${BUILD_DATE}'
GOLDFLAGS += -X ${PKG}.compileTime=${BUILD_TIME}
GOLDFLAGS += -X ${PKG}.compileOs=${GOOS}
GOLDFLAGS += -X ${PKG}.compileArch=${GOARCH}

AGENT_GOLDFLAGS = ${GOLDFLAGS}
AGENT_GOLDFLAGS += -X main.confDefault=${AGENT2_CONFIG_FILE}
AGENT_GOLDFLAGS += -X main.applicationName=zabbix_agent2

WEBSERVICE_GOLDFLAGS = ${GOLDFLAGS}
WEBSERVICE_GOLDFLAGS += -X main.applicationName=zabbix_web_service

dist_sysconf_DATA =
BIN_TARGETS =
CONFIGS =

if AGENT2
BIN_TARGETS +=  bin/zabbix_agent2$(EXEEXT)
dist_sysconf_DATA += conf/zabbix_agent2.conf
endif

if WEBSERVICE
BIN_TARGETS += bin/zabbix_web_service$(EXEEXT)
dist_sysconf_DATA += conf/zabbix_web_service.conf
endif

sbin_PROGRAMS = $(BIN_TARGETS)
noinst_PROGRAMS = bin/mock_server$(EXEEXT)

bin/zabbix_agent2$(EXEEXT): $(ZBX_GO_SOURCES)
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go build -ldflags="${AGENT_GOLDFLAGS}" \
			-o bin zabbix.com/cmd/zabbix_agent2

bin/zabbix_web_service$(EXEEXT): $(ZBX_GO_SOURCES)
	go build -ldflags="${WEBSERVICE_GOLDFLAGS}" -o bin zabbix.com/cmd/zabbix_web_service

bin/mock_server$(EXEEXT): $(ZBX_GO_SOURCES)
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go build -ldflags="${AGENT_GOLDFLAGS}" \
			-o bin zabbix.com/cmd/mock_server

clean-local:
	go clean ./...

check:
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go test ./...





