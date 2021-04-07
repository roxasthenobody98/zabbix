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

PLUGIN_CFG_POSTGRES = postgres.conf
PLUGIN_CFG_ORACLE = oracle.conf
PLUGIN_CFG_CEPH = ceph.conf
PLUGIN_CFG_MYSQL = mysql.conf
PLUGIN_CFG_MEMCACHED = memcached.conf
PLUGIN_CFG_MONGODB = mongodb.conf
PLUGIN_CFG_REDIS = redis.conf
PLUGIN_CFG_SMART = smart.conf
PLUGIN_CFG_DOCKER = docker.conf

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
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_POSTGRES)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_POSTGRES)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_POSTGRES)"
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_ORACLE)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_ORACLE)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_ORACLE)"
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_CEPH)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_CEPH)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_CEPH)"
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_MYSQL)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_MYSQL)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_MYSQL)"
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_MEMCACHED)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_MEMCACHED)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_MEMCACHED)"
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_MONGODB)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_MONGODB)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_MONGODB)"
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_REDIS)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_REDIS)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_REDIS)"
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_SMART)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_SMART)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_SMART)"
	test -f "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_DOCKER)" || cp "conf/zabbix_agent2.d/plugins.d/$(PLUGIN_CFG_DOCKER)" "$(DESTDIR)@AGENT2_CONFIG_PLUGINS_PATH@/$(PLUGIN_CFG_DOCKER)"

check:
	CGO_CFLAGS="${CGO_CFLAGS}" CGO_LDFLAGS="${CGO_LDFLAGS}" go test ./...

