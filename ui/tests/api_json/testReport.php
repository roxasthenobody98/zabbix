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

class testReport extends CAPITest {

	public static function reportGet_data() {
		$reportsids = ['15', '16', '17', '18', '21'];

		return [
			[
				'params' => [
					'output' => 'extend'
				],
				'expect' => [
					'error' => null,
					'reportid' => $reportsids
				]
			],
			[
				'params' => [
					'output' => ['reportid']
				],
				'expect' => [
					'error' => null,
					'reportid' => $reportsids
				]
			],
		];
	}

	/**
	 * @dataProvider reportGet_data
	 */
	public function testReportGet_call($params, $expect) {
		$result = $this->call('report.get', $params, $expect['error']);
		if ($expect['error'] !== null) {
			return;
		}

		$reportid = array_column($result['result'], 'reportid');
		sort($reportid);
		sort($expect['reportid']);
		$this->assertSame($expect['reportid'], $reportid);
	}


	public static function reportCreate_data() {
		return [
			[
				'params' => [
					'name' => 'API test with minimum params',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					]
				],
				'expected_error' => null
			],
			[
				'params' => [],
				'expected_error' => 'Invalid parameter "/": cannot be empty.'
			],
			[
				'params' => [
					'name' => ''
				],
				'expected_error' => 'Invalid parameter "/1/name": cannot be empty.'
			],
			[
				'params' => [
					'name' => 1
				],
				'expected_error' => 'Invalid parameter "/1/name": a character string is expected.'
			],
			[
				'params' => [
					'name' => []
				],
				'expected_error' => 'Invalid parameter "/1/name": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name'
				],
				'expected_error' => 'Invalid parameter "/1": the parameter "dashboardid" is missing.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => ''
				],
				'expected_error' => 'Invalid parameter "/1/dashboardid": a number is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => 'string'
				],
				'expected_error' => 'Invalid parameter "/1/dashboardid": a number is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => []
				],
				'expected_error' => 'Invalid parameter "/1/dashboardid": a number is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1'
				],
				'expected_error' => 'At least one user or user group must be specified.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'period' => 4
				],
				'expected_error' => 'Invalid parameter "/1/period": value must be one of 0, 1, 2, 3.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'period' => 'string'
				],
				'expected_error' => 'Invalid parameter "/1/period": an integer is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'cycle' => 4
				],
				'expected_error' => 'Invalid parameter "/1/cycle": value must be one of 0, 1, 2, 3.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'cycle' => "string"
				],
				'expected_error' => 'Invalid parameter "/1/cycle": an integer is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'start_time' => '86341'
				],
				'expected_error' => 'Invalid parameter "/1/start_time": value must be one of 0-86340.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'start_time' => ''
				],
				'expected_error' => 'Invalid parameter "/1/start_time": an integer is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'weekdays' => 1
				],
				'expected_error' => 'Invalid parameter "/1/weekdays": value must be 0.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'cycle' => 1,
					'weekdays' => 0
				],
				'expected_error' => 'Invalid parameter "/1/weekdays": value must be one of 1-127.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'cycle' => 1,
					'weekdays' => 'string'
				],
				'expected_error' => 'Invalid parameter "/1/weekdays": an integer is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'cycle' => 1,
					'weekdays' => ''
				],
				'expected_error' => 'Invalid parameter "/1/weekdays": an integer is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'active_since' => 1
				],
				'expected_error' => 'Invalid parameter "/1/active_since": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'active_since' => 'string'
				],
				'expected_error' => 'Invalid parameter "/1/active_since": a date in YYYY-MM-DD format is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'active_since' => '1950-01-01'
				],
				'expected_error' => 'Invalid parameter "/1/active_since": value must be between "1970-01-01" and "2038-01-18".'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'active_since' => '2039-01-18'
				],
				'expected_error' => 'Invalid parameter "/1/active_since": value must be between "1970-01-01" and "2038-01-18".'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'active_till' => 1
				],
				'expected_error' => 'Invalid parameter "/1/active_till": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'active_till' => 'string'
				],
				'expected_error' => 'Invalid parameter "/1/active_till": a date in YYYY-MM-DD format is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'active_till' => '1950-01-01'
				],
				'expected_error' => 'Invalid parameter "/1/active_till": value must be between "1970-01-01" and "2038-01-18".'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'active_till' => '2039-01-18'
				],
				'expected_error' => 'Invalid parameter "/1/active_till": value must be between "1970-01-01" and "2038-01-18".'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'subject' => 1
				],
				'expected_error' => 'Invalid parameter "/1/subject": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'subject' => []
				],
				'expected_error' => 'Invalid parameter "/1/subject": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'message' => 1
				],
				'expected_error' => 'Invalid parameter "/1/message": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'message' => []
				],
				'expected_error' => 'Invalid parameter "/1/message": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'status' => 3
				],
				'expected_error' => 'Invalid parameter "/1/status": value must be one of 1, 0.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'status' => 'string'
				],
				'expected_error' => 'Invalid parameter "/1/status": an integer is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'status' => []
				],
				'expected_error' => 'Invalid parameter "/1/status": an integer is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'status' => ''
				],
				'expected_error' => 'Invalid parameter "/1/status": an integer is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'description' => 1
				],
				'expected_error' => 'Invalid parameter "/1/description": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'description' => []
				],
				'expected_error' => 'Invalid parameter "/1/description": a character string is expected.'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'state' => 1
				],
				'expected_error' => 'Invalid parameter "/1": unexpected parameter "state".'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'lastsent' => 1
				],
				'expected_error' => 'Invalid parameter "/1": unexpected parameter "lastsent".'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '1',
							'access_userid' => '1',
							'exclude' => '0'
						]
					],
					'info' => 1
				],
				'expected_error' => 'Invalid parameter "/1": unexpected parameter "info".'
			],
			[
				'params' => [
					'name' => 'valid name',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => 'string'
						]
					],
				],
				'expected_error' => 'Invalid parameter "/1/users/1/userid": a number is expected.'
			],
			[
				'params' => [
					'name' => 'valid name report',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '8889',
						]
					],
				],
				'expected_error' => 'User with ID "8889" is not available.'
			],
			[
				'params' => [
					'name' => 'valid name report',
					'dashboardid' => '1',
					'users' => [
						[
							'userid' => '8889',
							'exclude' => 3
						]
					],
				],
				'expected_error' => 'Invalid parameter "/1/users/1/exclude": value must be one of 0, 1.'
			]
		];
	}

	/**
	 * @dataProvider reportCreate_data
	 */
	public function testReportCreate_call($params, $expected_error) {
		$this->call('report.create', $params, $expected_error);
	}


	public static function reportUpdate_data() {
		return [
			[
				'params' => [
					'reportid' => '15',
					'name' => 'new report name',
					'users' => [
						'userid' => '17',
						'access_userid' => '1',
						'exclude' => '0'
					]
				],
				'expect' => [
					'error' => null,
					'reportid' => [15]
				]
			],
		];
	}

	/**
	 * @dataProvider reportUpdate_data
	 */
	public function testReportUpdate_call($params, $expect) {
		$result = $this->call('report.update', $params, $expect);
		if ($expect['error'] !== null) {
			return;
		}

		$this->assertSame($expect['reportid'], $result['result']['reportids']);
	}

	public static function reportDelete_data() {
		return [
			[
				'params' => [16],
				'expected_error' => null
			],
			[
				'params' => ['string'],
				'expected_error' => 'Invalid parameter "/1": a number is expected.'
			],
			[
				'params' => [[]],
				'expected_error' => 'Invalid parameter "/1": a number is expected.'
			],
			[
				'params' => [],
				'expected_error' => 'Invalid parameter "/": cannot be empty.'
			],
			[
				'params' => ['888'],
				'expected_error' => 'No permissions to referred object or it does not exist!'
			]
		];
	}

	/**
	 * @dataProvider reportDelete_data
	 */
	public function testReportDelete_call($params, $expected_error) {
		$this->call('report.delete', $params, $expected_error);
	}

	public static function reportUserDelete_data() {
		return [
			[
				'params' => [17],
				'expected_error' => 'User "Report user user" is report "Test report one subscriptions" recipient.'
			],
		];
	}

	/**
	 * @dataProvider reportUserDelete_data
	 */
	public function testReportUserDelete_call($params, $expected_error) {
		$this->call('user.delete', $params, $expected_error);
	}

	public static function reportUserGroupDelete_data() {
		return [
			[
				'params' => [25],
				// 'expected_error' => 'User group "Reports group 1" is report "Report test remove group" recipient.'
				'expected_error' => 'No permissions to referred object or it does not exist!'
			],
		];
	}

	/**
	 * @dataProvider reportUserGroupDelete_data
	 */
	public function testReportUserGroupDelete_call($params, $expected_error) {
		$this->call('user.delete', $params, $expected_error);
	}
}
