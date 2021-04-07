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

all: build

build:
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go build -ldflags="${GOLDFLAGS}" -o bin ./...

clean:
	go clean ./...
	rm -f bin/zabbix_agent2 bin/mock_server

install:
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" GOBIN=${GOBIN} \
		go install -ldflags="${GOLDFLAGS}" zabbix.com/cmd/zabbix_agent2
	$(MKDIR_P) "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@"
	test -f "$(DESTDIR)@AGENT2_CONFIG_FILE@" || cp "conf/zabbix_agent2.conf" "$(DESTDIR)@AGENT2_CONFIG_FILE@"
	for cfg in \
		postgres.conf \
		oracle.conf \
		ceph.conf \
		mysql.conf \
		memcached.conf \
		mongodb.conf \
		redis.conf \
		smart.conf \
		docker.conf; \
	do \
		test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$$cfg" || \
		cp "conf/zabbix_agent2.d/plugins.d/$$cfg" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$$cfg"; \
	done

check:
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go test ./...

