<?php

use Modules\FormGrid\Helper\{
	CFormGrid,
	CFormField,
	CFormActions
};

require_once dirname(__FILE__).'/../assets/js/common.js.php';
$this->addJsFile('itemtest.js');
$this->addJsFile('multiselect.js');
$this->addJsFile('multilineinput.js');
$this->addJsFile('textareaflexible.js');
$this->addJsFile('class.cviewswitcher.js');

$form = new CFormGrid;

$key_controls = [(new CTextBox('key', 'test', false, DB::getFieldLength('items', 'key_')))
	->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
	->setAriaRequired()
];
$key_controls[] = (new CDiv())->addClass(ZBX_STYLE_FORM_INPUT_MARGIN);
$key_controls[] = (new CButton('keyButton', _('Select')))
	->addClass(ZBX_STYLE_BTN_GREY)
	->onClick('return PopUp("popup.generic",jQuery.extend('.
		json_encode([
			'srctbl' => 'help_items',
			'srcfld1' => 'key',
			'dstfrm' => '',
			'dstfld1' => 'key'
		]).
			',{itemtype: jQuery("#type option:selected").val()}), null, this);'
	);

$delayFlexTable = (new CTable())
	->setId('delayFlexTable')
	->setHeader([_('Type'), _('Interval'), _('Period'), _('Action')])
	->setAttribute('style', 'width: 100%;')
	->addRow(
		[
			(new CRadioButtonList('delay_flex', 0))
				->addValue(_('Flexible'), ITEM_DELAY_FLEXIBLE)
				->addValue(_('Scheduling'), ITEM_DELAY_SCHEDULING)
				->setModern(true),
			(new CTextBox('delay_flex[delay]', ''))
				->setAttribute('placeholder', ZBX_ITEM_FLEXIBLE_DELAY_DEFAULT),
			(new CTextBox('delay_flex[schedule]', ''))
				->setAttribute('placeholder', ZBX_DEFAULT_INTERVAL),
			(new CButton('delay_flex[remove]', _('Remove')))
				->addClass(ZBX_STYLE_BTN_LINK)
				->addClass('element-table-remove')
		]
	)
	->addRow([(new CButton('interval_add', _('Add')))
	->addClass(ZBX_STYLE_BTN_LINK)
	->addClass('element-table-add')]);

$form
	->addItem([
		(new CLabel(_('Name'), 'name'))->setAsteriskMark(),
		new CFormField(
			(new CTextBox('name', ''))
			->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
			->setAriaRequired()
			->setAttribute('autofocus', 'autofocus')
		),

		new CLabel(_('Type'), 'type'),
		new CFormField(
			new CComboBox('type', '', null, ['Zabbix agent', 'Zabbix agent (active)'])
		),

		(new CLabel(_('Key'), 'key'))->setAsteriskMark(),
		new CFormField(
			$key_controls
		),

		(new CLabel(_('Host interface'), 'interfaceid'))->setAsteriskMark(),
		new CFormField(
			(new CSpan(_('No interface found')))
				->addClass(ZBX_STYLE_RED)
				->setId('interface_not_defined')
		),

		new CLabel(_('Type of information'), 'value_type'),
		new CFormField(
			(new CComboBox('value_type', 1, null, [
				ITEM_VALUE_TYPE_UINT64 => _('Numeric (unsigned)'),
				ITEM_VALUE_TYPE_FLOAT => _('Numeric (float)'),
				ITEM_VALUE_TYPE_STR => _('Character'),
				ITEM_VALUE_TYPE_LOG => _('Log'),
				ITEM_VALUE_TYPE_TEXT => _('Text')
			]))
		),

		new CLabel(_('Units'), 'units'),
		new CFormField(
			(new CTextBox('units', ''))->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
		),

		(new CLabel(_('Update interval'), 'delay'))->setAsteriskMark(),
		new CFormField(
			(new CTextBox('delay', '1m'))
				->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
				->setAriaRequired()
		),

		new CLabel(_('Custom intervals')),
		new CFormField(
			(new CDiv($delayFlexTable))
				->addClass(ZBX_STYLE_TABLE_FORMS_SEPARATOR)
				->setAttribute('style', 'min-width: '.(ZBX_TEXTAREA_STANDARD_WIDTH+50).'px;')
		),

		(new CLabel(_('History storage period'), 'history'))->setAsteriskMark(),
		new CFormField(
			(new CDiv([
				(new CRadioButtonList('history_mode', 0))
					->addValue(_('Do not keep history'), ITEM_STORAGE_OFF)
					->addValue(_('Storage period'), ITEM_STORAGE_CUSTOM)
					->setModern(true)
			]))->addStyle('display:flex;')
		),

		(new CLabel(_('Trend storage period'), 'trends'))->setAsteriskMark(),
		new CFormField(
			(new CDiv([
				(new CRadioButtonList('trends_mode', 1))
					->addValue(_('Do not keep trends'), ITEM_STORAGE_OFF)
					->addValue(_('Storage period'), ITEM_STORAGE_CUSTOM)
					->setModern(true),
				(new CDiv())->addClass(ZBX_STYLE_FORM_INPUT_MARGIN),
				(new CTextBox('trends', '365d'))
					->setWidth(ZBX_TEXTAREA_TINY_WIDTH)
					->setAriaRequired()
			]))->addStyle('display:flex;')
		),

		new CLabel(_('New application'), 'new_application'),
		new CFormField(
			(new CSpan(
				(new CTextBox('new_application', ''))->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
			))->addClass(ZBX_STYLE_FORM_NEW_GROUP)
		),

		new CLabel(_('Applications')),
		new CFormField(
			new CListBox('applications_names[]', '', 6)
		),

		new CLabel(_('Populates host inventory field')),
		new CFormField(
			(new CComboBox('inventory_link'))->addItem(0, '-'._('None').'-', null)
		),

		new CLabel(_('Description')),
		new CFormField(
			(new CTextArea('description', ''))
				->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
				->setMaxlength(DB::getFieldLength('items', 'description'))
		),

		new CLabel(_('Enabled')),
		new CFormField(
			(new CCheckBox('status', ITEM_STATUS_ACTIVE))->setChecked(true)
		),

		new CFormActions(
			new CSubmit('add', _('Add')),
			[(new CSimpleButton(_('Test')))->setId('test_item'), new CButtonCancel(url_param('hostid'))]
		)
	]);

$divTabs = new CTabView();
$divTabs->addTab('test', 'Test', (new CForm())
	->setId('item-form')
	->setName('itemForm')
	->setAttribute('aria-labeledby', ZBX_STYLE_PAGE_TITLE)
	->addVar('form', 'create')
	->addVar('hostid', '10290')
	->addItem($form)
);

(new CWidget())
	->setTitle(_('Form Grid'))
	->addItem($divTabs)
	->show();
