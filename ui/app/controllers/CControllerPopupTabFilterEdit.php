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


class CControllerPopupTabFilterEdit extends CController {

	protected function init() {
		$this->disableSIDValidation();
	}

	protected function checkInput() {
		$rules = [
			'idx' =>					'string|required',
			'idx2' =>					'int32|required',
			'form_action' =>			'string|in update,delete',
			'filter_name' =>			'string|not_empty',
			'filter_show_counter' =>	'in 0,1',
			'filter_custom_time' =>		'in 0,1',
			'tabfilter_from' =>			'string',
			'tabfilter_to' =>			'string',
			'support_custom_time' =>	'in 0,1',
			'create' =>					'in 0,1'
		];

		$ret = $this->validateInput($rules) && $this->customValidation();

		if (!$ret) {
			$output = [];

			if (($messages = getMessages()) !== null) {
				$output['errors'] = $messages->toString();
			}

			$this->setResponse(
				(new CControllerResponseData(['main_block' => json_encode($output)]))->disableView()
			);
		}

		return $ret;
	}

	protected function customValidation(): bool {
		$ret = true;
		$rules = [];
		$input = $this->getInputAll();

		if ($this->getInput('filter_custom_time', 0) && $this->getInput('form_action', '') === 'update') {
			$rules = [
				'tabfilter_from' =>		'range_time|required',
				'tabfilter_to' =>		'range_time|required',
			];

			$validator = new CNewValidator($input, $rules);
			$ret = $ret && !$validator->isError() && !$validator->isErrorFatal() && $this->validateTimeSelectorPeriod();

			foreach ($validator->getAllErrors() as $error) {
				info($error);
			}

			if ($ret) {
				$this->input += [
					'from' => $this->input['tabfilter_from'],
					'to' => $this->input['tabfilter_to']
				];
				$ret = $this->validateTimeSelectorPeriod();
			}
		}

		return $ret;
	}

	protected function checkPermissions() {
		return true;
	}

	protected function doAction() {
		$data = [
			'form_action' => '',
			'idx' => '',
			'idx2' => '',
			'filter_name' => '',
			'filter_show_counter' => 0,
			'filter_custom_time' => 0,
			'tabfilter_from' => '',
			'tabfilter_to' => '',
			'support_custom_time' => 0,
			'create' => 0
		];
		$this->getInputs($data, array_keys($data));

		if (!$this->getInput('support_custom_time', 0)) {
			$data = [
				'filter_custom_time' => 0,
				'tabfilter_from' => '',
				'tabfilter_to' => '',
			] + $data;
		}

		if ($data['form_action'] === 'update') {
			$this->updateTab($data);
		}
		elseif ($data['form_action'] === 'delete') {
			$this->deleteTab($data);
		}
		else {
			$data += [
				'action' => $this->getAction(),
				'title' => _('Filter properties'),
				'errors' => hasErrorMesssages() ? getMessages() : null,
				'user' => [
					'debug_mode' => $this->getDebugMode()
				]
			];

			$this->setResponse(new CControllerResponseData($data));
		}

	}

	/**
	 * Update tab filter profile data.
	 *
	 * @param array $data  Filter properties.
	 */
	public function updateTab(array $data) {
		$filter = (new CTabFilterProfile($data['idx'], []))->read();
		$data['create'] = (int) $data['create'];
		$properties = [
			'filter_name' => $data['filter_name'],
			'filter_show_counter' => (int) $data['filter_show_counter'],
			'filter_custom_time' => (int) $data['filter_custom_time'],
			'from' => $data['tabfilter_from'],
			'to' => $data['tabfilter_to']
		];

		if (!$properties['filter_custom_time']) {
			unset($properties['from'], $properties['to']);
		}

		if (array_key_exists($data['idx2'], $filter->tabfilters)) {
			$properties = $properties + $filter->tabfilters[$data['idx2']];
		}
		else {
			$data['idx2'] = count($filter->tabfilters);
		}

		$filter->tabfilters[$data['idx2']] = $properties;
		$filter->update();

		$this->setResponse((new CControllerResponseData(['main_block' => json_encode($data)]))->disableView());
	}

	/**
	 * Delete tab filter with index idx2
	 */
	public function deleteTab(array $data) {
		(new CTabFilterProfile($data['idx'], []))
			->read()
			->deleteTab((int) $data['idx2'])
			->update();

		$this->setResponse((new CControllerResponseData(['main_block' => json_encode($data)]))->disableView());
	}
}