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

/**
 * @backup role
 */
class testUserRole extends CAPITest {

	public static function getUserRoleCreateData() {
		return [
			[
				'role' => [
					'name' => "Random",
				],
				'expected_error' => 'Invalid parameter "/1": the parameter "type" is missing.'
			],
			[
				'role' => [
					'type' => USER_TYPE_ZABBIX_USER
				],
				'expected_error' => 'Invalid parameter "/1": the parameter "name" is missing.'
			],
			[
				'role' => [
					'name' => '',
					'type' => USER_TYPE_ZABBIX_USER
				],
				'expected_error' => 'Invalid parameter "/1/name": cannot be empty.'
			],
			[
				'role' => [
					'name' => 'Admin role',
				],
				'expected_error' => 'User role with name "Admin role" already exists.'
			],
			[
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
			[
				'role' => [
					[
						'name' => 'User role with identical name',
						'type' => USER_TYPE_ZABBIX_USER,
					],
					[
						'name' => 'User role with identical name',
						'type' => USER_TYPE_ZABBIX_USER,
					]
				],
				'expected_error' => 'Invalid parameter "/2": value (name)=(User role with identical name) already exists.'
			],
			// Check successfully creation of user roles
			[
				'role' => [
					[
						'name' => 'User role created via API as part of batch',
						'type' => USER_TYPE_ZABBIX_USER,
					],
					[
						'name' => 'æų',
						'type' => USER_TYPE_ZABBIX_USER,
					]
				],
				'expected_error' => null
			],
			[
				'role' => [
					[
						'name' => 'Апи роль УТФ-8',
						'type' => USER_TYPE_ZABBIX_USER,
					]
				],
				'expected_error' => null
			],
			[
				'role' => [
					[
						'name' => 'User role=admin created via API',
						'type' => USER_TYPE_ZABBIX_ADMIN,
					]
				],
				'expected_error' => null
			],
			[
				'role' => [
					[
						'name' => 'User role=superadmin created via API',
						'type' => USER_TYPE_SUPER_ADMIN,
					]
				],
				'expected_error' => null
			]						
		];
	}

	/**
	* @dataProvider getUserRoleCreateData
	*/
	public function testUserRole_Create($role, $expected_error) {
		$result = $this->call('role.create', $role, $expected_error);

		if ($expected_error === null) {
			foreach ($result['result']['roleids'] as $key => $id) {
				$dbResult = DBSelect('SELECT * FROM role WHERE roleid='.zbx_dbstr($id));
				$dbRow = DBFetch($dbResult);
				$this->assertEquals($dbRow['name'], $role[$key]['name']);
				$this->assertEquals($dbRow['type'], $role[$key]['type']);
			}
		}
	}

	public static function getUserRoleUpdateData() {
		return [
			[
				'role' => [[
					'roleid' => '1',
					'name' => 'API update with non existent parameter',
					'_nonexist' => 42
				]],
				'expected_error' => 'Invalid parameter "/1": unexpected parameter "_nonexist".'
			],
			// Check user group id.
			[
				'role' => [[
					'name' => 'API user role updated'
				]],
				'expected_error' => 'Invalid parameter "/1": the parameter "roleid" is missing.'
			],
			[
				'role' => [[
					'roleid' => '',
					'name' => 'API user role udated'
				]],
				'expected_error' => 'Invalid parameter "/1/roleid": a number is expected.'
			],
			[
				'role' => [[
					'roleid' => '123456',
					'name' => 'API user role udated'
				]],
				'expected_error' => 'No permissions to referred object or it does not exist!'
			],
			[
				'role' => [[
					'roleid' => 'abc',
					'name' => 'API user role updated'
				]],
				'expected_error' => 'Invalid parameter "/1/roleid": a number is expected.'
			],
			[
				'role' => [[
					'roleid' => '1.1',
					'name' => 'API user role udated'
				]],
				'expected_error' => 'Invalid parameter "/1/roleid": a number is expected.'
			],
			// Check role group name.
			[
				'role' => [[
					'roleid' => '1',
					'name' => ''
				]],
				'expected_error' => 'Invalid parameter "/1/name": cannot be empty.'
			],
			[
				'role' => [[
					'roleid' => '1',
					'name' => 'Admin role'
				]],
				'expected_error' => 'User role with name "Admin role" already exists.'
			],
			// Check two user roles for update.
			[
				'role' => [
					[
						'roleid' => '1',
						'name' => 'User role with the same names'
					],
					[
						'roleid' => '1',
						'name' => 'User role with the same names'
					]
				],
				'expected_error' => 'Invalid parameter "/2": value (name)=(User role with the same names) already exists.'
			],
			[
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
				'expected_error' => null
			],
			[
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN,
					],
				],
				'expected_error' => 'At least one UI element must be checked.'
			],
			[
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => null
					],
				],
				'expected_error' => 'Invalid parameter "/1/rules": an array is expected.'
			],
			[
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => 1
					],
				],
				'expected_error' => 'Invalid parameter "/1/rules": an array is expected.'
			],
			[
				'role' => [
					[
						'roleid' => '1',
						'type' => USER_TYPE_ZABBIX_ADMIN,
						'rules' => ["_nonexist" => 1]
					],
				],
				'expected_error' => 'Invalid parameter "/1/rules": unexpected parameter "_nonexist".'
			],
			[
				'role' => [
					[
						'roleid' => '1',
						'name' => 'Роль пользователей обновлена апи/УТФ-8'
					]
				],
				'expected_error' => null
			],
			[
				'role' => [
					[
						'roleid' => 21,
						'type' => USER_TYPE_ZABBIX_USER,
						'rules' => [
						]
					],
				],
				'expected_error' => 'At least one UI element must be checked.'
			],
			[
				'role' => [
					[
						'roleid' => 21,
						'type' => USER_TYPE_ZABBIX_USER,
						'name' => 'User with default UI access',
						'rules' => [
							'ui.default_access' => 1
						]
					],
				],
				'expected_error' => null
			],
		];
	}

	/**
	* @dataProvider getUserRoleUpdateData
	*/
	public function testUserRole_Update($groups, $expected_error) {
		$result = $this->call('role.update', $groups, $expected_error);

		if ($expected_error === null) {
			foreach ($result['result']['roleids'] as $key => $id) {
				$dbResult = DBSelect('select * from usrgrp where roleid='.zbx_dbstr($id));
				$dbRow = DBFetch($dbResult);
				$this->assertEquals($dbRow['name'], $groups[$key]['name']);
				$this->assertEquals($dbRow['gui_access'], GROUP_GUI_ACCESS_SYSTEM);
				$this->assertEquals($dbRow['users_status'], 0);
				$this->assertEquals($dbRow['debug_mode'], 0);

				if (array_key_exists('rights', $groups[$key])){
					foreach ($groups[$key]['rights'] as $rights) {
						$dbRight = DBSelect('select * from rights where groupid='.zbx_dbstr($id));
						$dbRowRight = DBFetch($dbRight);
						$this->assertEquals($dbRowRight['id'], $rights['id']);
						$this->assertEquals($dbRowRight['permission'], $rights['permission']);
					}
				}
			}
		}
		else {
			foreach ($groups as $group) {
				if (array_key_exists('name', $group) && $group['name'] != 'Zabbix administrators'){
					$dbResult = 'select * from usrgrp where name='.zbx_dbstr($group['name']);
					$this->assertEquals(0, CDBHelper::getCount($dbResult));
				}
			}
		}
	}
}
