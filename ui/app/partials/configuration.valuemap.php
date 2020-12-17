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


/**
 * @var CPartial $this
 */

$form_list = new CFormList('valuemap-formlist');

$table = (new CTable())
	->setId('valuemap-table')
	->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_CONTAINER)
	->setColumns([
		(new CTableColumn(_('Name')))
			->addStyle('width: 250px;')
			->addClass('table-col-handle'),
		(new CTableColumn(_('Value')))
			->addStyle('width: 250px;')
			->addClass('table-col-handle'),
		(new CTableColumn(_('Action')))
			->addClass('table-col-handle')
	]);

foreach ($this->data['valuemaps'] as $valuemap) {
	zbx_add_post_js('new AddValueMap('.json_encode($valuemap).')');
}

$table->addItem([
	(new CTag('tfoot', true))->addItem([
		new CCol(
			(new CButton('tag_add', _('Add')))
				->addClass(ZBX_STYLE_BTN_LINK)
				->addClass('element-table-add')
				->setEnabled(!$this->data['readonly'])
				->onClick('return PopUp("popup.valuemap.edit", jQuery.extend('.
					json_encode([]).', null), null, this);'
				)
		)
	])
]);

$form_list->addRow(null, $table);
$form_list->show();

$this->includeJsFile('configuration.valuemap.js.php');