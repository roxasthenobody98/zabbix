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
 * Class containing methods for operations with graph.
 */
class CGraph extends CGraphGeneral {

	protected $tableName = 'graphs';
	protected $tableAlias = 'g';
	protected $sortColumns = ['graphid', 'name', 'graphtype'];

	public function __construct() {
		parent::__construct();

		$this->errorMessages = array_merge($this->errorMessages, [
			self::ERROR_TEMPLATE_HOST_MIX =>
				_('Graph "%1$s" with templated items cannot contain items from other hosts.'),
			self::ERROR_MISSING_GRAPH_NAME => _('Missing "name" field for graph.'),
			self::ERROR_MISSING_GRAPH_ITEMS => _('Missing items for graph "%1$s".'),
			self::ERROR_MISSING_REQUIRED_VALUE => _('No "%1$s" given for graph.'),
			self::ERROR_GRAPH_SUM => _('Cannot add more than one item with type "Graph sum" on graph "%1$s".')
		]);
	}

	/**
	 * Get graph data.
	 *
	 * @param array $options
	 *
	 * @return array
	 */
	public function get($options = []) {
		$result = [];

		$sqlParts = [
			'select'	=> ['graphs' => 'g.graphid'],
			'from'		=> ['graphs' => 'graphs g'],
			'where'		=> [],
			'group'		=> [],
			'order'		=> [],
			'limit'		=> null
		];

		$defOptions = [
			'groupids'					=> null,
			'templateids'				=> null,
			'hostids'					=> null,
			'graphids'					=> null,
			'itemids'					=> null,
			'templated'					=> null,
			'inherited'					=> null,
			'editable'					=> false,
			'nopermissions'				=> null,
			// filter
			'filter'					=> null,
			'search'					=> null,
			'searchByAny'				=> null,
			'startSearch'				=> false,
			'excludeSearch'				=> false,
			'searchWildcardsEnabled'	=> null,
			// output
			'output'					=> API_OUTPUT_EXTEND,
			'selectGroups'				=> null,
			'selectTemplates'			=> null,
			'selectHosts'				=> null,
			'selectItems'				=> null,
			'selectGraphItems'			=> null,
			'selectDiscoveryRule'		=> null,
			'selectGraphDiscovery'		=> null,
			'countOutput'				=> false,
			'groupCount'				=> false,
			'preservekeys'				=> false,
			'sortfield'					=> '',
			'sortorder'					=> '',
			'limit'						=> null
		];
		$options = zbx_array_merge($defOptions, $options);

		// permission check
		if (self::$userData['type'] != USER_TYPE_SUPER_ADMIN && !$options['nopermissions']) {
			$permission = $options['editable'] ? PERM_READ_WRITE : PERM_READ;
			$userGroups = getUserGroupsByUserId(self::$userData['userid']);

			// check permissions by graph items
			$sqlParts['where'][] = 'NOT EXISTS ('.
				'SELECT NULL'.
				' FROM graphs_items gi,items i,hosts_groups hgg'.
					' LEFT JOIN rights r'.
						' ON r.id=hgg.groupid'.
							' AND '.dbConditionInt('r.groupid', $userGroups).
				' WHERE g.graphid=gi.graphid'.
					' AND gi.itemid=i.itemid'.
					' AND i.hostid=hgg.hostid'.
				' GROUP BY i.hostid'.
				' HAVING MAX(permission)<'.zbx_dbstr($permission).
					' OR MIN(permission) IS NULL'.
					' OR MIN(permission)='.PERM_DENY.
				')';
			// check permissions by Y min item
			$sqlParts['where'][] = 'NOT EXISTS ('.
				'SELECT NULL'.
				' FROM items i,hosts_groups hgg'.
					' LEFT JOIN rights r'.
						' ON r.id=hgg.groupid'.
							' AND '.dbConditionInt('r.groupid', $userGroups).
				' WHERE g.ymin_type='.GRAPH_YAXIS_TYPE_ITEM_VALUE.
					' AND g.ymin_itemid=i.itemid'.
					' AND i.hostid=hgg.hostid'.
				' GROUP BY i.hostid'.
				' HAVING MAX(permission)<'.zbx_dbstr($permission).
					' OR MIN(permission) IS NULL'.
					' OR MIN(permission)='.PERM_DENY.
				')';
			// check permissions by Y max item
			$sqlParts['where'][] = 'NOT EXISTS ('.
				'SELECT NULL'.
				' FROM items i,hosts_groups hgg'.
					' LEFT JOIN rights r'.
						' ON r.id=hgg.groupid'.
							' AND '.dbConditionInt('r.groupid', $userGroups).
				' WHERE g.ymax_type='.GRAPH_YAXIS_TYPE_ITEM_VALUE.
					' AND g.ymax_itemid=i.itemid'.
					' AND i.hostid=hgg.hostid'.
				' GROUP BY i.hostid'.
				' HAVING MAX(permission)<'.zbx_dbstr($permission).
					' OR MIN(permission) IS NULL'.
					' OR MIN(permission)='.PERM_DENY.
				')';
		}

		// groupids
		if (!is_null($options['groupids'])) {
			zbx_value2array($options['groupids']);

			$sqlParts['from']['graphs_items'] = 'graphs_items gi';
			$sqlParts['from']['items'] = 'items i';
			$sqlParts['from']['hosts_groups'] = 'hosts_groups hg';
			$sqlParts['where'][] = dbConditionInt('hg.groupid', $options['groupids']);
			$sqlParts['where'][] = 'hg.hostid=i.hostid';
			$sqlParts['where']['gig'] = 'gi.graphid=g.graphid';
			$sqlParts['where']['igi'] = 'i.itemid=gi.itemid';
			$sqlParts['where']['hgi'] = 'hg.hostid=i.hostid';

			if ($options['groupCount']) {
				$sqlParts['group']['hg'] = 'hg.groupid';
			}
		}

		// templateids
		if (!is_null($options['templateids'])) {
			zbx_value2array($options['templateids']);

			if (!is_null($options['hostids'])) {
				zbx_value2array($options['hostids']);
				$options['hostids'] = array_merge($options['hostids'], $options['templateids']);
			}
			else {
				$options['hostids'] = $options['templateids'];
			}
		}

		// hostids
		if (!is_null($options['hostids'])) {
			zbx_value2array($options['hostids']);

			$sqlParts['from']['graphs_items'] = 'graphs_items gi';
			$sqlParts['from']['items'] = 'items i';
			$sqlParts['where'][] = dbConditionInt('i.hostid', $options['hostids']);
			$sqlParts['where']['gig'] = 'gi.graphid=g.graphid';
			$sqlParts['where']['igi'] = 'i.itemid=gi.itemid';

			if ($options['groupCount']) {
				$sqlParts['group']['i'] = 'i.hostid';
			}
		}

		// graphids
		if (!is_null($options['graphids'])) {
			zbx_value2array($options['graphids']);

			$sqlParts['where'][] = dbConditionInt('g.graphid', $options['graphids']);
		}

		// itemids
		if (!is_null($options['itemids'])) {
			zbx_value2array($options['itemids']);

			$sqlParts['from']['graphs_items'] = 'graphs_items gi';
			$sqlParts['where']['gig'] = 'gi.graphid=g.graphid';
			$sqlParts['where'][] = dbConditionInt('gi.itemid', $options['itemids']);

			if ($options['groupCount']) {
				$sqlParts['group']['gi'] = 'gi.itemid';
			}
		}

		// templated
		if (!is_null($options['templated'])) {
			$sqlParts['from']['graphs_items'] = 'graphs_items gi';
			$sqlParts['from']['items'] = 'items i';
			$sqlParts['from']['hosts'] = 'hosts h';
			$sqlParts['where']['igi'] = 'i.itemid=gi.itemid';
			$sqlParts['where']['ggi'] = 'g.graphid=gi.graphid';
			$sqlParts['where']['hi'] = 'h.hostid=i.hostid';

			if ($options['templated']) {
				$sqlParts['where'][] = 'h.status='.HOST_STATUS_TEMPLATE;
			}
			else {
				$sqlParts['where'][] = 'h.status<>'.HOST_STATUS_TEMPLATE;
			}
		}

		// inherited
		if (!is_null($options['inherited'])) {
			if ($options['inherited']) {
				$sqlParts['where'][] = 'g.templateid IS NOT NULL';
			}
			else {
				$sqlParts['where'][] = 'g.templateid IS NULL';
			}
		}

		// search
		if (is_array($options['search'])) {
			zbx_db_search('graphs g', $options, $sqlParts);
		}

		// filter
		if (is_null($options['filter'])) {
			$options['filter'] = [];
		}

		if (is_array($options['filter'])) {
			if (!array_key_exists('flags', $options['filter'])) {
				$options['filter']['flags'] = [ZBX_FLAG_DISCOVERY_NORMAL, ZBX_FLAG_DISCOVERY_CREATED];
			}

			$this->dbFilter('graphs g', $options, $sqlParts);

			if (isset($options['filter']['host'])) {
				zbx_value2array($options['filter']['host']);

				$sqlParts['from']['graphs_items'] = 'graphs_items gi';
				$sqlParts['from']['items'] = 'items i';
				$sqlParts['from']['hosts'] = 'hosts h';
				$sqlParts['where']['gig'] = 'gi.graphid=g.graphid';
				$sqlParts['where']['igi'] = 'i.itemid=gi.itemid';
				$sqlParts['where']['hi'] = 'h.hostid=i.hostid';
				$sqlParts['where']['host'] = dbConditionString('h.host', $options['filter']['host']);
			}

			if (isset($options['filter']['hostid'])) {
				zbx_value2array($options['filter']['hostid']);

				$sqlParts['from']['graphs_items'] = 'graphs_items gi';
				$sqlParts['from']['items'] = 'items i';
				$sqlParts['where']['gig'] = 'gi.graphid=g.graphid';
				$sqlParts['where']['igi'] = 'i.itemid=gi.itemid';
				$sqlParts['where']['hostid'] = dbConditionInt('i.hostid', $options['filter']['hostid']);
			}
		}

		// limit
		if (zbx_ctype_digit($options['limit']) && $options['limit']) {
			$sqlParts['limit'] = $options['limit'];
		}

		$sqlParts = $this->applyQueryOutputOptions($this->tableName(), $this->tableAlias(), $options, $sqlParts);
		$sqlParts = $this->applyQuerySortOptions($this->tableName(), $this->tableAlias(), $options, $sqlParts);
		$dbRes = DBselect(self::createSelectQueryFromParts($sqlParts), $sqlParts['limit']);
		while ($graph = DBfetch($dbRes)) {
			if ($options['countOutput']) {
				if ($options['groupCount']) {
					$result[] = $graph;
				}
				else {
					$result = $graph['rowscount'];
				}
			}
			else {
				$result[$graph['graphid']] = $graph;
			}
		}

		if ($options['countOutput']) {
			return $result;
		}

		if (isset($options['expandName'])) {
			$result = CMacrosResolverHelper::resolveGraphNameByIds($result);
		}

		if ($result) {
			$result = $this->addRelatedObjects($options, $result);
		}

		// removing keys (hash -> array)
		if (!$options['preservekeys']) {
			$result = zbx_cleanHashes($result);
		}

		return $result;
	}

	/**
	 * Updates the children of the graph on the given hosts and propagates the inheritance to the child hosts.
	 *
	 * @param array      $graphs   An array of graphs to inherit. Each graph must contain all graph properties including
	 *                             "gitems" property.
	 * @param array|null $hostids  An array of hosts to inherit to; if set to null, the graphs will be inherited to all
	 *                             linked hosts or templates.
	 * @throws APIException
	 */
	protected function inherit(array $graphs, array $hostids = null): void {
		$graphs = zbx_toHash($graphs, 'graphid');
		$itemids = [];
		$graphids_items = [];

		foreach ($graphs as $graphid => $graph) {
			if ($graph['ymin_itemid'] > 0) {
				$itemids[$graph['ymin_itemid']] = true;
				$graphids_items[$graphid][$graph['ymin_itemid']] = true;
			}

			if ($graph['ymax_itemid'] > 0) {
				$itemids[$graph['ymax_itemid']] = true;
				$graphids_items[$graphid][$graph['ymax_itemid']] = true;
			}

			foreach ($graph['gitems'] as $gitem) {
				$itemids[$gitem['itemid']] = true;
				$graphids_items[$graphid][$gitem['itemid']] = true;
			}
		}

		$itemids = array_keys($itemids);

		$items_templateids = [];
		$itemids_templateids = [];
		$templateids = [];

		$db_templates = DBselect(
			'SELECT i.hostid AS templateid,i.itemid,i.key_'.
			' FROM items i,hosts h'.
			' WHERE i.hostid=h.hostid'.
				' AND '.dbConditionId('i.itemid', $itemids).
				' AND h.status='.HOST_STATUS_TEMPLATE
		);

		$itemids = [];

		while ($data = DBfetch($db_templates)) {
			$items_templateids[$data['key_']][$data['itemid']] = $data['templateid'];
			$itemids_templateids[$data['itemid']] = $data['templateid'];
			$templateids[$data['templateid']] = true;
			$itemids[] = $data['itemid'];
		}

		if (!$items_templateids) {
			return;
		}

		$graphids = [];
		$same_name_graphs = [];

		// Cleaning non-template graphs and collect data of template graphs.
		foreach ($graphids_items as $graphid => $items) {
			if (array_diff(array_keys($items), $itemids)) {
				unset($graphs[$graphid]);
			}
			else {
				$graphids[] = $graphid;
				$same_name_graphs[$graphs[$graphid]['name']][$graphid] = true;
			}
		}

		if (!$graphs) {
			return;
		}

		$templateids = array_keys($templateids);

		$templateids_hosts = [];
		$hostids_condition = ($hostids === null) ? '' : ' AND '.dbConditionId('ht.hostid', $hostids);
		$hostids = [];

		$db_hosts = DBselect(
			'SELECT ht.templateid,ht.hostid'.
			' FROM hosts_templates ht,hosts h'.
			' WHERE ht.hostid=h.hostid'.
				' AND '.dbConditionId('ht.templateid', $templateids).
				' AND '.dbConditionInt('h.flags', [ZBX_FLAG_DISCOVERY_NORMAL, ZBX_FLAG_DISCOVERY_CREATED]).
				$hostids_condition
		);

		while ($data = DBfetch($db_hosts)) {
			$templateids_hosts[$data['templateid']][$data['hostid']] = true;
			$hostids[$data['hostid']] = true;
		}

		if (!$templateids_hosts) {
			return;
		}

		foreach ($same_name_graphs as $name => $_graphs) {
			if (count($_graphs) > 1) {
				$_graphids = [];
				$_templateids =[];

				foreach (array_keys($_graphs) as $graphid) {
					$itemid = reset($graphs[$graphid]['gitems'])['itemid'];
					$templateid = $itemids_templateids[$itemid];
					$_graphids[] = $templateid;
					$_templateids[] = $templateid;
				}

				$_templateids_count = count($_templateids);
				for ($i = 0; $i < ($_templateids_count - 1); $i++) {
					for ($j = $i + 1; $j < $_templateids_count; $j++) {
						$same_hosts = array_intersect_key($templateids_hosts[$_templateids[$i]],
							$templateids_hosts[$_templateids[$j]]
						);

						if ($same_hosts) {
							$hosts = API::Host()->get([
								'output' => ['host'],
								'hostids' => key($same_hosts),
								'nopermissions' => true,
								'preservekeys' => true,
								'templated_hosts' => true
							]);

							self::exception(ZBX_API_ERROR_PARAMETERS,
								_s('Unable to inherit graph with duplicate name "%1$s" in data to host "%2$s".', $name,
									$hosts[key($same_hosts)]['host']
								)
							);
						}
					}
				}
			}
		}

		$hostids = array_keys($hostids);

		$items_hostids = [];
		$db_items = DBselect(
			'SELECT dest.itemid,src.key_,dest.hostid'.
			' FROM items dest,items src'.
			' WHERE dest.key_=src.key_'.
				' AND '.dbConditionId('dest.hostid', $hostids).
				' AND '.dbConditionId('src.itemid', $itemids)
		);

		while ($item = DBfetch($db_items)) {
			$items_hostids[$item['key_']][$item['itemid']] = $item['hostid'];
		}

		$hosts_names = [];
		$db_hosts = DBselect('SELECT h.hostid,h.host FROM hosts h WHERE '.dbConditionId('h.hostid', $hostids));

		while ($host = DBfetch($db_hosts)) {
			$hosts_names[$host['hostid']] = $host['host'];
		}

		/*
		 * In case when all equivalent items to graphs templates items exists on all hosts, to which they are linked,
		 * there will be collected relations between template items and these equivalents on hosts.
		 */
		$template_itemids_hostids_itemids = [];

		foreach ($items_templateids as $key => $_itemids_templateids) {
			foreach ($_itemids_templateids as $template_itemid => $templateid) {
				foreach ($items_hostids[$key] as $host_itemid => $hostid) {
					if (array_key_exists($hostid, $templateids_hosts[$templateid])) {
						$template_itemids_hostids_itemids[$template_itemid][$hostid] = $host_itemid;
					}
				}
			}
		}

		$child_graphs_to_add = [];
		$child_graphs_to_update = [];

		/*
		 * Since the inherit() method called either when graphs are only created (called from create() or
		 * syncTemplates() methods) or when only updated (called from update() method), the child graphs will be only
		 * found when graph was updated.
		 */
		$child_graphs = $this->get([
			'output' => ['graphid', 'name', 'templateid'],
			'selectItems' => ['hostid'],
			'hostids' => $hostids,
			'filter' => ['templateid' => $graphids, 'flags' => ZBX_FLAG_DISCOVERY_NORMAL],
			'nopermissions' => true,
			'preservekeys' => true
		]);

		// Graphs data collection to inherit in case when inherit() method was called from update() API method.
		if ($child_graphs) {
			$graphs_child_graphs = [];
			$child_graphs_hostids = [];

			foreach ($child_graphs as $child_graphid => $child_graph) {
				$graphs_child_graphs[$child_graph['templateid']][$child_graphid] = $child_graph;

				/*
				* Since graph on template can have only items of this template, the hostid also will be the same for all
				* child graph items.
				*/
				$child_graphs_hostids[$child_graphid] = reset($child_graph['items'])['hostid'];
			}

			$graphs_changed_names_child_graphids = [];

			foreach ($graphs as $graphid => $graph) {
				foreach ($graphs_child_graphs[$graphid] as $child_graphid => $child_graph) {
					/*
					* If template graph name was changed, we collect all that names to check whether graphs with the
					* same name already exists on child hosts/templates.
					*/
					if ($graph['name'] !== $child_graph['name']) {
						$graphs_changed_names_child_graphids[$graph['name']][] = $child_graphid;
					}

					$child_graph_hostid = reset($child_graph['items'])['hostid'];
					$child_graph_to_update = ['graphid' => $child_graphid, 'templateid' => $graphid] + $graph;

					if ($graph['ymin_itemid'] != 0) {
						$child_graph_to_update['ymin_itemid'] =
							$template_itemids_hostids_itemids[$graph['ymin_itemid']][$child_graph_hostid];
					}

					if ($graph['ymax_itemid'] != 0) {
						$child_graph_to_update['ymax_itemid'] =
							$template_itemids_hostids_itemids[$graph['ymax_itemid']][$child_graph_hostid];
					}

					foreach ($child_graph_to_update['gitems'] as &$gitem) {
						$gitem['itemid'] = $template_itemids_hostids_itemids[$gitem['itemid']][$child_graph_hostid];
					}
					unset($gitem);

					$child_graphs_to_update[$child_graphid] = $child_graph_to_update;
				}
			}

			if ($graphs_changed_names_child_graphids) {
				$possible_duplicate_name_hostids = [];
				$db_possible_duplicate_name_graphs = DBselect(
					'SELECT g.name,i.hostid'.
					' FROM graphs g,graphs_items gi,items i'.
					' WHERE g.graphid=gi.graphid AND gi.itemid=i.itemid'.
						' AND '.dbConditionString('g.name', array_keys($graphs_changed_names_child_graphids)).
						' AND '.dbConditionId('i.hostid', $hostids).
					' GROUP BY g.name,i.hostid'
				);

				while ($row = DBfetch($db_possible_duplicate_name_graphs)) {
					$possible_duplicate_name_hostids[$row['name']][] = $row['hostid'];
				}

				if ($possible_duplicate_name_hostids) {
					foreach ($graphs_changed_names_child_graphids as $name => $child_graphids) {
						if (array_key_exists($name, $possible_duplicate_name_hostids)) {
							foreach ($child_graphids as $child_graphid) {
								$duplicate_name_hostid = reset(array_intersect($possible_duplicate_name_hostids[$name],
									[$child_graphs_hostids[$child_graphid]]
								));

								if ($duplicate_name_hostid) {
									self::exception(ZBX_API_ERROR_PARAMETERS,
										_s('Graph "%1$s" already exists on "%2$s".', $name,
											$hosts_names[$duplicate_name_hostid]
										)
									);
								}
							}
						}
					}
				}
			}
		}

		/*
		 * Graphs data collection to inherit in case when inherit() method was called from create() or syncTemplates()
		 * API method.
		 */
		else {
			$graphids_names = [];
			$graphs_names_required_hosts = [];
			$hostids_graph_names_parent_graphids = [];
			$hostids_graphs = [];

			foreach ($graphs as $graphid => $graph) {
				$graphids_names[$graphid] = $graph['name'];

				$itemid = reset($graph['gitems'])['itemid'];
				$templateid = $itemids_templateids[$itemid];

				$graphs_names_required_hosts[$graph['name']] += $templateids_hosts[$templateid];

				foreach (array_keys($templateids_hosts[$templateid]) as $hostid) {
					$hostids_graph_names_parent_graphids[$hostid][$graph['name']] = $graphid;
					$hostids_graphs[$hostid][$graphid] = true;
				}
			}

			$possible_same_name_hosts_graphs = $this->get([
				'output' => ['graphid', 'name', 'templateid', 'flags'],
				'selectGraphItems' => ['gitemid', 'graphid', 'itemid'],
				'selectItems' => ['hostid'],
				'hostids' => $hostids,
				'filter' => ['name' => $graphids_names],
				'nopermissions' => true,
				'preservekeys' => true
			]);

			$parent_graphids_updated_hosts = [];

			foreach ($possible_same_name_hosts_graphs as $graphid => $graph) {
				$graph_hostids = array_unique(array_column($graph['items'], 'hostid'));
				$required_hostids = array_intersect($graph_hostids,
					array_keys($graphs_names_required_hosts[$graph['name']])
				);

				if (!$required_hostids) {
					continue;
				}

				if (count($graph_hostids) > 1) {
					$hostid = reset($required_hostids);
					self::exception(ZBX_API_ERROR_PARAMETERS,
						_s('Graph "%1$s" already exists on "%2$s" (items are not identical).', $graph['name'],
							$hosts_names[reset($required_hostids)]
						)
					);
				}

				$hostid = reset($graph_hostids);

				$parent_graphid = $hostids_graph_names_parent_graphids[$hostid][$graph['name']];
				$parent_graph = $graphs[$parent_graphid];

				if ($graph['templateid'] != 0) {
					self::exception(ZBX_API_ERROR_PARAMETERS, _s(
						'Graph "%1$s" already exists on "%2$s" (inherited from another template).',
						$graph['name'], $hosts_names[$hostid]
					));
				}
				elseif  ($graph['flags'] & ZBX_FLAG_DISCOVERY_CREATED) {
					self::exception(ZBX_API_ERROR_PARAMETERS,
						_('Graph with same name but other type exist.')
					);
				}

				if (count($graph['gitems']) === count($parent_graph['gitems'])) {
					$gitems_itemids = array_column($graphs['gitems'], 'itemid');

					foreach ($parent_graph['gitems'] as $parent_gitem) {
						$index = array_search($template_itemids_hostids_itemids[$parent_gitem['itemid']][$hostid],
							$gitems_itemids
						);

						if ($index === false) {
							self::exception(ZBX_API_ERROR_PARAMETERS,
								_s('Graph "%1$s" already exists on "%2$s" (items are not identical).',
									$parent_graph['name'], $hosts_names[$hostid]
								)
							);
						}

						unset($gitems_itemids[$index]);
					}

					// $child_graph_to_update = ['graphid' => $child_graphid, 'templateid' => $graphid] + $graph;
					// ...
					// $child_graphs_to_update[] = $child_graph_to_update;

					$parent_graphids_updated_hosts[$parent_graphid][$hostid] = true;
				}
				else {
					self::exception(ZBX_API_ERROR_PARAMETERS,
						_s('Graph "%1$s" already exists on "%2$s" (items are not identical).', $parent_graph['name'],
							$hosts_names[$hostid]
						)
					);
				}
			}

			foreach ($graphs as $graphid => $graph) {
				$itemid = reset($graph['gitems'])['itemid'];
				$templateid = $itemids_templateids[$itemid];

				$hosts_to_create_graph = array_diff_key($templateids_hosts[$templateid],
					$parent_graphids_updated_hosts[$graphid]
				);

				foreach (array_keys($hosts_to_create_graph) as $hostid) {
					$child_graph_to_add = ['templateid' => $graphid] + array_diff_key($graph, ['graphid' => true]);

					if ($graph['ymin_itemid'] != 0) {
						$child_graph_to_add['ymin_itemid'] =
							$template_itemids_hostids_itemids[$graph['ymin_itemid']][$hostid];
					}

					if ($graph['ymax_itemid'] != 0) {
						$child_graph_to_add['ymax_itemid'] =
							$template_itemids_hostids_itemids[$graph['ymax_itemid']][$hostid];
					}

					foreach ($child_graph_to_add['gitems'] as &$gitem) {
						$gitem['itemid'] = $template_itemids_hostids_itemids[$gitem['itemid']][$hostid];
					}
					unset($gitem);

					$child_graphs_to_add[] = $child_graph_to_add;
				}
			}
		}

		if ($child_graphs_to_add) {
			$this->createReal($child_graphs_to_add);
			$child_graphs_to_add = zbx_toHash($child_graphs_to_add, 'graphid');
		}

		if ($child_graphs_to_update) {
			$this->updateReal($child_graphs_to_update);
		}

		$this->inherit($child_graphs_to_add + $child_graphs_to_update);
	}

	/**
	 * Inherit template graphs from template to host.
	 *
	 * @param array $data
	 *
	 * @return bool
	 */
	public function syncTemplates($data) {
		$data['templateids'] = zbx_toArray($data['templateids']);
		$data['hostids'] = zbx_toArray($data['hostids']);

		$dbLinks = DBSelect(
			'SELECT ht.hostid,ht.templateid'.
			' FROM hosts_templates ht'.
			' WHERE '.dbConditionInt('ht.hostid', $data['hostids']).
				' AND '.dbConditionInt('ht.templateid', $data['templateids'])
		);
		$linkage = [];
		while ($link = DBfetch($dbLinks)) {
			if (!isset($linkage[$link['templateid']])) {
				$linkage[$link['templateid']] = [];
			}
			$linkage[$link['templateid']][$link['hostid']] = 1;
		}

		$graphs = $this->get([
			'output' => ['graphid', 'name', 'width', 'height', 'yaxismin', 'yaxismax', 'templateid',
				'show_work_period', 'show_triggers', 'graphtype', 'show_legend', 'show_3d', 'percent_left',
				'percent_right', 'ymin_type', 'ymax_type', 'ymin_itemid', 'ymax_itemid', 'flags', 'discover'
			],
			'selectGraphItems' => ['gitemid', 'graphid', 'itemid', 'drawtype', 'sortorder', 'color', 'yaxisside',
				'calc_fnc', 'type'
			],
			'selectHosts' => ['hostid'],
			'hostids' => $data['templateids'],
			'preservekeys' => true
		]);

		$inherit_graphs = [];
		foreach ($graphs as $graph) {
			foreach ($data['hostids'] as $hostid) {
				if (array_key_exists($graph['hosts'][0]['hostid'], $linkage)
						&& array_key_exists($hostid, $linkage[$graph['hosts'][0]['hostid']])) {
					$inherit_graphs[] = $graph;
				}
			}
		}

		if ($inherit_graphs) {
			$this->inherit($inherit_graphs, $data['hostids']);
		}

		return true;
	}

	/**
	 * Delete graphs.
	 *
	 * @param array $graphids
	 *
	 * @return array
	 */
	public function delete(array $graphids) {
		$this->validateDelete($graphids, $db_graphs);

		CGraphManager::delete($graphids);

		$this->addAuditBulk(AUDIT_ACTION_DELETE, AUDIT_RESOURCE_GRAPH, $db_graphs);

		return ['graphids' => $graphids];
	}

	/**
	 * Validates the input parameters for the delete() method.
	 *
	 * @param array $graphids   [IN/OUT]
	 * @param array $db_graphs  [OUT]
	 *
	 * @throws APIException if the input is invalid.
	 */
	private function validateDelete(array &$graphids, array &$db_graphs = null) {
		$api_input_rules = ['type' => API_IDS, 'flags' => API_NOT_EMPTY, 'uniq' => true];
		if (!CApiInputValidator::validate($api_input_rules, $graphids, '/', $error)) {
			self::exception(ZBX_API_ERROR_PARAMETERS, $error);
		}

		$db_graphs = $this->get([
			'output' => ['graphid', 'name', 'templateid'],
			'graphids' => $graphids,
			'editable' => true,
			'preservekeys' => true
		]);

		foreach ($graphids as $graphid) {
			if (!array_key_exists($graphid, $db_graphs)) {
				self::exception(ZBX_API_ERROR_PERMISSIONS,
					_('No permissions to referred object or it does not exist!')
				);
			}

			if ($db_graphs[$graphid]['templateid'] != 0) {
				self::exception(ZBX_API_ERROR_PARAMETERS, _('Cannot delete templated graph.'));
			}
		}
	}

	protected function addRelatedObjects(array $options, array $result) {
		$result = parent::addRelatedObjects($options, $result);

		$graphids = array_keys($result);

		// adding Items
		if ($options['selectItems'] !== null && $options['selectItems'] !== API_OUTPUT_COUNT) {
			$relationMap = $this->createRelationMap($result, 'graphid', 'itemid', 'graphs_items');
			$items = API::Item()->get([
				'output' => $options['selectItems'],
				'itemids' => $relationMap->getRelatedIds(),
				'webitems' => true,
				'nopermissions' => true,
				'preservekeys' => true
			]);
			$result = $relationMap->mapMany($result, $items, 'items');
		}

		// adding discoveryRule
		if ($options['selectDiscoveryRule'] !== null) {
			$dbRules = DBselect(
				'SELECT id.parent_itemid,gd.graphid'.
					' FROM graph_discovery gd,item_discovery id,graphs_items gi,items i'.
					' WHERE '.dbConditionInt('gd.graphid', $graphids).
					' AND gd.parent_graphid=gi.graphid'.
						' AND gi.itemid=id.itemid'.
						' AND id.parent_itemid=i.itemid'.
						' AND i.flags='.ZBX_FLAG_DISCOVERY_RULE
			);
			$relationMap = new CRelationMap();
			while ($relation = DBfetch($dbRules)) {
				$relationMap->addRelation($relation['graphid'], $relation['parent_itemid']);
			}

			$discoveryRules = API::DiscoveryRule()->get([
				'output' => $options['selectDiscoveryRule'],
				'itemids' => $relationMap->getRelatedIds(),
				'nopermissions' => true,
				'preservekeys' => true
			]);
			$result = $relationMap->mapOne($result, $discoveryRules, 'discoveryRule');
		}

		// adding graph discovery
		if ($options['selectGraphDiscovery'] !== null) {
			$graphDiscoveries = API::getApiService()->select('graph_discovery', [
				'output' => $this->outputExtend($options['selectGraphDiscovery'], ['graphid']),
				'filter' => ['graphid' => array_keys($result)],
				'preservekeys' => true
			]);
			$relationMap = $this->createRelationMap($graphDiscoveries, 'graphid', 'graphid');

			$graphDiscoveries = $this->unsetExtraFields($graphDiscoveries, ['graphid'],
				$options['selectGraphDiscovery']
			);
			$result = $relationMap->mapOne($result, $graphDiscoveries, 'graphDiscovery');
		}

		return $result;
	}

	/**
	 * Validate create.
	 *
	 * @param array $graphs
	 */
	protected function validateCreate(array $graphs) {
		$itemIds = $this->validateItemsCreate($graphs);
		$this->validateItems($itemIds, $graphs);

		parent::validateCreate($graphs);
	}

	/**
	 * Validate update.
	 *
	 * @param array $graphs
	 * @param array $dbGraphs
	 */
	protected function validateUpdate(array $graphs, array $dbGraphs) {
		// check for "itemid" when updating graph with only "gitemid" passed
		foreach ($graphs as &$graph) {
			if (isset($graph['gitems'])) {
				foreach ($graph['gitems'] as &$gitem) {
					if (isset($gitem['gitemid']) && !isset($gitem['itemid'])) {
						$dbGitems = zbx_toHash($dbGraphs[$graph['graphid']]['gitems'], 'gitemid');
						$gitem['itemid'] = $dbGitems[$gitem['gitemid']]['itemid'];
					}
				}
				unset($gitem);
			}
		}
		unset($graph);

		$itemIds = $this->validateItemsUpdate($graphs);
		$this->validateItems($itemIds, $graphs);

		parent::validateUpdate($graphs, $dbGraphs);
	}

	/**
	 * Validates items.
	 *
	 * @param array $itemIds
	 * @param array $graphs
	 */
	protected function validateItems(array $itemIds, array $graphs) {
		$dbItems = API::Item()->get([
			'output' => ['name', 'value_type'],
			'itemids' => $itemIds,
			'webitems' => true,
			'editable' => true,
			'preservekeys' => true
		]);

		// check if items exist and user has permission to access those items
		foreach ($itemIds as $itemId) {
			if (!isset($dbItems[$itemId])) {
				self::exception(ZBX_API_ERROR_PARAMETERS, _('No permissions to referred object or it does not exist!'));
			}
		}

		$allowedValueTypes = [ITEM_VALUE_TYPE_FLOAT, ITEM_VALUE_TYPE_UINT64];

		// get value type and name for these items
		foreach ($graphs as $graph) {
			// graph items
			foreach ($graph['gitems'] as $graphItem) {
				$item = $dbItems[$graphItem['itemid']];

				if (!in_array($item['value_type'], $allowedValueTypes)) {
					self::exception(ZBX_API_ERROR_PARAMETERS, _s(
						'Cannot add a non-numeric item "%1$s" to graph "%2$s".',
						$item['name'],
						$graph['name']
					));
				}
			}

			// Y axis min
			if (isset($graph['ymin_itemid']) && $graph['ymin_itemid']
					&& isset($graph['ymin_type']) && $graph['ymin_type'] == GRAPH_YAXIS_TYPE_ITEM_VALUE) {
				$item = $dbItems[$graph['ymin_itemid']];

				if (!in_array($item['value_type'], $allowedValueTypes)) {
					self::exception(ZBX_API_ERROR_PARAMETERS, _s(
						'Cannot add a non-numeric item "%1$s" to graph "%2$s".',
						$item['name'],
						$graph['name']
					));
				}
			}

			// Y axis max
			if (isset($graph['ymax_itemid']) && $graph['ymax_itemid']
					&& isset($graph['ymax_type']) && $graph['ymax_type'] == GRAPH_YAXIS_TYPE_ITEM_VALUE) {
				$item = $dbItems[$graph['ymax_itemid']];

				if (!in_array($item['value_type'], $allowedValueTypes)) {
					self::exception(ZBX_API_ERROR_PARAMETERS, _s(
						'Cannot add a non-numeric item "%1$s" to graph "%2$s".',
						$item['name'],
						$graph['name']
					));
				}
			}
		}
	}
}
