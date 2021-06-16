package tlsconfig

import (
	"fmt"
)

type Details struct {
	SessionName string
	TlsConnect  string
	TlsWallet   string
	RawUri      string
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

	return Details{session, dbConnect, wallet, uri}, nil
}
