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
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
**/


/**
 * Helper class containing methods for operations with user macros.
 */
class CApiUserMacroHelper {

	/**
	 * Validates the main fields of hosts macros API objects.
	 *
	 * @static
	 *
	 * @param array $hosts_macros  Array of host macro objects.
	 *
	 * @throws APIException
	 */
	public static function checkHostsMacrosFields(array $hosts_macros) {
		foreach ($hosts_macros as $macro) {
			self::checkMacro($macro);

			foreach ($macro as $field => $value) {
				if (!DB::hasField('hostmacro', $field)) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS, $macro['macro']);
				}
			}

			self::checkMacroType($macro);
		}

		self::checkDuplicateMacros($hosts_macros);
		self::checkIfHostMacrosDontRepeat($hosts_macros);
	}

	/**
	 * Validates the "macro" field.
	 *
	 * @static
	 *
	 * @param array $macro
	 * @param string $macro['macro']
	 *
	 * @throws APIException if the field is not valid.
	 */
	private static function checkMacro(array $macro) {
		$missing_keys = array_diff(['macro'], array_keys($macro));

		if ($missing_keys) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS,
				_s('User macro missing parameters: %1$s', implode(', ', $missing_keys))
			);
		}

		$user_macro_parser = new CUserMacroParser();

		if ($user_macro_parser->parse($macro['macro']) != CParser::PARSE_SUCCESS) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS,
				_s('Invalid macro "%1$s": %2$s.', $macro['macro'], $user_macro_parser->getError())
			);
		}
	}

	/**
	 * Validate the "type" field.
	 *
	 * @static
	 *
	 * @param array $macro
	 *
	 * @throws APIException
	 */
	private static function checkMacroType(array $macro) {
		if (array_key_exists('type', $macro) && $macro['type'] != ZBX_MACRO_TYPE_TEXT
				&& $macro['type'] != ZBX_MACRO_TYPE_SECRET) {
			throw new APIException(ZBX_API_ERROR_PARAMETERS, _s('Invalid type for macro "%1$s".', $macro['macro']));
		}
	}

	/**
	 * Checks if the given macros contain duplicates. Assumes the "macro" field is valid.
	 *
	 * @static
	 *
	 * @param array $macros
	 *
	 * @throws APIException if the given macros contain duplicates.
	 */
	private static function checkDuplicateMacros(array $macros) {
		if (count($macros) <= 1) {
			return;
		}

		$existing_macros = [];
		$user_macro_parser = new CUserMacroParser();

		foreach ($macros as $macro) {
			// Global macros don't have a 'hostid'.
			$hostid = array_key_exists('hostid', $macro) ? $macro['hostid'] : 1;

			$user_macro_parser->parse($macro['macro']);

			$macro_name = $user_macro_parser->getMacro();
			$context = $user_macro_parser->getContext();
			$regex = $user_macro_parser->getRegex();

			/*
			 * Macros with same name can have different contexts. A macro with no context is not the same
			 * as a macro with an empty context.
			 */
			if (array_key_exists($hostid, $existing_macros)
					&& array_key_exists($macro_name, $existing_macros[$hostid])) {
				$has_context = in_array($context, array_column($existing_macros[$hostid][$macro_name], 'context'),
					true
				);
				$context_exists = ($context !== null && $has_context);

				$has_regex = in_array($regex, array_column($existing_macros[$hostid][$macro_name], 'regex'), true);
				$regex_exists = ($regex !== null && $has_regex);

				$is_macro_without_context = ($context === null && $regex === null);

				if (($is_macro_without_context && $has_context && $has_regex) || ($context_exists || $regex_exists)) {
					throw new APIException(ZBX_API_ERROR_PARAMETERS,_s('Macro "%1$s" is not unique.', $macro['macro']));
				}
			}

			$existing_macros[$hostid][$macro_name][] = ['context' => $context, 'regex' => $regex];
		}
	}

	/**
	 * Checks if any of the given host macros already exist on the corresponding hosts. If the macros are updated and
	 * the "hostmacroid" field is set, the method will only fail, if a macro with a different hostmacroid exists.
	 * Assumes the "macro", "hostid" and "hostmacroid" fields are valid.
	 *
	 * @static
	 *
	 * @param array  $hostmacros
	 * @param int    $hostmacros[]['hostmacroid']
	 * @param int    $hostmacros[]['hostid']
	 * @param string $hostmacros[]['macro']
	 *
	 * @throws APIException if any of the given macros already exist.
	 */
	private static function checkIfHostMacrosDontRepeat(array $hostmacros) {
		if (!$hostmacros) {
			return;
		}

		$macro_names = [];
		$user_macro_parser = new CUserMacroParser();

		// Parse each macro, get unique names and, if context exists, narrow down the search.
		foreach ($hostmacros as $hostmacro) {
			$user_macro_parser->parse($hostmacro['macro']);

			$macro_name = $user_macro_parser->getMacro();
			$context = $user_macro_parser->getContext();
			$regex = $user_macro_parser->getRegex();

			if ($context === null && $regex === null) {
				$macro_names['{$'.$macro_name] = true;
			}
			else {
				// Narrow down the search for macros with contexts.
				$macro_names['{$'.$macro_name.':'] = true;
			}
		}

		// When updating with empty array, don't select any data from database.
		$db_hostmacros = API::UserMacro()->get([
			'output' => ['hostmacroid', 'hostid', 'macro'],
			'filter' => ['hostid' => array_unique(array_column($hostmacros, 'hostid'))],
			'search' => ['macro' => array_keys($macro_names)],
			'searchByAny' => true,
			'nopermissions' => true
		]);

		$existing_macros = [];

		// Collect existing unique macro names and their contexts for each host.
		foreach ($db_hostmacros as $db_hostmacro) {
			$user_macro_parser->parse($db_hostmacro['macro']);

			$macro_name = $user_macro_parser->getMacro();
			$context = $user_macro_parser->getContext();
			$regex = $user_macro_parser->getRegex();

			$existing_macros[$db_hostmacro['hostid']][$macro_name][$db_hostmacro['hostmacroid']] =
				['context' => $context, 'regex' => $regex];
		}

		// Compare each macro name and context to existing one.
		foreach ($hostmacros as $hostmacro) {
			$hostid = $hostmacro['hostid'];

			$user_macro_parser->parse($hostmacro['macro']);

			$macro_name = $user_macro_parser->getMacro();
			$context = $user_macro_parser->getContext();
			$regex = $user_macro_parser->getRegex();

			if (array_key_exists($hostid, $existing_macros)
					&& array_key_exists($macro_name, $existing_macros[$hostid])) {
				$has_context = ($context !== null && in_array($context,
					array_column($existing_macros[$hostid][$macro_name], 'context'), true
				));
				$has_regex = ($regex !== null && in_array($regex,
					array_column($existing_macros[$hostid][$macro_name], 'regex'), true
				));
				$is_macro_without_context = ($context === null && $regex === null);

				if ($is_macro_without_context || $has_context || $has_regex) {
					foreach ($existing_macros[$hostid][$macro_name] as $hostmacroid => $macro_details) {
						if ((!array_key_exists('hostmacroid', $hostmacro)
									|| bccomp($hostmacro['hostmacroid'], $hostmacroid) != 0)
								&& $context === $macro_details['context'] && $regex === $macro_details['regex']) {
							$hosts = API::Host()->get([
								'output' => ['name'],
								'hostids' => $hostid,
								'nopermissions' => true
							]);

							throw new APIException(ZBX_API_ERROR_PARAMETERS,
								_s('Macro "%1$s" already exists on "%2$s".', $hostmacro['macro'], $hosts[0]['name'])
							);
						}
					}
				}
			}
		}
	}
}
