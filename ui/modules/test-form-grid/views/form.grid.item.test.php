<?php

use Modules\FormGrid\Helper\{
	CFormGrid,
	CFormGridItemLabel,
	CFormGridItemDetail
};

$this->addJsFile('itemtest.js');
$this->addJsFile('multiselect.js');
$this->addJsFile('multilineinput.js');
$this->addJsFile('textareaflexible.js');
$this->addJsFile('class.cviewswitcher.js');

$form = new CFormGrid;

$macros_table = (new CTable())->addClass(ZBX_STYLE_TEXTAREA_FLEXIBLE_CONTAINER);
$macros_table
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

$form
	->addItem([
		new CFormGridItemLabel(
			new CLabel(_('Get value from host'), 'get_value')
		),
		new CFormGridItemDetail(
			(new CCheckBox('get_value', 1))->setChecked(true)
		),
		(new CFormGridItemLabel(
			new CLabel(_('Host address'), 'host_address')
		))->setSmall(),
		(new CFormGridItemDetail(
			(new CTextBox('interface[address]', '127.0.0.1'))
					->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
		))->setSmall(),
		(new CFormGridItemLabel(
			new CLabel(_('Port'), 'port')
		))->setSmall(),
		(new CFormGridItemDetail(
			(new CTextBox('interface[port]', '10050', '', 64))
				->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
		))->setSmall(),
		new CFormGridItemLabel(
			new CLabel(_('Proxy'), 'proxy_hostid')
		),
		new CFormGridItemDetail(
			(new CComboBox('proxy_hostid', 0, null, [0 => _('(no proxy)')]))
				->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
		),
		(new CFormGridItemDetail(
			(new CSimpleButton(_('Get value')))
				->addClass(ZBX_STYLE_BTN_ALT)
				->setId('get_value_btn')
		))->setOffset(3)->addStyle('justify-self: end;'),
		(new CFormGridItemLabel(
			new CLabel(_('Value'), 'value')
		))->setSmall(),
		(new CFormGridItemDetail(
			(new CMultilineInput('value', '', [
				'disabled' => false,
				'readonly' => false
			]))->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
		))->setSmall(),
		(new CFormGridItemLabel(
			new CLabel(_('Time'), 'time')
		))->setSmall(),
		(new CFormGridItemDetail(
			(new CTextBox(null, 'now', true))
				->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
				->setId('time')
		))->setSmall(),
		(new CFormGridItemLabel(
			new CLabel(_('Previous value'), 'prev_item_value')
		))->setSmall(),
		(new CFormGridItemDetail(
			(new CMultilineInput('prev_value', '', [
				'disabled' => true
			]))->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
		))->setSmall(),
		(new CFormGridItemLabel(
			new CLabel(_('Prev. time'), 'prev_time')
		))->setSmall(),
		(new CFormGridItemDetail(
			(new CTextBox('prev_time', ''))
			->setEnabled(false)
			->setWidth(ZBX_TEXTAREA_SMALL_WIDTH)
		))->setSmall(),
		new CFormGridItemLabel(
			new CLabel(_('End of line sequence'), 'eol')
		),
		new CFormGridItemDetail(
			(new CRadioButtonList('eol', 0))
			->addValue(_('LF'), ZBX_EOL_LF)
			->addValue(_('CRLF'), ZBX_EOL_CRLF)
			->setModern(true)
		),
		new CFormGridItemLabel(
			new CLabel(_('Macros'))
		),
		new CFormGridItemDetail(
			(new CDiv($macros_table))->addClass(ZBX_STYLE_TABLE_FORMS_SEPARATOR)
		),
		new CFormGridItemLabel(
			new CLabel(_('Result'))
		),
		new CFormGridItemDetail(
			(new CDiv([
				(new CSpan('Result converted to Text'))->addClass('grey'),
				new CSpan('Zabbix server')
			]))
				->addClass(ZBX_STYLE_TABLE_FORMS_SEPARATOR)
				->addStyle('display: flex; justify-content: space-between;')
		),
	]);

$div = (new CDiv($form))->addStyle('width: 850px;');

$divTabs = new CTabView();
$divTabs->addTab('test', 'Test', $div);

(new CWidget())
	->setTitle(_('Form Grid'))
	->addItem($divTabs)
	->show();
