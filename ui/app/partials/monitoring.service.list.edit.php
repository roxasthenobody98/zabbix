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


/**
 * @var CPartial $this
 */

$form = (new CForm())
	->setId('service-list')
	->setName('service-list');

$header = [
	(new CColHeader(
		(new CCheckBox('all_services'))->onClick("checkAll('".$form->getName()."', 'all_services', 'serviceids');")
	))->addClass(ZBX_STYLE_CELL_WIDTH)
];

if ($data['is_filtered']) {
	$path = null;

	$header[] = (new CColHeader(_('Parent services')))->addStyle('width: 15%');
	$header[] = (new CColHeader(_('Name')))->addStyle('width: 10%');
}
else {
	$path = $data['path'];
	if ($data['service'] !== null) {
		$path[] = $data['service']['serviceid'];
	}

	$header[] = (new CColHeader(_('Name')))->addStyle('width: 25%');
}

$table = (new CTableInfo())
	->setHeader(array_merge($header, [
		(new CColHeader(_('Status')))->addStyle('width: 14%'),
		(new CColHeader(_('Root cause')))->addStyle('width: 24%'),
		(new CColHeader(_('SLA')))->addStyle('width: 14%'),
		(new CColHeader(_('Tags')))->addClass(ZBX_STYLE_COLUMN_TAGS_3),
		(new CColHeader())
	]));

foreach ($data['services'] as $serviceid => $service) {
	$row = [new CCheckBox('serviceids['.$serviceid.']', $serviceid)];

	if ($data['is_filtered']) {
		$parents = [];
		$count = 0;
		while ($parent = array_shift($service['parents'])) {
			$parents[] = (new CLink($parent['name'], (new CUrl('zabbix.php'))
				->setArgument('action', 'service.list')
				->setArgument('serviceid', $parent['serviceid'])
			))->setAttribute('data-serviceid', $parent['serviceid']);

			$count++;
			if ($count >= $data['max_in_table'] || !$service['parents']) {
				break;
			}

			$parents[] = ', ';
		}

		$row[] = $parents;
	}

	$table->addRow(new CRow(array_merge($row, [
		($service['children'] > 0)
			? [
				(new CLink($service['name'], (new CUrl('zabbix.php'))
					->setArgument('action', 'service.list.edit')
					->setArgument('path', $path)
					->setArgument('serviceid', $serviceid)
				))->setAttribute('data-serviceid', $serviceid),
				CViewHelper::showNum($service['children'])
			]
			: $service['name'],
		in_array($service['status'], [TRIGGER_SEVERITY_INFORMATION, TRIGGER_SEVERITY_NOT_CLASSIFIED])
			? (new CCol(_('OK')))->addClass(ZBX_STYLE_GREEN)
			: (new CCol(getSeverityName($service['status'])))->addClass(getSeverityStyle($service['status'])),
		'',
		($service['showsla'] == SERVICE_SHOW_SLA_ON) ? sprintf('%.4f', $service['goodsla']) : '',
		array_key_exists($serviceid, $data['tags']) ? $data['tags'][$serviceid] : 'tags',
		(new CCol([
			(new CButton(null))
				->addClass(ZBX_STYLE_BTN_ADD)
				->addClass('js-add-child-service')
				->setAttribute('data-serviceid', $serviceid),
			(new CButton(null))
				->addClass(ZBX_STYLE_BTN_EDIT)
				->addClass('js-edit-service')
				->setAttribute('data-serviceid', $serviceid),
			(new CButton(null))
				->addClass(ZBX_STYLE_BTN_REMOVE)
				->addClass('js-remove-service')
				->setAttribute('data-serviceid', $serviceid)
		]))->addClass(ZBX_STYLE_LIST_TABLE_ACTIONS)
	])));
}

$form
	->addItem([
		$table,
		$data['paging'],
		new CActionButtonList('action', 'serviceids', [
			'popup.massupdate.service' => [
				'content' => (new CButton('', _('Mass update')))
					->onClick("return openMassupdatePopup(this, 'popup.massupdate.service');")
					->addClass(ZBX_STYLE_BTN_ALT)
					->removeAttribute('id')
			],
			'service.delete' => ['name' => _('Delete'), 'confirm' => _('Delete selected services?')]
		])
	])
	->show();