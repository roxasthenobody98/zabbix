<?php
/*
** Zabbix
** Copyright (C) 2001-2020 Zabbix SIA
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


class CControllerProblemViewRefresh extends CControllerProblemView {

	protected function doAction() {
		if ($this->getInput('filter_counters', 0)) {
			$profile = (new CTabFilterProfile(static::FILTER_IDX, static::FILTER_FIELDS_DEFAULT))->read();
			$show_counters = [];

			foreach ($profile->getTabsWithDefaults() as $index => $tabfilter) {
				if (!$tabfilter['filter_custom_time']) {
					$tabfilter = [
						'from' => $profile->from,
						'to' => $profile->to
					] + $tabfilter;
				}

				$show_counters[$index] = $tabfilter['filter_show_counter'] ? $this->getCount($tabfilter) : 0;
			}

			$data['filter_counters'] = $show_counters;
		}

		$response = new CControllerResponseData($data);
		$this->setResponse($response);
	}
}