<?php declare(strict_types = 0);
/*
** Zabbix
** Copyright (C) 2001-2024 Zabbix SIA
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


class CControllerProxyHostDisable extends CController {

	protected function init(): void {
		$this->setPostContentType(self::POST_CONTENT_TYPE_JSON);
	}

	protected function checkInput(): bool {
		$fields = [
			'proxyids' => 'required|array_id'
		];

		$ret = $this->validateInput($fields);

		if (!$ret) {
			$this->setResponse(
				new CControllerResponseData(['main_block' => json_encode([
					'error' => [
						'messages' => array_column(get_and_clear_messages(), 'message')
					]
				])])
			);
		}

		return $ret;
	}

	protected function checkPermissions(): bool {
		if (!$this->checkAccess(CRoleHelper::UI_ADMINISTRATION_PROXIES)) {
			return false;
		}

		$num_proxies = API::Proxy()->get([
			'proxyids' => $this->getInput('proxyids'),
			'countOutput' => true,
			'editable' => true
		]);

		return $num_proxies == count($this->getInput('proxyids'));
	}

	protected function doAction() {
		$hosts = API::Host()->get([
			'output' => ['hostid'],
			'filter' => [
				'proxy_hostid' => $this->getInput('proxyids'),
				'status' => HOST_STATUS_MONITORED
			]
		]);

		$update = [];

		foreach ($hosts as $host) {
			$update[] = [
				'hostid' => $host['hostid'],
				'status' => HOST_STATUS_NOT_MONITORED
			];
		}

		$result = !$update || API::Host()->update($update);

		$output = [];

		if ($result) {
			$output['success']['title'] = _n('Host disabled', 'Hosts disabled', count($update));

			if ($messages = get_and_clear_messages()) {
				$output['success']['messages'] = array_column($messages, 'message');
			}
		}
		else {
			$output['error'] = [
				'title' => _n('Cannot disable host', 'Cannot disable hosts', count($update)),
				'messages' => array_column(get_and_clear_messages(), 'message')
			];
		}

		$this->setResponse(new CControllerResponseData(['main_block' => json_encode($output)]));
	}
}
