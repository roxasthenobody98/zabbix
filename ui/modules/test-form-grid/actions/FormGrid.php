<?php declare(strict_types = 1);

namespace Modules\FormGrid\Actions;

use CController as CAction;

class FormGrid extends CAction {

	public function init() {
		$this->disableSIDvalidation();
	}

	/**
	 * @inheritDoc
	 */
	protected function checkPermissions() {
		return true;
	}

	/**
	 * @inheritDoc
	 */
	protected function checkInput() {
		return true;
	}

	/**
	 * @inheritDoc
	 */
	protected function doAction() {
		$response = new \CControllerResponseData([]);
		$response->setTitle('Test');

		$this->setResponse($response);
	}
}
