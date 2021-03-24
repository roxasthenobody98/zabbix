<?php declare(strict_types = 1);
/*
** Zabbix
** Copyright (C) 2001-2021 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
**/


/**
 * Helper class containing methods for validation of hosts interfaces.
 */
class CApiHostInterfaceHelper {

	/**
	 * Check if main interfaces are correctly set for every interface type. Each host must either have only one main
	 * interface for each interface type, or have no interface of that type at all. If no interfaces are given, it means
	 * the last remaining main interface is trying to be deleted. In that case use $db_interfaces as reference.
	 *
	 * @param array $interfaces     Array of interfaces that are created, updated (plus DB) or deleted (plus DB).
	 * @param array $db_interfaces  Array of interfaces from DB (used for delete only and if no interfaces are given).
	 *
	 * @throws APIException
	 */
	public static function checkMainInterfaces(array $interfaces, array $db_interfaces = []) {
		if (!$interfaces && $db_interfaces) {
			$host = API::Host()->get([
				'output' => ['name', 'hostid'],
				'hostids' => zbx_objectValues($db_interfaces, 'hostid'),
				'preservekeys' => true,
				'nopermissions' => true
			]);
			$host = reset($host);

			if ($host) {
				foreach ($db_interfaces as $db_interface) {
					if (bccomp($db_interface['hostid'], $host['hostid']) == 0) {
						$type = $db_interface['type'];
						break;
					}
				}

				throw new APIException(ZBX_API_ERROR_PARAMETERS, _s('No default interface for "%1$s" type on "%2$s".',
					hostInterfaceTypeNumToName($type), $host['name']
				));
			}
			// Otherwise it's not a host. Could be a Proxy.
		}

		$interface_count = [];

		if ($db_interfaces) {
			foreach ($db_interfaces as $db_interface) {
				$hostid = $db_interface['hostid'];
				$type = $db_interface['type'];

				if (!array_key_exists($hostid, $interface_count)) {
					$interface_count[$hostid] = [];
				}

				if (!array_key_exists($type, $interface_count[$hostid])) {
					$interface_count[$hostid][$type] = ['main' => 0, 'all' => 0];
				}
			}
		}

		foreach ($interfaces as $interface) {
			$hostid = $interface['hostid'];
			$type = $interface['type'];

			if (!array_key_exists($hostid, $interface_count)) {
				$interface_count[$hostid] = [];
			}

			if (!array_key_exists($type, $interface_count[$hostid])) {
				$interface_count[$hostid][$type] = ['main' => 0, 'all' => 0];
			}

			if ($interface['main'] == INTERFACE_PRIMARY) {
				$interface_count[$hostid][$type]['main']++;
			}
			else {
				$interface_count[$hostid][$type]['all']++;
			}
		}

		$main_interface_count = [];
		$all_interface_count = [];

		foreach ($interface_count as $hostid => $interface_type) {
			foreach ($interface_type as $type => $counters) {
				if (!array_key_exists($hostid, $main_interface_count)) {
					$main_interface_count[$hostid] = 0;
				}

				$main_interface_count[$hostid] += $counters['main'];

				if (!array_key_exists($hostid, $all_interface_count)) {
					$all_interface_count[$hostid] = 0;
				}

				$all_interface_count[$hostid] += $counters['all'];
			}
		}

		foreach ($interface_count as $hostid => $interface_type) {
			foreach ($interface_type as $type => $counters) {
				if (($counters['all'] > 0 && $counters['main'] == 0)
						|| ($main_interface_count[$hostid] == 0 && $all_interface_count[$hostid] == 0)) {
					$host = API::Host()->get([
						'output' => ['name'],
						'hostids' => $hostid,
						'preservekeys' => true,
						'nopermissions' => true
					]);
					$host = reset($host);

					if ($host) {
						throw new APIException(ZBX_API_ERROR_PARAMETERS, _s('No default interface for "%1$s" type on "%2$s".',
							hostInterfaceTypeNumToName($type), $host['name']
						));
					}
					// Otherwise it's not a host. Could be a Proxy.
				}

				if ($counters['main'] > 1) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS,
						_('Host cannot have more than one default interface of the same type.')
					);
				}
			}
		}
	}

	/**
	 * Check interfaces input.
	 *
	 * @param array  $interfaces
	 * @param string $method
	 *
	 * @throws APIException
	 */
	public static function checkInput(array &$interfaces, $method) {
		$update = ($method == 'update');

		// permissions
		if ($update) {
			$interfaceDBfields = ['interfaceid' => null];
			$dbInterfaces = API::HostInterface()->get([
				'output' => API_OUTPUT_EXTEND,
				'interfaceids' => zbx_objectValues($interfaces, 'interfaceid'),
				'editable' => true,
				'preservekeys' => true
			]);
		}
		else {
			$interfaceDBfields = [
				'hostid' => null,
				'ip' => null,
				'dns' => null,
				'useip' => null,
				'port' => null,
				'main' => null
			];
		}

		$dbHosts = API::Host()->get([
			'output' => ['host'],
			'hostids' => zbx_objectValues($interfaces, 'hostid'),
			'editable' => true,
			'preservekeys' => true
		]);

		$dbProxies = API::Proxy()->get([
			'output' => ['host'],
			'proxyids' => zbx_objectValues($interfaces, 'hostid'),
			'editable' => true,
			'preservekeys' => true
		]);

		$check_have_items = [];
		foreach ($interfaces as &$interface) {
			if (!check_db_fields($interfaceDBfields, $interface)) {
				throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Incorrect arguments passed to function.'));
			}

			if ($update) {
				if (!isset($dbInterfaces[$interface['interfaceid']])) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS,
						_('No permissions to referred object or it does not exist!')
					);
				}

				$dbInterface = $dbInterfaces[$interface['interfaceid']];
				if (isset($interface['hostid']) && bccomp($dbInterface['hostid'], $interface['hostid']) != 0) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS, _s('Cannot switch host for interface.'));
				}

				if (array_key_exists('type', $interface) && $interface['type'] != $dbInterface['type']) {
					$check_have_items[] = $interface['interfaceid'];
				}

				$interface['hostid'] = $dbInterface['hostid'];

				// we check all fields on "updated" interface
				$updInterface = $interface;
				$interface = zbx_array_merge($dbInterface, $interface);
			}
			else {
				if (!isset($dbHosts[$interface['hostid']]) && !isset($dbProxies[$interface['hostid']])) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS,
						_('No permissions to referred object or it does not exist!')
					);
				}

				if (isset($dbProxies[$interface['hostid']])) {
					$interface['type'] = INTERFACE_TYPE_UNKNOWN;
				}
				elseif (!isset($interface['type'])) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Incorrect arguments passed to function.'));
				}
			}

			if (zbx_empty($interface['ip']) && zbx_empty($interface['dns'])) {
				throw new APIException(ZBX_API_ERROR_PARAMETERS, _('IP and DNS cannot be empty for host interface.'));
			}

			if ($interface['useip'] == INTERFACE_USE_IP && zbx_empty($interface['ip'])) {
				throw new APIException(ZBX_API_ERROR_PARAMETERS,
					_s('Interface with DNS "%1$s" cannot have empty IP address.', $interface['dns'])
				);
			}

			if ($interface['useip'] == INTERFACE_USE_DNS && zbx_empty($interface['dns'])) {
				if ($dbHosts && !empty($dbHosts[$interface['hostid']]['host'])) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS,
						_s('Interface with IP "%1$s" cannot have empty DNS name while having "Use DNS" property on "%2$s".',
							$interface['ip'],
							$dbHosts[$interface['hostid']]['host']
						)
					);
				}
				elseif ($dbProxies && !empty($dbProxies[$interface['hostid']]['host'])) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS,
						_s('Interface with IP "%1$s" cannot have empty DNS name while having "Use DNS" property on "%2$s".',
							$interface['ip'],
							$dbProxies[$interface['hostid']]['host']
						)
					);
				}
				else {
					throw new APIException(ZBX_API_ERROR_PARAMETERS,
						_s('Interface with IP "%1$s" cannot have empty DNS name.', $interface['ip'])
					);
				}
			}

			if (isset($interface['dns'])) {
				self::checkDns($interface);
			}
			if (isset($interface['ip'])) {
				self::checkIp($interface);
			}
			if (isset($interface['port']) || $method == 'create') {
				self::checkPort($interface);
			}

			if ($update) {
				$interface = $updInterface;
			}
		}
		unset($interface);

		// check if any of the affected hosts are discovered
		if ($update) {
			$interfaces = $this->extendObjects('interface', $interfaces, ['hostid']);

			if ($check_have_items) {
				$this->checkIfInterfaceHasItems($check_have_items);
			}
		}
		$this->checkValidator(zbx_objectValues($interfaces, 'hostid'), new CHostNormalValidator([
			'message' => _('Cannot update interface for discovered host "%1$s".')
		]));
	}

	/**
	 * Validates the "dns" field.
	 *
	 * @param array $interface
	 * @param string $interface['dns']
	 *
	 * @throws APIException if the field is invalid.
	 */
	private static function checkDns(array $interface) {
		if ($interface['dns'] === '') {
			return;
		}

		$user_macro_parser = new CUserMacroParser();

		if (!preg_match('/^'.ZBX_PREG_DNS_FORMAT.'$/', $interface['dns'])
				&& $user_macro_parser->parse($interface['dns']) != CParser::PARSE_SUCCESS) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS,
				_s('Incorrect interface DNS parameter "%1$s" provided.', $interface['dns'])
			);
		}
	}

	/**
	 * Validates the "ip" field.
	 *
	 * @param array $interface
	 * @param string $interface['ip']
	 *
	 * @throws APIException if the field is invalid.
	 */
	private static function checkIp(array $interface) {
		if ($interface['ip'] === '') {
			return;
		}

		$user_macro_parser = new CUserMacroParser();

		if (preg_match('/^'.ZBX_PREG_MACRO_NAME_FORMAT.'$/', $interface['ip'])
				|| $user_macro_parser->parse($interface['ip']) == CParser::PARSE_SUCCESS) {
			return;
		}

		$ip_parser = new CIPParser(['v6' => ZBX_HAVE_IPV6]);

		if ($ip_parser->parse($interface['ip']) != CParser::PARSE_SUCCESS) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS, _s('Invalid IP address "%1$s".', $interface['ip']));
		}
	}

	/**
	 * Validates the "port" field.
	 *
	 * @param array $interface
	 *
	 * @throws APIException if the field is empty or invalid.
	 */
	private static function checkPort(array $interface) {
		if (!isset($interface['port']) || zbx_empty($interface['port'])) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Port cannot be empty for host interface.'));
		}
		elseif (!validatePortNumberOrMacro($interface['port'])) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS,
				_s('Incorrect interface port "%1$s" provided.', $interface['port'])
			);
		}
	}

	/**
	 * Check weather interfaces is linked to item(s).
	 *
	 * @param array $interfaceids
	 *
	 * @throws APIException if some of interfaces is linked to item(s).
	 */
	public static function checkIfInterfaceHasItems(array $interfaceids) {
		$items = API::Item()->get([
			'output' => ['name'],
			'selectHosts' => ['name'],
			'interfaceids' => $interfaceids,
			'preservekeys' => true,
			'nopermissions' => true,
			'limit' => 1
		]);

		foreach ($items as $item) {
			$host = reset($item['hosts']);

			throw new APIException(ZBX_API_ERROR_PARAMETERS,
				_s('Interface is linked to item "%1$s" on "%2$s".', $item['name'], $host['name'])
			);
		}
	}

	/**
	 * Check SNMP related inputs.
	 *
	 * @param array $interfaces
	 *
	 * @throws APIException
	 */
	public static function checkSnmpInput(array $interfaces) {
		foreach ($interfaces as $interface) {
			if (!array_key_exists('type', $interface) || $interface['type'] != INTERFACE_TYPE_SNMP) {
				continue;
			}

			if (!array_key_exists('details', $interface)) {
				throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Incorrect arguments passed to function.'));
			}

			self::checkSnmpVersion($interface);

			self::checkSnmpCommunity($interface);

			self::checkSnmpBulk($interface);

			self::checkSnmpSecurityLevel($interface);

			self::checkSnmpAuthProtocol($interface);

			self::checkSnmpPrivProtocol($interface);
		}
	}

	/**
	 * Check if SNMP version is valid. Valid versions: SNMP_V1, SNMP_V2C, SNMP_V3.
	 *
	 * @param array $interface
	 *
	 * @throws APIException if "version" value is incorrect.
	 */
	private static function checkSnmpVersion(array $interface) {
		if (!array_key_exists('version', $interface['details'])
				|| !in_array($interface['details']['version'], [SNMP_V1, SNMP_V2C, SNMP_V3])) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Incorrect arguments passed to function.'));
		}
	}

	/**
	 * Check SNMP community. For SNMPv1 and SNMPv2c it required.
	 *
	 * @param array $interface
	 *
	 * @throws APIException if "community" value is incorrect.
	 */
	private static function checkSnmpCommunity(array $interface) {
		if (($interface['details']['version'] == SNMP_V1 || $interface['details']['version'] == SNMP_V2C)
				&& (!array_key_exists('community', $interface['details'])
					|| $interface['details']['community'] === '')) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Incorrect arguments passed to function.'));
		}
	}

	/**
	 * Validates SNMP interface "bulk" field.
	 *
	 * @param array $interface
	 *
	 * @throws APIException if "bulk" value is incorrect.
	 */
	private static function checkSnmpBulk(array $interface) {
		if ($interface['type'] !== null && (($interface['type'] != INTERFACE_TYPE_SNMP
					&& isset($interface['details']['bulk']) && $interface['details']['bulk'] != SNMP_BULK_ENABLED)
					|| ($interface['type'] == INTERFACE_TYPE_SNMP && isset($interface['details']['bulk'])
						&& (zbx_empty($interface['details']['bulk'])
							|| ($interface['details']['bulk'] != SNMP_BULK_DISABLED
							&& $interface['details']['bulk'] != SNMP_BULK_ENABLED))))) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Incorrect bulk value for interface.'));
		}
	}

	/**
	 * Check SNMP Security level field.
	 *
	 * @param array $interface
	 * @param array $interface['details']
	 * @param array $interface['details']['version']        SNMP version
	 * @param array $interface['details']['securitylevel']  SNMP security level
	 *
	 * @throws APIException if "securitylevel" value is incorrect.
	 */
	private static function checkSnmpSecurityLevel(array $interface) {
		if ($interface['details']['version'] == SNMP_V3 && (array_key_exists('securitylevel', $interface['details'])
					&& !in_array($interface['details']['securitylevel'], [ITEM_SNMPV3_SECURITYLEVEL_NOAUTHNOPRIV,
						ITEM_SNMPV3_SECURITYLEVEL_AUTHNOPRIV, ITEM_SNMPV3_SECURITYLEVEL_AUTHPRIV]))) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Incorrect arguments passed to function.'));
		}
	}

	/**
	 * Check SNMP authentication  protocol.
	 *
	 * @param array $interface
	 * @param array $interface['details']
	 * @param array $interface['details']['version']       SNMP version
	 * @param array $interface['details']['authprotocol']  SNMP authentication protocol
	 *
	 * @throws APIException if "authprotocol" value is incorrect.
	 */
	private static function checkSnmpAuthProtocol(array $interface) {
		if ($interface['details']['version'] == SNMP_V3 && (array_key_exists('authprotocol', $interface['details'])
					&& !in_array($interface['details']['authprotocol'], [ITEM_AUTHPROTOCOL_MD5,
						ITEM_AUTHPROTOCOL_SHA]))) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS, _('Incorrect arguments passed to function.'));
		}
	}

	/**
	 * Check SNMP Privacy protocol.
	 *
	 * @param array $interface
	 * @param array $interface['details']
	 * @param array $interface['details']['version']       SNMP version
	 * @param array $interface['details']['privprotocol']  SNMP privacy protocol
	 *
	 * @throws APIException if "privprotocol" value is incorrect.
	 */
	private static function checkSnmpPrivProtocol(array $interface) {
		if ($interface['details']['version'] == SNMP_V3 && (array_key_exists('privprotocol', $interface['details'])
				&& !in_array($interface['details']['privprotocol'], [ITEM_PRIVPROTOCOL_DES, ITEM_PRIVPROTOCOL_AES]))) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS,  _('Incorrect arguments passed to function.'));
		}
	}

	/**
	 * Sanitize SNMP fields by version.
	 *
	 * @param array $interfaces
	 *
	 * @return array
	 */
	public static function sanitizeSnmpFields(array $interfaces): array {
		$default_fields = [
			'community' => '',
			'securityname' => '',
			'securitylevel' => DB::getDefault('interface_snmp', 'securitylevel'),
			'authpassphrase' => '',
			'privpassphrase' => '',
			'authprotocol' => DB::getDefault('interface_snmp', 'authprotocol'),
			'privprotocol' => DB::getDefault('interface_snmp', 'privprotocol'),
			'contextname' => ''
		];

		foreach ($interfaces as &$interface) {
			if ($interface['version'] == SNMP_V1 || $interface['version'] == SNMP_V2C) {
				unset($interface['securityname'], $interface['securitylevel'], $interface['authpassphrase'],
					$interface['privpassphrase'], $interface['authprotocol'], $interface['privprotocol'],
					$interface['contextname']
				);
			}
			else {
				unset($interface['community']);
			}

			$interface = $interface + $default_fields;
		}

		return $interfaces;
	}

	public static function prepareUpdateData(array $interfaces): array {
		$data = [];

		while ($interfaces) {
			$interface_params = reset($interfaces);
			$interfaceid = $interface_params['interfaceid'];
			$interfaceids = [$interfaceid];
			unset($interface_params['interfaceid']);
			unset($interfaces[key($interfaces)]);

			foreach ($interfaces as $index => $interface) {
				if ($interface_params === array_diff_key($interface, ['interfaceid' => true])) {
					$interfaceids[] = $interface['interfaceid'];
					unset($interfaces[$index]);
				}
			}

			$data[] = [
				'values' => $interface_params,
				'where' => ['interfaceid' => $interfaceids]
			];
		}

		return $data;
	}
}
