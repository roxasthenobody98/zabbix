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
	 * Check creation/edit for dashboard, map, screen, maintenance.
	 *
	 * @dataProvider getPageActionsData
	 */
	public function testUserRolesPermissions_PageActions($data) {
		$this->page->login();
		$this->page->userLogin('user_for_role', 'zabbix');
		foreach ([true, false] as $action_status) {
			$this->page->open($data['list_page']);
			foreach ($data['buttons'] as $button) {
				if ($button === 'Delete') {
					$table = $this->query('class:list-table')->asTable()->waitUntilReady()->one();
					$table->getRow(0)->select();
				}
				$this->assertTrue($this->query('button', $button)->one()->isEnabled($action_status));
			}
			$this->page->open($data['action_page']);
			foreach ($data['button_selector'] as $button) {
				$this->assertTrue($this->query($data['button_selector'])->one()->isEnabled($action_status));
				if ($data['action_page'] === 'zabbix.php?action=dashboard.view&dashboardid=122') {
					$new_widget = ($action_status === true) ? 'Add a new widget' : 'You do not have permissions to edit dashboard';
					$this->assertEquals($new_widget, $this->query('class:dashbrd-grid-new-widget-label')->one()->getText());
				}
				elseif ($data['action_page'] === 'maintenance.php?form=update&maintenanceid=5') {
					$this->assertTrue($this->query('button:Cancel')->one()->isEnabled());
				}

			}
			if ($action_status === true) {
				$this->changeAction($data['action']);
			}
		}
		$this->checkLinks($data['check_links']);
		if (array_key_exists('return_status', $data)) {
			$this->changeAction(['Create and edit dashboards and screens' => true]);
		}
	}

	public static function getProblemActionsData() {
		return [
			// Comment.
			[
				[
					'activity_id' => [
						'message'
					],
					'disabled_action' => [
						'Add problem comments' => false
					],
					'enabled_action' => [
						'Add problem comments' => true
					]
				]
			],
			// Severity.
			[
				[
					'activity_id' => [
						'change_severity'
					],
					'disabled_action' => [
						'Change severity' => false
					],
					'enabled_action' => [
						'Change severity' => true
					]
				]
			],
			// Acknowledge problem.
			[
				[
					'activity_id' => [
						'acknowledge_problem'
					],
					'disabled_action' => [
						'Acknowledge problems' => false
					],
					'enabled_action' => [
						'Acknowledge problems' => true
					]
				]
			],
			// Close problem.
			[
				[
					'activity_id' => [
						'close_problem'
					],
					'disabled_action' => [
						'Close problems' => false
					],
					'enabled_action' => [
						'Close problems' => true
					]
				]
			]
		];
	}

	/**
	 * Check problem actions.
	 *
	 * @dataProvider getProblemActionsData
	 */
	public function testUserRolesPermissions_ProblemActions($data) {
		$this->page->login();
		$this->page->userLogin('user_for_role', 'zabbix');
		foreach ([true, false] as $action_status) {
			$this->page->open('zabbix.php?action=problem.view')->waitUntilReady();
			$row = $this->query('class:list-table')->asTable()->one()->findRow('Problem', 'Test trigger with tag');
			$row->query('link', 'No')->one()->click();
			$dialog = COverlayDialogElement::find()->waitUntilVisible()->one();
			foreach ($data['activity_id'] as $id) {
				$this->assertTrue($dialog->query('id', $id)->one()->isEnabled($action_status));
			}
			if ($action_status === true) {
				$this->changeAction($data['disabled_action']);
			}
			else {
				$this->changeAction($data['enabled_action']);
			}
		}
	}

	/**
	 * Check that Acknowledge link is disabled after all problem actions is disabled.
	 */
	public function testUserRolesPermissions_ProblemActionsAll() {
		$context_before = [
			'Problems',
			'Acknowledge',
			'Configuration',
			'Webhook url for all',
			'1_item'
		];
		$actions = [
			'Add problem comments' => false,
			'Change severity' => false,
			'Acknowledge problems' => false,
			'Close problems' => false
		];
		$this->page->login();
		$this->page->userLogin('user_for_role', 'zabbix');
		foreach ([true, false] as $action_status) {
			// Problem page.
			$this->page->open('zabbix.php?action=problem.view')->waitUntilReady();
			$problem_row = $this->query('class:list-table')->asTable()->one()->findRow('Time', '2020-10-23 18:23:48');
			$this->assertTrue($problem_row->query('xpath://*[text()="No"]')->one()->isClickable($action_status));

			// Problem widget in dashboard.
			$this->page->open('zabbix.php?action=dashboard.view&dashboardid=1')->waitUntilReady();
			$widget_row = $this->query('xpath:(//table[@class="list-table"])[10]')->asTable()->one()->findRow('Time', '2020-10-23 18:23:48');
			$this->assertTrue($widget_row->query('xpath://*[text()="No"]')->one()->isClickable($action_status));

			// Event details page.
			$this->page->open('tr_events.php?triggerid=99251&eventid=93')->waitUntilReady();
			foreach (['2', '5'] as $table_number) {
				$table = $this->query('xpath:(//*[@class="list-table"])['.$table_number.']')->asTable()->one();
				$this->assertTrue($table->query('xpath://*[text()="No"]')->one()->isClickable($action_status));
			}

			// Overview page.
			$this->page->open('overview.php?type=0')->waitUntilReady();
			$overview_table = $this->query('class:list-table')->asTable()->one();
			$overview_table->query('xpath://td[@class="disaster-bg cursor-pointer"]')->one()->click();
			$this->page->waitUntilReady();
			$popup = CPopupMenuElement::find()->waitUntilVisible()->one();
			if ($action_status === true) {
				$this->assertTrue($popup->hasItems($context_before));
				$this->changeAction($actions);
			}
			else {
				$context_after = array_values(array_diff($context_before, ['Acknowledge']));
				$this->assertTrue($popup->hasItems($context_after));
			}
		}
	}

	public static function getScriptActionData() {
		return [
			// Monitoring problems page.
			[
				[
					'link' => 'zabbix.php?action=problem.view',
					'selector' => 'xpath:(//a[@class="link-action" and text()="ЗАББИКС Сервер"])[1]'
				]
			],
			// Dashboard problem widget.
			[
				[
					'link' => 'zabbix.php?action=dashboard.view&dashboardid=1',
					'selector' => 'link:ЗАББИКС Сервер'
				]
			],
			// Monitoring hosts page.
			[
				[
					'link' => 'zabbix.php?action=host.view',
					'selector' => 'link:3_Host_to_check_Monitoring_Overview'
				]
			],
			// Event detail page.
			[
				[
					'link' => 'tr_events.php?triggerid=99251&eventid=93',
					'selector' => 'xpath:(//*[@class="list-table"])[1]//*[text()="ЗАББИКС Сервер"]'
				]
			],
			// Monitoring maps page.
			[
				[
					'link' => 'zabbix.php?action=map.view&sysmapid=1',
					'selector' => 'xpath://*[name()="g"][@class="map-elements"]/*[name()="image"]'
				]
			]
		];
	}

	/**
	 * Check script actions.
	 *
	 * @dataProvider getScriptActionData
	 */
	public function testUserRolesPermissions_ScriptAction($data) {
		$context_before = [
			'Inventory',
			'Latest data',
			'Problems',
			'Graphs',
			'Dashboards',
			'Web',
			'Configuration',
			'Detect operating system',
			'Ping',
			'Script for Clone',
			'Script for Delete',
			'Script for Update',
			'Selenium script',
			'Traceroute'
		];
		$context_after = [
			'Inventory',
			'Latest data',
			'Problems',
			'Graphs',
			'Dashboards',
			'Web',
			'Configuration'
		];
		$this->page->login();
		$this->page->userLogin('user_for_role', 'zabbix');
		foreach ([true, false] as $action_status) {
			$this->page->open($data['link'])->waitUntilReady();
			$this->query($data['selector'])->waitUntilPresent()->one()->click();
			$popup = CPopupMenuElement::find()->waitUntilVisible()->one();
			if ($action_status === true) {
				$this->assertTrue($popup->hasItems($context_before));
				$this->assertEquals(['HOST', 'SCRIPTS'], $popup->getTitles()->asText());
				$this->changeAction(['Execute scripts' => false]);
			}
			else {
				$this->assertTrue($popup->hasItems($context_after));
				$this->assertEquals(['HOST'], $popup->getTitles()->asText());
				$this->changeAction(['Execute scripts' => true]);
			}
		}
	}

	/**
	 * Module enable/disable.
	 */
	public function testUserRolesPermissions_Module() {
		$pages_before = [
			'Monitoring',
			'Inventory',
			'Reports',
			'Configuration',
			'Administration',
			'Module 5 menu'
		];
		$this->page->login();
		$this->page->userLogin('user_for_role', 'zabbix');
		$this->page->open('zabbix.php?action=module.list')->waitUntilReady();
		$this->query('button:Scan directory')->one()->click();
		$table = $this->query('class:list-table')->asTable()->one();
		$table->findRows(['Name' => '5th Module'])->select();
		$this->query('button:Enable')->one()->click();
		$this->page->acceptAlert();
		$this->page->waitUntilReady();
		foreach ([true, false] as $action_status) {
			$page_number = $this->query('xpath://ul[@class="menu-main"]/li/a')->count();
			for ($i = 1; $i <= $page_number; ++$i) {
				$all_pages[] = $this->query('xpath:(//ul[@class="menu-main"]/li/a)['.$i.']')->one()->getText();
			}
			if ($action_status === true) {
				$this->assertEquals($pages_before, $all_pages);
				$this->changeAction(['5th Module' => false]);
				$all_pages = [];
			}
			else {
				$pages_after = array_values(array_diff($pages_before, ['Module 5 menu']));
				$this->assertEquals($pages_after, $all_pages);
				$this->changeAction(['5th Module' => true]);
			}
		}
	}

	public static function getUIData() {
		return [
			[
				[
					'section' => 'Inventory',
					'page' => 'Overview',
					'remove_ui' => [
						'Inventory' => [
							'Hosts'
						]
					],
					'link' => ['hostinventoriesoverview.php']
				]
			],
			[
				[
					'section' => 'Inventory',
					'page' => 'Hosts',
					'remove_ui' => [
						'Inventory' => [
							'Overview'
						]
					],
					'link' => ['hostinventories.php']
				]
			],
			[
				[
					'section' => 'Reports',
					'page' => 'System information',
					'remove_ui' => [
						'Reports' => [
							'Availability report',
							'Triggers top 100',
							'Audit',
							'Action log',
							'Notifications'
						]
					],
					'link' => ['zabbix.php?action=report.status']
				]
			],
			[
				[
					'section' => 'Reports',
					'page' => 'Availability report',
					'remove_ui' => [
						'Reports' => [
							'System information',
							'Triggers top 100',
							'Audit',
							'Action log',
							'Notifications'
						]
					],
					'link' => ['report2.php']
				]
			],
			[
				[
					'section' => 'Reports',
					'page' => 'Triggers top 100',
					'remove_ui' => [
						'Reports' => [
							'Availability report',
							'System information',
							'Audit',
							'Action log',
							'Notifications'
						]
					],
					'link' => ['toptriggers.php']
				]
			],
			[
				[
					'section' => 'Reports',
					'page' => 'Audit',
					'remove_ui' => [
						'Reports' => [
							'Availability report',
							'System information',
							'Triggers top 100',
							'Action log',
							'Notifications'
						]
					],
					'link' => ['zabbix.php?action=auditlog.list']
				]
			],
			[
				[
					'section' => 'Reports',
					'page' => 'Action log',
					'remove_ui' => [
						'Reports' => [
							'Availability report',
							'System information',
							'Triggers top 100',
							'Audit',
							'Notifications'
						]
					],
					'link' => ['auditacts.php']
				]
			],
			[
				[
					'section' => 'Reports',
					'page' => 'Notifications',
					'remove_ui' => [
						'Reports' => [
							'Availability report',
							'System information',
							'Triggers top 100',
							'Audit',
							'Action log'
						]
					],
					'link' => ['report4.php']
				]
			],
			[
				[
					'section' => 'Configuration',
					'page' => 'Host groups',
					'remove_ui' => [
						'Configuration' => [
							'Templates',
							'Hosts',
							'Maintenance',
							'Actions',
							'Event correlation',
							'Discovery',
							'Services'
						]
					],
					'link' => ['hostgroups.php']
				]
			],
			[
				[
					'section' => 'Configuration',
					'page' => 'Templates',
					'remove_ui' => [
						'Configuration' => [
							'Host groups',
							'Hosts',
							'Maintenance',
							'Actions',
							'Event correlation',
							'Discovery',
							'Services'
						]
					],
					'link' => ['templates.php']
				]
			],
			[
				[
					'section' => 'Configuration',
					'page' => 'Hosts',
					'remove_ui' => [
						'Configuration' => [
							'Host groups',
							'Templates',
							'Maintenance',
							'Actions',
							'Event correlation',
							'Discovery',
							'Services'
						]
					],
					'link' => ['hosts.php']
				]
			],
			[
				[
					'section' => 'Configuration',
					'page' => 'Maintenance',
					'remove_ui' => [
						'Configuration' => [
							'Host groups',
							'Templates',
							'Hosts',
							'Actions',
							'Event correlation',
							'Discovery',
							'Services'
						]
					],
					'link' => ['maintenance.php']
				]
			],
			[
				[
					'section' => 'Configuration',
					'page' => 'Actions',
					'remove_ui' => [
						'Configuration' => [
							'Host groups',
							'Templates',
							'Hosts',
							'Maintenance',
							'Event correlation',
							'Discovery',
							'Services'
						]
					],
					'link' => ['actionconf.php']
				]
			],
			[
				[
					'section' => 'Configuration',
					'page' => 'Event correlation',
					'remove_ui' => [
						'Configuration' => [
							'Host groups',
							'Templates',
							'Hosts',
							'Maintenance',
							'Actions',
							'Discovery',
							'Services'
						]
					],
					'link' => ['correlation.php']
				]
			],
			[
				[
					'section' => 'Configuration',
					'page' => 'Discovery',
					'remove_ui' => [
						'Configuration' => [
							'Host groups',
							'Templates',
							'Hosts',
							'Maintenance',
							'Actions',
							'Event correlation',
							'Services'
						]
					],
					'link' => ['discoveryconf.php']
				]
			],
			[
				[
					'section' => 'Configuration',
					'page' => 'Services',
					'remove_ui' => [
						'Configuration' => [
							'Host groups',
							'Templates',
							'Hosts',
							'Maintenance',
							'Actions',
							'Event correlation',
							'Discovery'
						]
					],
					'link' => ['services.php']
				]
			],
			[
				[
					'section' => 'Administration',
					'page' => 'General',
					'remove_ui' => [
						'Administration' => [
							'Proxies',
							'Authentication',
							'User groups',
							'User roles',
							'Users',
							'Media types',
							'Scripts',
							'Queue'
						]
					],
					'link' => [
						'zabbix.php?action=gui.edit',
						'zabbix.php?action=autoreg.edit',
						'zabbix.php?action=housekeeping.edit',
						'zabbix.php?action=image.list',
						'zabbix.php?action=iconmap.list',
						'zabbix.php?action=regex.list',
						'zabbix.php?action=macros.edit',
						'zabbix.php?action=valuemap.list',
						'zabbix.php?action=trigdisplay.edit',
						'zabbix.php?action=module.list',
						'zabbix.php?action=miscconfig.edit'
					]
				]
			],
			[
				[
					'section' => 'Administration',
					'page' => 'Proxies',
					'remove_ui' => [
						'Administration' => [
							'General',
							'Authentication',
							'User groups',
							'User roles',
							'Users',
							'Media types',
							'Scripts',
							'Queue'
						]
					],
					'link' => ['zabbix.php?action=proxy.list']
				]
			],
			[
				[
					'section' => 'Administration',
					'page' => 'Authentication',
					'remove_ui' => [
						'Administration' => [
							'General',
							'Proxies',
							'User groups',
							'User roles',
							'Users',
							'Media types',
							'Scripts',
							'Queue'
						]
					],
					'link' => ['zabbix.php?action=authentication.edit']
				]
			],
			[
				[
					'section' => 'Administration',
					'page' => 'User groups',
					'remove_ui' => [
						'Administration' => [
							'General',
							'Proxies',
							'Authentication',
							'User roles',
							'Users',
							'Media types',
							'Scripts',
							'Queue'
						]
					],
					'link' => ['zabbix.php?action=usergroup.list']
				]
			],
			[
				[
					'section' => 'Administration',
					'page' => 'Users',
					'remove_ui' => [
						'Administration' => [
							'General',
							'Proxies',
							'Authentication',
							'User roles',
							'User groups',
							'Media types',
							'Scripts',
							'Queue'
						]
					],
					'link' => ['zabbix.php?action=user.list']
				]
			],
			[
				[
					'section' => 'Administration',
					'page' => 'Media types',
					'remove_ui' => [
						'Administration' => [
							'General',
							'Proxies',
							'Authentication',
							'User roles',
							'User groups',
							'Users',
							'Scripts',
							'Queue'
						]
					],
					'link' => ['zabbix.php?action=mediatype.list']
				]
			],
			[
				[
					'section' => 'Administration',
					'page' => 'Scripts',
					'remove_ui' => [
						'Administration' => [
							'General',
							'Proxies',
							'Authentication',
							'User roles',
							'User groups',
							'Users',
							'Media types',
							'Queue'
						]
					],
					'link' => ['zabbix.php?action=script.list']
				]
			],
			[
				[
					'section' => 'Administration',
					'page' => 'Queue',
					'remove_ui' => [
						'Administration' => [
							'General',
							'Proxies',
							'Authentication',
							'User roles',
							'User groups',
							'Users',
							'Media types',
							'Scripts'
						]
					],
					'link' => ['queue.php']
				]
			],
			[
				[
					'section' => 'Administration',
					'user_roles' => true,
					'page' => 'User roles',
					'remove_ui' => [
						'Administration' => [
							'General',
							'Proxies',
							'Authentication',
							'Queue',
							'User groups',
							'Users',
							'Media types',
							'Scripts'
						]
					],
					'link' => ['zabbix.php?action=userrole.list']
				]
			],
			[
				[
					'section' => 'Monitoring',
					'page' => 'Problems',
					'remove_ui' => [
						'Monitoring' => [
							'Dashboard',
							'Hosts',
							'Overview',
							'Latest data',
							'Screens',
							'Maps',
							'Discovery',
							'Services'
						]
					],
					'link' => ['zabbix.php?action=problem.view']
				]
			],
			[
				[
					'section' => 'Monitoring',
					'page' => 'Hosts',
					'remove_ui' => [
						'Monitoring' => [
							'Dashboard',
							'Problems',
							'Overview',
							'Latest data',
							'Screens',
							'Maps',
							'Discovery',
							'Services'
						]
					],
					'link' => ['zabbix.php?action=host.view']
				]
			],
			[
				[
					'section' => 'Monitoring',
					'page' => 'Overview',
					'remove_ui' => [
						'Monitoring' => [
							'Dashboard',
							'Problems',
							'Hosts',
							'Latest data',
							'Screens',
							'Maps',
							'Discovery',
							'Services'
						]
					],
					'link' => ['overview.php']
				]
			],
			[
				[
					'section' => 'Monitoring',
					'page' => 'Latest data',
					'remove_ui' => [
						'Monitoring' => [
							'Dashboard',
							'Problems',
							'Hosts',
							'Overview',
							'Screens',
							'Maps',
							'Discovery',
							'Services'
						]
					],
					'link' => ['zabbix.php?action=latest.view']
				]
			],
			[
				[
					'section' => 'Monitoring',
					'page' => 'Screens',
					'remove_ui' => [
						'Monitoring' => [
							'Dashboard',
							'Problems',
							'Hosts',
							'Overview',
							'Latest data',
							'Maps',
							'Discovery',
							'Services'
						]
					],
					'link' => ['screenconf.php']
				]
			],
			[
				[
					'section' => 'Monitoring',
					'page' => 'Maps',
					'remove_ui' => [
						'Monitoring' => [
							'Dashboard',
							'Problems',
							'Hosts',
							'Overview',
							'Latest data',
							'Screens',
							'Discovery',
							'Services'
						]
					],
					'link' => ['sysmaps.php']
				]
			],
			[
				[
					'section' => 'Monitoring',
					'page' => 'Discovery',
					'remove_ui' => [
						'Monitoring' => [
							'Dashboard',
							'Problems',
							'Hosts',
							'Overview',
							'Latest data',
							'Screens',
							'Maps',
							'Services'
						]
					],
					'link' => ['zabbix.php?action=discovery.view']
				]
			],
			[
				[
					'section' => 'Monitoring',
					'page' => 'Services',
					'remove_ui' => [
						'Monitoring' => [
							'Dashboard',
							'Problems',
							'Hosts',
							'Overview',
							'Latest data',
							'Screens',
							'Maps',
							'Discovery'
						]
					],
					'link' => ['srv_status.php']
				]
			]
		];
	}

	/**
	 * UI permission
	 *
	 * @dataProvider getUIData
	 */
	public function testUserRolesPermissions_UI($data) {
		$user_roles = [
			'Administration' => [
				'General',
				'Proxies',
				'Authentication',
				'User roles',
				'User groups',
				'Users',
				'Media types',
				'Scripts'
			]
		];
		$this->page->userLogin('user_for_role', 'zabbix');
		foreach ([true, false] as $action_status) {
			$main_section = $this->query('xpath://ul[@class="menu-main"]')->query('link', $data['section']);
			if ($data['section'] !== 'Monitoring') {
				$main_section->waitUntilReady()->one()->click();
				$element = $this->query('xpath://a[text()="'.$data['section'].'"]/../ul[@class="submenu"]')->one();
				CElementQuery::wait()->until(function () use ($element) {
				return CElementQuery::getDriver()->executeScript('return arguments[0].clientHeight ==='.
						' parseInt(arguments[0].style.maxHeight, 10)', [$element]);
				});
			}
			$this->assertEquals($action_status, $main_section->one()->parents('tag:li')->query('link', $data['page'])->exists());
			if ($action_status === true) {
				if (array_key_exists('user_roles', $data)) {
					$this->clickSignout();
					$this->page->userLogin('Admin', 'zabbix');
					$this->changeAction($data['remove_ui']);
					$this->clickSignout();
					$this->page->userLogin('user_for_role', 'zabbix');
				}
				else {
					$this->changeAction($data['remove_ui']);
					$this->page->open('zabbix.php?action=dashboard.view')->waitUntilReady();
				}
			}
			else {
				if (array_key_exists('user_roles', $data)) {
					$this->checkLinks($data['link']);
					$this->clickSignout();
					$this->page->userLogin('Admin', 'zabbix');
					$this->changeAction($user_roles);
					$this->clickSignout();
				}
				else {
					$this->checkLinks($data['link']);
					$this->clickSignout();
				}
			}
		}
	}

	public static function getDashboardData() {
		return [
			[
				[

					'page' => 'Dashboard',
					'button' => 'Problems',
					'remove_ui' => [
						'Monitoring' => [
							'Problems',
							'Hosts',
							'Overview',
							'Latest data',
							'Screens',
							'Maps',
							'Discovery',
							'Services'
						]
					]
				]
			],
			[
				[
					'button' => 'Hosts',
					'remove_ui' => [
						'Monitoring' => [
							'Hosts',
							'Overview',
							'Latest data',
							'Screens',
							'Maps',
							'Discovery',
							'Services'
						]
					]
				]
			],
			[
				[
					'button' => 'Overview',
					'remove_ui' => [
						'Monitoring' => [
							'Overview',
							'Latest data',
							'Screens',
							'Maps',
							'Discovery',
							'Services'
						]
					]
				]
			]
		];
	}

	/**
	 * Disabling access to Dashboard. Check warning message text and button.
	 *
	 * @dataProvider getDashboardData
	 */
	public function testUserRolesPermissions_Dashboard($data) {
		$enabled_ui = [
			'Monitoring' => [
				'Problems',
				'Hosts',
				'Overview',
				'Latest data',
				'Screens',
				'Maps',
				'Discovery',
				'Services'
			]
		];
		$this->page->login();
		$this->page->userLogin('user_for_role', 'zabbix');
		foreach ([true, false] as $action_status) {
			$main_section = $this->query('xpath://ul[@class="menu-main"]')->query('link:Monitoring');
			if (array_key_exists('page', $data)) {
				$this->assertEquals($action_status, $main_section->one()->parents('tag:li')->query('link', $data['page'])->exists());
			}
			if ($action_status === true) {
				$this->changeAction($data['remove_ui']);
			}
			else {
				$this->checkLinks(['zabbix.php?action=dashboard.view'], $data['button']);
				$this->changeAction($enabled_ui);
			}
		}
	}

	/**
	 * Check disabled actions with links.
	 *
	 * @param array $links	checked links after disabling action
	 */
	private function checkLinks($links, $page = 'Dashboard') {
		foreach ($links as $link) {
			$this->page->open($link)->waitUntilReady();
			$this->assertMessage(TEST_BAD, 'Access denied', 'You are logged in as "user_for_role". '.
					'You have no permissions to access this page.');
			$this->query('button:Go to "'.$page.'"')->one()->waitUntilClickable()->click();
			if ($page === 'Dashboard') {
				$this->assertContains('zabbix.php?action=dashboard', $this->page->getCurrentUrl());
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

	private function clickSignout() {
		$this->query('xpath://a[@class="icon-signout"]')->waitUntilPresent()->one()->click();
		$this->query('button:Sign in')->waitUntilVisible()->one();
	}
}
