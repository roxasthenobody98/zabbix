<?php declare(strict_types = 1);
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
 * Class for validating password complexity.
 */
class CPasswordComplexityValidator extends CValidator {

	/**
	 * Files of common used passwords.
	 *
	 * @var array
	 */
	private static $top_passwords_lists;

	/**
	 * Strings forbidden to be part of validated password.
	 *
	 * @var array
	 */
	private $context_data = [];

	/**
	 * An options array.
	 *
	 * @var array
	 */
	private $options = [
		'passwd_min_length' => 8,
		'passwd_check_rules' => PASSWD_CHECK_SIMPLE
	];

	/**
	 * Class constructor.
	 *
	 * @param array $options
	 * @param int   $options['passwd_min_length']   Minimum required password length.
	 * @param int   $options['passwd_check_rules']  Password complexity rules.
	 */
	public function __construct(array $options = []) {
		$this->options = $options + $this->options;

		// Collect files containing common used passwords list.
		self::$top_passwords_lists = array_filter(glob(dirname(__FILE__).'/../data/topPasswords[0-9]*.php'), 'is_file');
	}

	/**
	 * Validate password complexity.
	 *
	 * @param string $value  A password string to validate.
	 *
	 * @return bool
	 */
	public function validate($value) {
		$value = (string) $value;
		$this->setError('');

		if ($this->options['passwd_min_length'] > mb_strlen($value)) {
			$this->setError(_n('must be at least %1$d character long', 'must be at least %1$d characters long',
				$this->options['passwd_min_length']
			));

			return false;
		}

		$check_case = ((PASSWD_CHECK_CASE & $this->options['passwd_check_rules']) == PASSWD_CHECK_CASE);
		if ($check_case && self::checkCase($value) === false) {
			$this->setError(_('must contain at least one lowercase and one uppercase Latin letter'));

			return false;
		}

		$check_digits = ((PASSWD_CHECK_DIGITS & $this->options['passwd_check_rules']) == PASSWD_CHECK_DIGITS);
		if ($check_digits && self::ckeckDigits($value) === false) {
			$this->setError(_('must contain at least one digit'));

			return false;
		}

		$check_special = ((PASSWD_CHECK_SPECIAL & $this->options['passwd_check_rules']) == PASSWD_CHECK_SPECIAL);
		if ($check_special && self::checkSpecialCharacters($value) === false) {
			$this->setError(_('must contain at least one special character'));

			return false;
		}

		$check_simple = ((PASSWD_CHECK_SIMPLE & $this->options['passwd_check_rules']) == PASSWD_CHECK_SIMPLE);
		if ($check_simple && $this->chechSimple($value) === false) {
			$this->setError(_("must not contain user's name, surname or username"));

			return false;
		}

		if ($check_simple && self::chechIfPasswordIsCommonlyUsed($value) === false) {
			$this->setError(_('must not be one of common or context-specific passwords'));

			return false;
		}

		return true;
	}

	/**
	 * Set array of context specific strings forbidden to be part of validated password.
	 *
	 * @param array  $context_data  Indexed array of strings that are forbidden to be part of password.
	 */
	public function setContextData(array $context_data = []) {
		$this->context_data = $context_data;
	}

	/**
	 * Check if string contains special character.
	 *
	 * @static
	 *
	 * @param string $value  String to check.
	 *
	 * @return bool
	 */
	private static function checkSpecialCharacters(string $value): bool {
		$spec_chars = [
			' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '<', '=', '>',
			'?', '@', '[', '\\', ']', '^', '_', '`', '{', '|', '}', '~'
		];

		foreach ($spec_chars as $char) {
			if (strpos($value, $char) !== false) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Check if string contains upper and lower case characters.
	 *
	 * @static
	 *
	 * @param string $value  String to check.
	 *
	 * @return bool
	 */
	private static function checkCase(string $value): bool {
		return (preg_match('/^(?=.*[a-z])(?=.*[A-Z]).+$/', $value) == 1);
	}

	/**
	 * Check if string contains digit.
	 *
	 * @static
	 *
	 * @param string $value  String to check.
	 *
	 * @return bool
	 */
	private static function ckeckDigits(string $value): bool {
		return (preg_match('/\d/', $value) == 1);
	}

	/**
	 * Check if string doesn't contain context specific substring.
	 *
	 * @param string $value  String to check.
	 *
	 * @return bool
	 */
	private function chechSimple(string $value): bool {
		$value = mb_strtolower($value);
		foreach ($this->context_data as $context_string) {
			if (strpos($value, mb_strtolower($context_string)) !== false) {
				return false;
			}
		}

		return true;
	}

	/**
	 * Check if string doesn't contain context specific substring.
	 *
	 * @static
	 *
	 * @param string $value  String to check.
	 *
	 * @return bool
	 */
	private static function chechIfPasswordIsCommonlyUsed(string $value): bool {
		$value = base64_encode($value);
		foreach (self::$top_passwords_lists as $file) {
			if (in_array($value, require $file)) {
				return false;
			}
		}
		return true;
	}
}