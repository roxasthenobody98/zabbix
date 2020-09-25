<?php declare(strict_types = 1);

namespace Modules\FormGrid;

use Core\CModule,
	APP,
	CMenu;

class Module extends CModule {

	/**
	 * Initialize module.
	 */
	public function init(): void {
		$menu = APP::Component()->get('menu.main');

		$menu
			->findOrAdd(_('Form grid'))
			->setIcon('icon-share')
			->getSubmenu()
			->add(
				(new \CMenuItem('Item edit'))->setAction('form.grid.item')
			)
			->insertAfter('',
				(new \CMenuItem('Item test'))->setAction('form.grid.item.test')
			);
	}
}
