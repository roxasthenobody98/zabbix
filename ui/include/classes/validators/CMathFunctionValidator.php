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
 * Class to validate mathematical functions used in trigger expressions.
 */
class CMathFunctionValidator extends CValidator {

	/**
	 * The array containing valid functions.
	 *
	 * @var array
	 */
	private $allowed = [
		'abs',
		'avg',
		'max',
		'min',
		'length',
		'sum'
	];

	/**
	 * The array containing functions with supported exactly one parameter.
	 *
	 * @var array
	 */
	private $single_parameter_functions = [
		'abs',
		'length'
	];

	/**
	 * If set to true, LLD macros can be used inside functions and are properly validated using LLD macro parser.
	 *
	 * @var bool
	 */
	private $lldmacros = false;

	public function __construct(array $options = []) {
		/*
		 * CValidator is an abstract class, so no specific functionality should be bound to it. Thus putting
		 * an option "lldmacros" (or class variable $lldmacros) in it, is not preferred. Without it, class
		 * initialization would fail due to __set(). So instead we create a local variable in this extended class
		 * and remove the option "lldmacros" before calling the parent constructor.
		 */
		if (array_key_exists('lldmacros', $options)) {
			$this->lldmacros = $options['lldmacros'];
			unset($options['lldmacros']);
		}
		parent::__construct($options);
	}

	/**
	 * Validate trigger math function.
	 *
	 * @param CFunctionParserResult $fn
	 *
	 * @return bool
	 */
	public function validate($fn) {
		$this->setError('');

		if (!in_array($fn->function, $this->allowed)) {
			$this->setError(_s('Incorrect trigger function "%1$s" provided in expression.', $fn->match).' '.
				_('Unknown function.'));
			return false;
		}

		if ((in_array($fn->function, $this->single_parameter_functions) && count($fn->params_raw['parameters']) != 1)
				|| count($fn->params_raw['parameters']) == 0) {
			$this->setError(_s('Incorrect trigger function "%1$s" provided in expression.', $fn->match).' '.
				_('Invalid number of parameters.'));
			return false;
		}

		foreach ($fn->params_raw['parameters'] as $param) {
			if ($param instanceof CQueryParserResult) {
				$this->setError(_s('Incorrect trigger function "%1$s" provided in expression.', $param->match));
				return false;
			}
		}

		return true;
	}
}