<?php

use Modules\FormGrid\Helper\{
	CFormGrid,
	CFormField
};

$this->addJsFile('itemtest.js');
$this->addJsFile('multiselect.js');
$this->addJsFile('multilineinput.js');
$this->addJsFile('textareaflexible.js');
$this->addJsFile('class.cviewswitcher.js');

$macros_table = (new CTable())
	->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_CONTAINER)
	->addRow([
		(new CCol(
			(new CTextAreaFlexible('macro_rows[]', '{$MACRO}', ['readonly' => false]))
				->setWidth(ZBX_TEXTAREA_MACRO_WIDTH)
				->removeAttribute('name')
				->removeId()
		))->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_PARENT),
		(new CCol('&rArr;'))->addStyle('vertical-align: top;'),
		(new CCol(
			(new CTextAreaFlexible('macros[]', ''))
				->setWidth(ZBX_TEXTAREA_MACRO_VALUE_WIDTH)
				->setAttribute('placeholder', _('value'))
				->removeId()
		))->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_PARENT)
	])
	->addRow([
		(new CCol(
			(new CTextAreaFlexible('macro_rows[]', '{$MACRO}', ['readonly' => false]))
				->setWidth(ZBX_TEXTAREA_MACRO_WIDTH)
				->removeAttribute('name')
				->removeId()
		))->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_PARENT),
		(new CCol('&rArr;'))->addStyle('vertical-align: top;'),
		(new CCol(
			(new CTextAreaFlexible('macros[]', ''))
				->setWidth(ZBX_TEXTAREA_MACRO_VALUE_WIDTH)
				->setAttribute('placeholder', _('value'))
				->removeId()
		))->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_PARENT)
	])
	->addRow([
		(new CCol(
			(new CTextAreaFlexible('macro_rows[]', '{$MACRO}', ['readonly' => false]))
				->setWidth(ZBX_TEXTAREA_MACRO_WIDTH)
				->removeAttribute('name')
				->removeId()
		))->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_PARENT),
		(new CCol('&rArr;'))->addStyle('vertical-align: top;'),
		(new CCol(
			(new CTextAreaFlexible('macros[]', ''))
				->setWidth(ZBX_TEXTAREA_MACRO_VALUE_WIDTH)
				->setAttribute('placeholder', _('value'))
				->removeId()
		))->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_PARENT)
	]);

$form = (new CFormGrid())
	->addClass(CFormGrid::ZBX_STYLE_FORM_GRID_3_1)
	->addItem([
		new CLabel(_('Get value from host'), 'get_value'),
		(new CFormField(
			(new CCheckBox('get_value', 1))->setChecked(true)
		))->addClass(CFormField::ZBX_STYLE_FORM_FIELD_FLUID),

		new CLabel(_('Host address'), 'host_address'),
		(new CFormField(
			(new CTextBox('interface[address]', '127.0.0.1'))
					->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
		)),

		new CLabel(_('Port'), 'port'),
		(new CFormField(
			(new CTextBox('interface[port]', '10050', '', 64))
				->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
		)),

		new CLabel(_('Proxy'), 'proxy_hostid'),
		(new CFormField(
			(new CComboBox('proxy_hostid', 0, null, [0 => _('(no proxy)')]))
				->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
		))->addClass(CFormField::ZBX_STYLE_FORM_FIELD_FLUID),

		(new CFormField(
			(new CSimpleButton(_('Get value')))
				->addClass(ZBX_STYLE_BTN_ALT)
				->setId('get_value_btn')
		))
			->addClass(CFormField::ZBX_STYLE_FORM_FIELD_OFFSET_3)
			->addStyle('justify-self: end;'),

		new CLabel(_('Value'), 'value'),
		(new CFormField(
			(new CMultilineInput('value', '', [
				'disabled' => false,
				'readonly' => false
			]))->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
		)),

		new CLabel(_('Time'), 'time'),
		(new CFormField(
			(new CTextBox(null, 'now', true))
				->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
				->setId('time')
		)),

		new CLabel(_('Previous value'), 'prev_item_value'),
		(new CFormField(
			(new CMultilineInput('prev_value', '', [
				'disabled' => true
			]))->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
		)),

		new CLabel(_('Prev. time'), 'prev_time'),
		(new CFormField(
			(new CTextBox('prev_time', ''))
			->setEnabled(false)
			->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
		)),

		new CLabel(_('End of line sequence'), 'eol'),
		(new CFormField(
			(new CRadioButtonList('eol', 0))
			->addValue(_('LF'), ZBX_EOL_LF)
			->addValue(_('CRLF'), ZBX_EOL_CRLF)
			->setModern(true)
		))->addClass(CFormField::ZBX_STYLE_FORM_FIELD_FLUID),

		new CLabel(_('Macros')),
		(new CFormField(
			(new CDiv($macros_table))->addClass(ZBX_STYLE_TABLE_FORMS_SEPARATOR)
		))->addClass(CFormField::ZBX_STYLE_FORM_FIELD_FLUID),

		new CLabel(_('Result')),
		(new CFormField(
			(new CDiv([
				(new CSpan('Result converted to Text'))->addClass('grey'),
				new CSpan('Zabbix server')
			]))
				->addClass(ZBX_STYLE_TABLE_FORMS_SEPARATOR)
				->addStyle('display: flex; justify-content: space-between;')
		))->addClass(CFormField::ZBX_STYLE_FORM_FIELD_FLUID)
	]);

$div = (new CDiv($form))->addStyle('width: 850px;');

$divTabs = new CTabView();
$divTabs->addTab('test', 'Test', $div);

(new CWidget())
	->setTitle(_('Form Grid'))
	->addItem($divTabs)
	->show();
