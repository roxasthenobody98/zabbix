<?php
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
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/


require_once dirname(__FILE__).'/../include/CAPITest.php';
require_once dirname(__FILE__).'/../../include/classes/helpers/CRoleHelper.php';

/**
 * @backup role
 */
class testUserRole extends CAPITest {

	public static function getUserRoleCreateData() {
		$cases = [
			'Missing type param' => [
				'role' => [
					[
						'name' => "Random",
					],
				],
				'expected_error' => 'Invalid parameter "/1": the parameter "type" is missing.'
			],
			'Missing name param' => [
				'role' => [
					[
						'type' => USER_TYPE_ZABBIX_USER
					]
				],
				'expected_error' => 'Invalid parameter "/1": the parameter "name" is missing.'
			],
			'Empty name' => [
				'role' => [
					[
						'name' => '',
						'type' => USER_TYPE_ZABBIX_USER
					]
				],
				'expected_error' => 'Invalid parameter "/1/name": cannot be empty.'
			],
			'Existing name' => [
				'role' => [
					[
						'name' => 'Admin role',
						'type' => USER_TYPE_ZABBIX_ADMIN
					]
				],
				'expected_error' => 'User role with name "Admin role" already exists.'
			],
			'Existing name in set' => [
				'role' => [
					[
						'name' => 'One user role with existing name',
						'type' => USER_TYPE_ZABBIX_USER
					],
					[
						'name' => 'Admin role',
						'type' => USER_TYPE_ZABBIX_USER
					]
				],
				'expected_error' => 'User role with name "Admin role" already exists.'
			],
			'Same name in set' => [
				'role' => [
					[
						'name' => 'User role with identical name',
						'type' => USER_TYPE_ZABBIX_USER
					],
					[
						'name' => 'User role with identical name',
						'type' => USER_TYPE_ZABBIX_USER
					]
				],
				'expected_error' => 'Invalid parameter "/2": value (name)=(User role with identical name) already exists.'
			],
			'Set with UTF' => [
				'role' => [
					[
						'name' => 'User role created via API as part of batch',
						'type' => USER_TYPE_ZABBIX_USER
					],
					[
						'name' => 'æų',
						'type' => USER_TYPE_ZABBIX_USER
					]
				],
				'expected_error' => null
			],
			'Single user, UTF' => [
				'role' => [
					[
						'name' => 'Апи роль УТФ-8',
						'type' => USER_TYPE_ZABBIX_USER
					]
				],
				'expected_error' => null
			],
			'Single Admin' => [
				'role' => [
					[
						'name' => 'User role=admin created via API',
						'type' => USER_TYPE_ZABBIX_ADMIN
					]
				],
				'expected_error' => null
			],
			'Single Superadmin' => [
				'role' => [
					[
						'name' => 'User role=superadmin created via API',
						'type' => USER_TYPE_SUPER_ADMIN
					]
				],
				'expected_error' => null
			],
			'Create role with default UI access revoked' => [
				'role' => [
					[
						'name' => 'All but default access checked',
						'type' => USER_TYPE_ZABBIX_USER,
						'rules' => [
							'ui.default_access' => 0
						]
					],
				],
				'expected_error' => null
			]
		];

		$role = [
			'name' => 'All ui access revoked',
			'type' => USER_TYPE_SUPER_ADMIN,
			'rules' => [
				'ui' => [],
				'ui.default_access' => 0
			]
		];
		$ui_rules = CRoleHelper::getAllUiElements(USER_TYPE_SUPER_ADMIN, true);
		foreach($ui_rules as $rule){
			$role['rules']['ui'][] = ['name'=>$rule, 'status' => 0];
		}
		$cases['User with all ui access disabled'] = [
			'role' => $role,
			'expected_error' => 'At least one UI element must be checked.'
		];

		return $cases;
	}

	/**
	* @dataProvider getUserRoleCreateData
	*/
	public function testUserRole_Create($role, $expected_error) {
		$result = $this->call('role.create', $role, $expected_error);

		if ($expected_error !== null) {
			return;
		}

		foreach ($result['result']['roleids'] as $key => $id) {
			$dbResult = DBSelect('SELECT * FROM role WHERE roleid='.zbx_dbstr($id));
			$dbRow = DBFetch($dbResult);

			if (array_key_exists('name', $role[$key])) {
				$this->assertEquals($dbRow['name'], $role[$key]['name']);
			}

			if (array_key_exists('type', $role[$key])) {
				$this->assertEquals($dbRow['type'], $role[$key]['type']);
			}
		}
	}

	public static function getUserRoleUpdateData() {
		return [
			'Unknown param' => [
				'role' => [
					[
						'roleid' => '1',
						'name' => 'API update with non existent parameter',
						'_nonexist' => 42
					]
				],
				'expected_error' => 'Invalid parameter "/1": unexpected parameter "_nonexist".'
			],
			'roleid missing' => [
				'role' => [
					[
						'name' => 'API user role updated'
					]
				],
				'expected_error' => 'Invalid parameter "/1": the parameter "roleid" is missing.'
			],
			'roleid empty' => [
				'role' => [
					[
						'roleid' => '',
						'name' => 'API user role udated'
					]
				],
				'expected_error' => 'Invalid parameter "/1/roleid": a number is expected.'
			],
			'Non-existing roleid' => [
				'role' => [
					[
						'roleid' => '123456',
						'name' => 'API user role udated'
					]
				],
				'expected_error' => 'No permissions to referred object or it does not exist!'
			],
			'Non-numeric roleid' => [
				'role' => [
					[
						'roleid' => 'abc',
						'name' => 'API user role updated'
					]
				],
				'expected_error' => 'Invalid parameter "/1/roleid": a number is expected.'
			],
			'Non-int roleid' => [
				'role' => [
					[
						'roleid' => '1.1',
						'name' => 'API user role udated'
					]
				],
				'expected_error' => 'Invalid parameter "/1/roleid": a number is expected.'
			],
			'Empty name' => [
				'role' => [
					[
						'roleid' => '1',
						'name' => ''
					]
				],
				'expected_error' => 'Invalid parameter "/1/name": cannot be empty.'
			],
			'Update to pre-existing name' => [
				'role' => [
					[
						'roleid' => '1',
						'name' => 'Admin role',
						'type' => USER_TYPE_ZABBIX_ADMIN
					]
				],
				'expected_error' => 'User role with name "Admin role" already exists.'
			],
			'Pre-existing name in set' => [
				'role' => [
					[
						'roleid' => '1',
						'name' => 'User role with the same names'
					],
					[
						'roleid' => '1',
						'name' => 'Admin role'
					]
				],
				'expected_error' => 'User role with name "Admin role" already exists.'
			],
			'Update to same name' => [
				'role' => [
					[
						'roleid' => '1',
						'name' => 'Update on user with same name'
					],
					[
						'roleid' => '1',
						'name' => 'Update on user with same name'
					]
				],
				'expected_error' => 'Invalid parameter "/2": value (name)=(Update on user with same name) already exists.'
			],
			'Type change, no UIrule' => [
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN
					],
				],
				'expected_error' => null
			],
			'Type change, nonexist type' => [
				'role' => [
					[
						'roleid' => '1',
						'type' => 123
					],
				],
				'expected_error' => 'Invalid parameter "/1/type": value must be one of 1, 2, 3.'
			],
			'Empty ruleset' => [
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => ''
					],
				],
				'expected_error' => 'Invalid parameter "/1/rules": an array is expected.'
			],
			'Null ruleset' => [
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => null
					],
				],
				'expected_error' => 'Invalid parameter "/1/rules": an array is expected.'
			],
			'Non-array ruleset' => [
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => 1
					],
				],
				'expected_error' => 'Invalid parameter "/1/rules": an array is expected.'
			],
			'Uknown rule' => [
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => ["_nonexist" => 1]
					],
				],
				'expected_error' => 'Invalid parameter "/1/rules": unexpected parameter "_nonexist".'
			],
			'Name in UTF8' => [
				'role' => [
					[
						'roleid' => '1',
						'name' => 'Роль пользователей обновлена апи/УТФ-8'
					]
				],
				'expected_error' => null
			],
			'Update with rule on same type' => [
				'role' => [
					[
						'roleid' => 1,
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => [
							'ui.default_access' => 1
						]
					],
				],
				'expected_error' => null
			],
			'Revoke default UI access rule' => [
				'role' => [
					[
						'roleid' => 1,
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => [
							'ui.default_access' => 0
						]
					],
				],
				'expected_error' => 'At least one UI element must be checked.'
			],
			'Revoke default UI access rule, add another' => [
				'role' => [
					[
						'roleid' => 1,
						'rules' => [
							'ui' => [
								[
									"name" => "inventory.hosts",
									"status" => 1
								],
							],
							"ui.default_access" => 0,
						]
					],
				],
				'expected_error' => null
			],
			'Revoke only left UI access rule' => [
				'role' => [
					[
						'roleid' => 1,
						'rules' => [
							'ui' => [
								[
									"name" => "inventory.hosts",
									"status" => 0
								],
							]
						]
					],
				],
				'expected_error' => 'At least one UI element must be checked.'
			],
			'Update with UI rule and type change' => [
				'role' => [
					[
						'roleid' => 1,
						'type' => USER_TYPE_ZABBIX_USER,
						'rules' => [
							'ui.default_access' => 1
						]
					],
				],
				'expected_error' => null
			],
			'Nonexist UI rule' => [
				'role' => [
					[
						'roleid' => 1,
						'rules' => [
							'ui' => [
								[
									'name' => '_nonexist',
									'status' => 1
								]
							]
						]
					],
				],
				'expected_error' => 'UI element "_nonexist" is not available.'
			],
		];
	}

	/**
	* @dataProvider getUserRoleUpdateData
	*/
	public function testUserRole_Update($role, $expected_error) {
		$result = $this->call('role.update', $role, $expected_error);

		if ($expected_error !== null) {
			return;
		}

		foreach ($result['result']['roleids'] as $key => $id) {
			$dbResult = DBSelect('SELECT * FROM role WHERE roleid='.zbx_dbstr($id));
			$dbRow = DBFetch($dbResult);

			if (array_key_exists('name', $role[$key])) {
				$this->assertEquals($dbRow['name'], $role[$key]['name']);
			}

			if (array_key_exists('type', $role[$key])) {
				$this->assertEquals($dbRow['type'], $role[$key]['type']);
			}
		}
	}
}
