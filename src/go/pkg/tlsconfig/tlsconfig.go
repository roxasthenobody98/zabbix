package tlsconfig

import (
	"crypto/tls"
	"crypto/x509"
	"errors"
	"fmt"
	"io/ioutil"
)

type Details struct {
	SessionName string
	TlsConnect  string
	TlsCaFile   string
	TlsCertFile string
	TlsKeyFile  string
	TlsWallet   string
	RawUri      string
}

func CreateConfig(details Details, skipVerify bool) (*tls.Config, error) {
	rootCertPool := x509.NewCertPool()
	pem, err := ioutil.ReadFile(details.TlsCaFile)
	if err != nil {
		return nil, err
	}

	if ok := rootCertPool.AppendCertsFromPEM(pem); !ok {
		return nil, errors.New("Failed to append PEM")
	}

	clientCerts := make([]tls.Certificate, 0, 1)
	certs, err := tls.LoadX509KeyPair(details.TlsCertFile, details.TlsKeyFile)
	if err != nil {
		return nil, err
	}

	clientCerts = append(clientCerts, certs)

	if skipVerify {
		return &tls.Config{RootCAs: rootCertPool, Certificates: clientCerts, InsecureSkipVerify: skipVerify}, nil
	}

	return &tls.Config{RootCAs: rootCertPool, Certificates: clientCerts, InsecureSkipVerify: skipVerify, ServerName: details.RawUri}, nil
}

func CreateDetailsWithWallet(session, dbConnect, wallet, uri string) (Details, error) {
	if dbConnect != "" && dbConnect != "required" {
		if wallet == "" {
			return Details{}, fmt.Errorf("missing Wallet folder path for database uri %s, with session %s", uri, session)
		}

	} else {
		if wallet != "" {
			return Details{}, fmt.Errorf("Wallet folder configuration parameter set without wallet being used for database uri %s, with session %s", uri, session)

		}
	}

	return Details{session, dbConnect, "", "", "", wallet, uri}, nil
}
