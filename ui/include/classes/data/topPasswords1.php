<?php declare(strict_types = 1);
/**
 * This file is meant to strengthen password validation for internal users. Passwords included in the list
 * are considered weak due to their common use and are not allowed to be chosen by Zabbix internal users
 * for security reasons. The file is generated automatically from the list of NCSC "Top 100k passwords",
 * the list of SecLists "Top 1M passwords" and the list of Zabbix context-specific passwords.
 *
 * The list of passwords is used to check for commonly used passwords according to the password policy.
 * Passwords are stored as indexed array of base64-encoded strings.
 */

