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

require_once dirname(__FILE__).'/../../include/CWebTest.php';
require_once dirname(__FILE__).'/../behaviors/CMessageBehavior.php';
require_once dirname(__FILE__).'/../../include/helpers/CDataHelper.php';
require_once dirname(__FILE__).'/../traits/TableTrait.php';

/**
 * @backup role, module, users
 * @on-before prepareRoleData
 * @on-before prepareUserData
 */
class testUserRolesPermissions extends CWebTest {

	/**
	 * Attach MessageBehavior to the test.
	 */
	public function getBehaviors() {
		return [CMessageBehavior::class];
	}

	/**
	 * Id of role that created for future role change for Super admin.
	 *
	 * @var integer
	 */
	protected static $super_roleid;

	/**
	 * Function used to create roles.
	 */
	public function prepareRoleData() {
		$response = CDataHelper::call('role.create', [
			[
				'name' => 'super_role',
				'type' => 3
			]
		]);
		$this->assertArrayHasKey('roleids', $response);
		self::$super_roleid = $response['roleids'][0];
	}

	public function prepareUserData() {
		CDataHelper::call('user.create', [
			[
				'alias' => 'user_for_role',
				'passwd' => 'zabbix',
				'roleid' => self::$super_roleid,
				'usrgrps' => [
					[
						'usrgrpid' => '7'
					]
				]
			]
		]);
	}

	public static function getPageActionsData() {
		return [
			// Map creation/edit.
			[
				[
					'buttons' => [
						'Create map',
						'Import',
						'Delete'
					],
					'button_selector' => [
						'xpath://*[@data-url="sysmap.php?sysmapid=1"]'
					],
					'list_page' => 'sysmaps.php',
					'action_page' => 'zabbix.php?action=map.view&sysmapid=1',
					'action' => [
						'Create and edit maps' => false
					],
					'check_links' => ['sysmap.php?sysmapid=1', 'sysmaps.php?form=Create+map']
				]
			],
			// Dashboard creation/edit.
			[
				[
					'return_status' => true,
					'buttons' => [
						'Create dashboard',
						'Delete'
					],
					'button_selector' => [
						'xpath://button[@id="dashbrd-edit"]',
						'xpath://button[@id="dashbrd-actions"]',
					],
					'list_page' => 'zabbix.php?action=dashboard.list',
					'action_page' => 'zabbix.php?action=dashboard.view&dashboardid=122',
					'action' => [
						'Create and edit dashboards and screens' => false
					],
					'check_links' => ['zabbix.php?action=dashboard.view&new=1']
				]
			],
			// Screen creation/edit.
			[
				[
					'buttons' => [
						'Create screen',
						'Import',
						'Delete'
					],
					'button_selector' => [
						'xpath://button[@id="edit"]'
					],
					'list_page' => 'screenconf.php',
					'action_page' => 'screens.php?elementid=1',
					'action' => [
						'Create and edit dashboards and screens' => false
					],
					'check_links' => ['screenedit.php?screenid=1', 'screenconf.php?form=Create+screen']
				]
			],
			// Maintenance creation/edit.
			[
				[
					'buttons' => [
						'Create maintenance period',
						'Delete'
					],
					'button_selector' => [
						'xpath://button[@id="update"]',
						'xpath://button[@id="clone"]',
						'xpath://button[@id="delete"]'
					],
					'list_page' => 'maintenance.php',
					'action_page' => 'maintenance.php?form=update&maintenanceid=5',
					'action' => [
						'Create and edit maintenance' => false
					],
					'check_links' => ['maintenance.php?form=create']
				]
			]
		];
	}


	/**
	 * Check creation/edit for dashboard, map, screen, maintenanceю
	 *
	 * @dataProvider getPageActionsData
	 */
	public function testUserRolesPermissions_PageActions($data) {
		$this->checkAction($data['buttons'], $data['button_selector'], $data['list_page'], $data['action_page'], $data['action']);
		$this->checkLinks($data['check_links']);
		if (array_key_exists('return_status', $data)) {
			$this->changeAction(['Create and edit dashboards and screens' => true]);
		}
	}

	/**
	 * Check disabled actions with links.
	 *
	 * @param array $links	checked links after disabling action
	 */
	private function checkLinks($links) {
		foreach ($links as $link) {
			$this->page->open($link)->waitUntilReady();
			$this->assertMessage(TEST_BAD, 'Access denied', 'You are logged in as "user_for_role". '.
					'You have no permissions to access this page.');
			$this->query('button:Go to "Dashboard"')->one()->waitUntilClickable()->click();
			$this->assertContains('zabbix.php?action=dashboard', $this->page->getCurrentUrl());
		}
	}

	/**
	 *
	 * @param type $buttons
	 * @param type $buttons_selector
	 * @param type $list_page
	 * @param type $action_page
	 * @param type $action
	 */
	private function checkAction($buttons, $buttons_selector, $list_page, $action_page, $action) {
		$this->page->login();
		$this->page->userLogin('user_for_role', 'zabbix');
		foreach ([true, false] as $action_status) {
			$this->page->open($list_page);
			foreach ($buttons as $button) {
				if ($button === 'Delete') {
					$table = $this->query('class:list-table')->asTable()->waitUntilReady()->one();
					$table->getRow(0)->select();
				}
				$this->assertTrue($this->query('button', $button)->one()->isEnabled($action_status));
			}
			$this->page->open($action_page);
			foreach ($buttons_selector as $button) {
				$this->assertTrue($this->query($buttons_selector)->one()->isEnabled($action_status));
				if ($action_page === 'zabbix.php?action=dashboard.view&dashboardid=122') {
					$new_widget = ($action_status === true) ? 'Add a new widget' : 'You do not have permissions to edit dashboard';
					$this->assertEquals($new_widget, $this->query('class:dashbrd-grid-new-widget-label')->one()->getText());
				}
				elseif ($action_page === 'maintenance.php?form=update&maintenanceid=5') {
					$this->assertTrue($this->query('button:Cancel')->one()->isEnabled());
				}

			}
			if ($action_status === true) {
				$this->changeAction($action);
			}
		}
	}

	/**
	 * Enable/disable action.
	 *
	 * @param array $action		action name in role page with status true/false
	 */
	private function changeAction($action) {
		$this->page->open('zabbix.php?action=userrole.edit&roleid='.self::$super_roleid);
		$form = $this->query('id:userrole-form')->waitUntilPresent()->asFluidForm()->one();
		$form->fill($action);
		$form->submit();
	}
}
