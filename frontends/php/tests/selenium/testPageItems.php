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

require_once dirname(__FILE__).'/../include/CLegacyWebTest.php';

class testPageItems extends CLegacyWebTest {
	public static function data() {
		return CDBHelper::getDataProvider(
			'SELECT hostid,status'.
			' FROM hosts'.
			' WHERE host LIKE \'%-layout-test%\''
		);
	}

	/**
	* @dataProvider data
	*/
	public function testPageItems_CheckLayout($data) {
		$this->zbxTestLogin('items.php?filter_set=1&groupid=0&hostid='.$data['hostid']);
		$this->zbxTestCheckTitle('Configuration of items');
		$this->zbxTestCheckHeader('Items');
		$this->zbxTestTextPresent('Displaying');

		if ($data['status'] == HOST_STATUS_MONITORED || $data['status'] == HOST_STATUS_NOT_MONITORED) {
			$this->zbxTestTextPresent('All hosts');
			$this->zbxTestTextPresent(
				[
					'Wizard',
					'Name',
					'Triggers',
					'Key',
					'Interval',
					'History',
					'Trends',
					'Type',
					'Applications',
					'Status',
					'Info'
				]
			);
		}
		elseif ($data['status'] == HOST_STATUS_TEMPLATE) {
			$this->zbxTestTextPresent('All templates');
			$this->zbxTestTextPresent(
				[
					'Wizard',
					'Name',
					'Triggers',
					'Key',
					'Interval',
					'History',
					'Trends',
					'Type',
					'Applications',
					'Status',
					'Info'
				]
			);
		}

		$this->zbxTestAssertElementText("//button[@value='item.masscheck_now'][@disabled]", 'Check now');

		// TODO someday should check that interval is not shown for trapper items, trends not shown for non-numeric items etc
		$this->zbxTestTextPresent('Enable', 'Disable', 'Mass update', 'Copy', 'Clear history', 'Delete');
	}

	/**
	 * @dataProvider data
	 */
	public function testPageItems_CheckNowAll($data) {
		$this->zbxTestLogin('items.php?filter_set=1&groupid=0&hostid='.$data['hostid']);
		$this->zbxTestCheckHeader('Items');

		$this->zbxTestClick('all_items');

		if ($data['status'] == HOST_STATUS_TEMPLATE) {
			$this->assertFalse($this->query('button:Check now')->one()->isEnabled());
			$this->assertFalse($this->query('button:Clear history')->one()->isEnabled());
		}
		else {
			$this->zbxTestClickButtonText('Check now');
			$this->zbxTestWaitUntilMessageTextPresent('msg-good', 'Request sent successfully');
		}
	}
}
