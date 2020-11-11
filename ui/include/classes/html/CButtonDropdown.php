<?php declare(strict_types = 1);
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


class CButtonDropdown extends CButton {

	/**
	 * Default CSS class name for HTML root element.
	 */
	public const ZBX_STYLE_CLASS = 'btn-dropdown-container';

	/**
	 * Button style names.
	 */
	public const ZBX_STYLE_BTN_TOGGLE = 'btn-dropdown-toggle';
	public const ZBX_STYLE_BTN_VALUE = 'dropdown-value';

	/**
	 * Data for dropdown menu.
	 *
	 * @var array
	 */
	public $menu_data = [];

	/**
	 * Create CButtonDropdown instance.
	 *
	 * @param string $name                         Element name.
	 * @param string $value                        Element selected value.
	 * @param array  $menu_data                    Data for dropdown menu.
	 * @param string $menu_data[submit_form]       Enables form submit behaviour if set.
	 * @param string $menu_data[items]             Dropdown items.
	 * @param string $menu_data[items][]['label']  Dropdown item label.
	 * @param string $menu_data[items][]['value']  Dropdown item value.
	 * @param string $menu_data[items][]['url']    Dropdown item url.
	 * @param string $menu_data[items][]['class']  Dropdown css class to be used for CButtonDropdown when item is
	 *                                             selected.
	 * @param string $menu_data[toggle_class]      Name of dropdown toggle class.
	 */
	public function __construct(string $name, $value = null, array $menu_data = []) {
		parent::__construct($name, '');

		$this->setId(uniqid('btn-dropdown-'));
		$this->addClass(ZBX_STYLE_BTN_ALT);
		$this->addClass(self::ZBX_STYLE_BTN_TOGGLE);
		$this->menu_data = $menu_data;
		$this->menu_data += [
			'toggle_class' => self::ZBX_STYLE_BTN_TOGGLE
		];

		if ($value !== null) {
			$this->setAttribute('value', $value);
		}
	}

	public function toString($destroy = true) {
		$is_submit_form = array_key_exists('submit_form', $this->menu_data);
		$name = $this->getAttribute('name');
		$node = (new CDiv())
			->setId($this->getId())
			->addClass(self::ZBX_STYLE_CLASS)
			->addItem((new CButton(null, $is_submit_form ? $this->getAttribute('value') : ''))
				->setAttribute('aria-label', $this->getAttribute('title'))
				->setAttribute('disabled', $this->getAttribute('disabled'))
				->addClass($this->getAttribute('class'))
				->setId(zbx_formatDomId($name.'[btn]'))
				->setMenuPopup([
					'type' => 'dropdown',
					'data' => $this->menu_data
				])
			);

		if (!$is_submit_form) {
			$node->addItem(
				(new CInput('hidden', $name, $this->getAttribute('value')))->addClass(self::ZBX_STYLE_BTN_VALUE)
			);
		}

		return $node->toString(true);
	}
}
