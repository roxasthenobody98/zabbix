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
 * @var CView $this
 */

$rulesTable = (new CTable())
	->setHeader(['', _('Update existing'), _('Create new'), _('Delete missing')]);

$titles = [
	'groups' => _('Groups'),
	'hosts' => _('Hosts'),
	'templates' => _('Templates'),
	'valueMaps' => _('Value mappings'),
	'templateDashboards' => _('Template dashboards'),
	'templateLinkage' => _('Template linkage'),
	'applications' => _('Applications'),
	'items' => _('Items'),
	'discoveryRules' => _('Discovery rules'),
	'triggers' => _('Triggers'),
	'graphs' => _('Graphs'),
	'httptests' => _('Web scenarios'),
	'maps' => _('Maps')
];

$user_type = CWebUser::getType();

if ($user_type == USER_TYPE_SUPER_ADMIN) {
	$titles['images'] = _('Images');
	$titles['mediaTypes'] = _('Media types');
}

foreach ($titles as $key => $title) {
	$cbExist = null;
	$cbMissed = null;
	$cbDeleted = null;

	if (array_key_exists('updateExisting', $data['rules'][$key])) {
		$cbExist = (new CCheckBox('rules['.$key.'][updateExisting]'))
			->setChecked($data['rules'][$key]['updateExisting']);

		if ($key !== 'maps' && $user_type != USER_TYPE_SUPER_ADMIN && $user_type != USER_TYPE_ZABBIX_ADMIN) {
			$cbExist->setAttribute('disabled', 'disabled');
		}
		elseif ($key === 'maps') {
			$cbExist->setAttribute('disabled', $data['allowed_edit_maps'] ? null : true);
		}

		if ($key === 'images') {
			$cbExist->onClick('updateWarning(this, '.json_encode(_('Images for all maps will be updated!')).')');
		}
	}

	if (array_key_exists('createMissing', $data['rules'][$key])) {
		$cbMissed = (new CCheckBox('rules['.$key.'][createMissing]'))
			->setChecked($data['rules'][$key]['createMissing']);
	}

	if ($key !== 'maps' && $user_type != USER_TYPE_SUPER_ADMIN && $user_type != USER_TYPE_ZABBIX_ADMIN) {
		$cbMissed->setAttribute('disabled', 'disabled');
	}
	elseif ($key === 'maps') {
		$cbMissed->setAttribute('disabled', $data['allowed_edit_maps'] ? null : true);
	}

	if (array_key_exists('deleteMissing', $data['rules'][$key])) {
		$cbDeleted = (new CCheckBox('rules['.$key.'][deleteMissing]'))
			->setChecked($data['rules'][$key]['deleteMissing'])
			->addClass('deleteMissing');

		if ($key !== 'maps' && $user_type != USER_TYPE_SUPER_ADMIN && $user_type != USER_TYPE_ZABBIX_ADMIN) {
			$cbDeleted->setAttribute('disabled', 'disabled');
		}

		if ($key === 'templateLinkage') {
			$cbDeleted->onClick('updateWarning(this, '.json_encode(
				_('Template and host properties that are inherited through template linkage will be unlinked and cleared.')
			).')');
		}
	}

	$rulesTable->addRow([
		$title,
		(new CCol($cbExist))->addClass(ZBX_STYLE_CENTER),
		(new CCol($cbMissed))->addClass(ZBX_STYLE_CENTER),
		(new CCol($cbDeleted))->addClass(ZBX_STYLE_CENTER)
	]);
}

$form_list = (new CFormList())
	->addRow((new CLabel(_('Import file'), 'import_file'))->setAsteriskMark(),
		(new CFile('import_file'))
			->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
			->setAriaRequired()
			->setAttribute('autofocus', 'autofocus')
	)
	->addRow(_('Rules'), new CDiv($rulesTable));

$form = (new CForm('post', null, 'multipart/form-data'))
	->setId('import-form')
	->setAttribute('aria-labeledby', ZBX_STYLE_PAGE_TITLE)
	->addVar('import', 1)
	->addVar('rules_preset', $data['rules_preset'])
	->addItem($form_list);

$output = [
	'header' => $data['title'],
	'script_inline' => trim($this->readJsFile('popup.import.js.php')),
	'body' => $form->toString(),
	'buttons' => [
		[
			'title' => _('Import'),
			'class' => '',
			'keepOpen' => true,
			'isSubmit' => true,
			'action' => 'return confirmSubmit(overlay);'
		]
	]
];

if ($data['user']['debug_mode'] == GROUP_DEBUG_MODE_ENABLED) {
	CProfiler::getInstance()->stop();
	$output['debug'] = CProfiler::getInstance()->make()->toString();
}

echo json_encode($output);
