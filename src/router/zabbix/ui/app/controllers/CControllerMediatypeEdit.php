<?php
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


class CControllerMediatypeEdit extends CController {

	private $mediatype = [];

	protected function init() {
		$this->disableCsrfValidation();
	}

	protected function checkInput() {
		$fields = [
			'mediatypeid' =>			'db media_type.mediatypeid',
			'type' =>					'db media_type.type|in '.implode(',', array_keys(CMediatypeHelper::getMediaTypes())),
			'name' =>					'db media_type.name',
			'smtp_server' =>			'db media_type.smtp_server',
			'smtp_port' =>				'db media_type.smtp_port',
			'smtp_helo' =>				'db media_type.smtp_helo',
			'smtp_email' =>				'db media_type.smtp_email',
			'smtp_security' =>			'db media_type.smtp_security|in '.SMTP_CONNECTION_SECURITY_NONE.','.SMTP_CONNECTION_SECURITY_STARTTLS.','.SMTP_CONNECTION_SECURITY_SSL_TLS,
			'smtp_verify_peer' =>		'db media_type.smtp_verify_peer|in 0,1',
			'smtp_verify_host' =>		'db media_type.smtp_verify_host|in 0,1',
			'smtp_authentication' =>	'db media_type.smtp_authentication|in '.SMTP_AUTHENTICATION_NONE.','.SMTP_AUTHENTICATION_NORMAL,
			'exec_path' =>				'db media_type.exec_path',
			'gsm_modem' =>				'db media_type.gsm_modem',
			'smtp_username' =>			'db media_type.username',
			'passwd' =>					'db media_type.passwd',
			'parameters_exec' =>		'array',
			'parameters_webhook' =>		'array',
			'script' => 				'db media_type.script',
			'timeout' => 				'db media_type.timeout',
			'process_tags' =>			'in '.ZBX_MEDIA_TYPE_TAGS_DISABLED.','.ZBX_MEDIA_TYPE_TAGS_ENABLED,
			'show_event_menu' =>		'in '.ZBX_EVENT_MENU_HIDE.','.ZBX_EVENT_MENU_SHOW,
			'event_menu_url' =>			'db media_type.event_menu_url',
			'event_menu_name' =>		'db media_type.event_menu_name',
			'status' =>					'db media_type.status|in '.MEDIA_TYPE_STATUS_ACTIVE.','.MEDIA_TYPE_STATUS_DISABLED,
			'maxsessions' =>			'db media_type.maxsessions',
			'maxattempts' =>			'db media_type.maxattempts',
			'attempt_interval' =>		'db media_type.attempt_interval',
			'description' =>			'db media_type.description',
			'form_refresh' =>			'int32',
			'content_type' =>			'db media_type.content_type|in '.SMTP_MESSAGE_FORMAT_PLAIN_TEXT.','.SMTP_MESSAGE_FORMAT_HTML,
			'message_templates' =>		'array',
			'provider' =>				'int32| in '.implode(',', array_keys(CMediatypeHelper::getEmailProviders()))
		];

		$ret = $this->validateInput($fields);

		if (!$ret) {
			$this->setResponse(new CControllerResponseFatal());
		}

		return $ret;
	}

	protected function checkPermissions() {
		if (!$this->checkAccess(CRoleHelper::UI_ADMINISTRATION_MEDIA_TYPES)) {
			return false;
		}

		if ($this->hasInput('mediatypeid')) {
			$mediatypes = API::Mediatype()->get([
				'output' => ['mediatypeid', 'type', 'name', 'smtp_server', 'smtp_port', 'smtp_helo', 'smtp_email',
					'exec_path', 'gsm_modem', 'username', 'passwd', 'status', 'smtp_security', 'smtp_verify_peer',
					'smtp_verify_host', 'smtp_authentication', 'maxsessions', 'maxattempts',
					'attempt_interval', 'content_type', 'script', 'timeout', 'process_tags', 'show_event_menu',
					'event_menu_url', 'event_menu_name', 'parameters', 'description', 'provider'
				],
				'selectMessageTemplates' => ['eventsource', 'recovery', 'subject', 'message'],
				'mediatypeids' => $this->getInput('mediatypeid'),
				'editable' => true
			]);

			if (!$mediatypes) {
				return false;
			}

			$this->mediatype = $mediatypes[0];
		}

		return true;
	}

	protected function doAction() {
		// default values
		$db_defaults = DB::getDefaults('media_type');
		$email_defaults =  CMediatypeHelper::getEmailProviders(CMediatypeHelper::EMAIL_PROVIDER_SMTP);

		$data = [
			'mediatypeid' => 0,
			'type' => MEDIA_TYPE_EMAIL,
			'provider' => CMediatypeHelper::EMAIL_PROVIDER_SMTP,
			'name' => '',
			'smtp_server' => $email_defaults['smtp_server'],
			'smtp_port' => $email_defaults['smtp_port'],
			'smtp_helo' => $db_defaults['smtp_helo'],
			'smtp_email' => $email_defaults['smtp_email'],
			'smtp_security' => $email_defaults['smtp_security'],
			'smtp_verify_peer' => $email_defaults['smtp_verify_peer'],
			'smtp_verify_host' => $email_defaults['smtp_verify_host'],
			'smtp_authentication' => $email_defaults['smtp_authentication'],
			'exec_path' => '',
			'gsm_modem' => '/dev/ttyS0',
			'smtp_username' => '',
			'passwd' => '',
			'status' => MEDIA_TYPE_STATUS_ACTIVE,
			'change_passwd' => true,
			'maxsessions' => $db_defaults['maxsessions'],
			'maxattempts' => $db_defaults['maxattempts'],
			'attempt_interval' => $db_defaults['attempt_interval'],
			'script' => $db_defaults['script'],
			'timeout' => $db_defaults['timeout'],
			'process_tags' => $db_defaults['process_tags'],
			'show_event_menu' => $db_defaults['show_event_menu'],
			'event_menu_url' => $db_defaults['event_menu_url'],
			'event_menu_name' => $db_defaults['event_menu_name'],
			'parameters_exec' => [],
			'parameters_webhook' => [
				['name' => 'URL', 'value'=> ''],
				['name' => 'HTTPProxy', 'value'=> ''],
				['name' => 'To', 'value' => '{ALERT.SENDTO}'],
				['name' => 'Subject', 'value' => '{ALERT.SUBJECT}'],
				['name' => 'Message', 'value' => '{ALERT.MESSAGE}']
			],
			'description' => '',
			'form_refresh' => 0,
			'content_type' => $email_defaults['content_type'],
			'message_templates' => [],
			'providers' => CMediatypeHelper::getEmailProviders()
		];
		$message_templates = [];

		// get values from the database
		if ($this->hasInput('mediatypeid')) {
			$data['mediatypeid'] = $this->mediatype['mediatypeid'];
			$data['type'] = $this->mediatype['type'];
			$data['name'] = $this->mediatype['name'];
			$data['smtp_server'] = $this->mediatype['smtp_server'];
			$data['smtp_port'] = $this->mediatype['smtp_port'];
			$data['smtp_helo'] = $this->mediatype['smtp_helo'];
			$data['smtp_email'] = $this->mediatype['smtp_email'];
			$data['smtp_security'] = $this->mediatype['smtp_security'];
			$data['smtp_verify_peer'] = $this->mediatype['smtp_verify_peer'];
			$data['smtp_verify_host'] = $this->mediatype['smtp_verify_host'];
			$data['smtp_authentication'] = $this->mediatype['smtp_authentication'];
			$data['exec_path'] = $this->mediatype['exec_path'];
			$data['content_type'] = $this->mediatype['content_type'];
			$data['description'] = $this->mediatype['description'];
			$data['provider'] = $this->mediatype['provider'];
			$message_templates = $this->mediatype['message_templates'];

			$data['gsm_modem'] = $this->mediatype['gsm_modem'];
			$data['passwd'] = $this->mediatype['passwd'];
			$data['status'] = $this->mediatype['status'];
			$data['maxsessions'] = $this->mediatype['maxsessions'];
			$data['maxattempts'] = $this->mediatype['maxattempts'];
			$data['attempt_interval'] = $this->mediatype['attempt_interval'];

			switch ($data['type']) {
				case MEDIA_TYPE_EMAIL:
					$data['smtp_username'] = $this->mediatype['username'];
					break;

				case MEDIA_TYPE_EXEC:
					$data['parameters_exec'] = $this->mediatype['parameters'];
					break;

				case MEDIA_TYPE_SMS:
					$data['maxsessions'] = 1;
					break;

				case MEDIA_TYPE_WEBHOOK:
					$data['script'] = $this->mediatype['script'];
					$data['timeout'] = $this->mediatype['timeout'];
					$data['process_tags'] = $this->mediatype['process_tags'];
					$data['show_event_menu'] = $this->mediatype['show_event_menu'];
					$data['event_menu_url'] = $this->mediatype['event_menu_url'];
					$data['event_menu_name'] = $this->mediatype['event_menu_name'];
					$data['parameters_webhook'] = $this->mediatype['parameters'];
					CArrayHelper::sort($data['parameters_webhook'], ['name']);
					break;
			}

			$data['change_passwd'] = $this->hasInput('passwd');
		}

		// overwrite with input variables
		$this->getInputs($data, ['type', 'name', 'smtp_server', 'smtp_port', 'smtp_helo', 'smtp_email', 'smtp_security',
			'smtp_verify_peer', 'smtp_verify_host', 'smtp_authentication', 'exec_path', 'gsm_modem',
			'smtp_username', 'passwd', 'status', 'maxsessions', 'maxattempts', 'attempt_interval', 'maxsessionsType',
			'form_refresh', 'content_type', 'script', 'timeout', 'process_tags', 'show_event_menu', 'event_menu_url',
			'event_menu_name', 'description', 'provider'
		]);

		if ($this->hasInput('form_refresh')) {
			$data['parameters_exec'] = [];
			foreach (array_values($this->getInput('parameters_exec', [])) as $sortorder => $parameter) {
				$data['parameters_exec'][] = ['sortorder' => $sortorder, 'value' => $parameter['value']];
			}

			$data['parameters_webhook'] = [];
			$parameters = $this->getInput('parameters_webhook', ['name' => [], 'value' => []]);
			$name = reset($parameters['name']);
			$value = reset($parameters['value']);

			while ($name !== false) {
				$data['parameters_webhook'][] = compact('name', 'value');
				$name = next($parameters['name']);
				$value = next($parameters['value']);
			}

			$message_templates = $this->getInput('message_templates', []);
		}

		if ($message_templates) {
			CArrayHelper::sort($message_templates, ['recovery']);

			// Sort message templates in a certain order by event source.
			foreach ([EVENT_SOURCE_TRIGGERS, EVENT_SOURCE_SERVICE, EVENT_SOURCE_DISCOVERY,
					EVENT_SOURCE_AUTOREGISTRATION, EVENT_SOURCE_INTERNAL] as $eventsource) {
				foreach ($message_templates as $index => $message_template) {
					if ($message_template['eventsource'] == $eventsource) {
						$data['message_templates'][] = $message_template;
						unset($message_templates[$index]);
					}
				}
			}
		}

		$response = new CControllerResponseData($data);
		$response->setTitle(_('Configuration of media types'));
		$this->setResponse($response);
	}
}
