/*
 * rlm_eap_teap.c  contains the interfaces that are called from eap
 *
 * Version:     $Id: 5f2a8db04cbc7670167c3b6d2b09d7b3b7955bca $
 *
 * Copyright (C) 2022 Network RADIUS SARL <legal@networkradius.com>
 *
 * This software may not be redistributed in any form without the prior
 * written consent of Network RADIUS.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

RCSID("$Id: 5f2a8db04cbc7670167c3b6d2b09d7b3b7955bca $")
USES_APPLE_DEPRECATED_API	/* OpenSSL API has been deprecated by Apple */

#include "eap_teap.h"

typedef struct rlm_eap_teap_t {
	/*
	 *	TLS configuration
	 */
	char const *tls_conf_name;
	fr_tls_server_conf_t *tls_conf;

	/*
	 *	Default tunneled EAP type
	 */
	char const *default_method_name;
	int default_method;

	/*
	 *	Use the reply attributes from the tunneled session in
	 *	the non-tunneled reply to the client.
	 */
	bool use_tunneled_reply;

	/*
	 *	Use SOME of the request attributes from outside of the
	 *	tunneled session in the tunneled request
	 */
	bool copy_request_to_tunnel;

	/*
	 * 	Do we do require a client cert?
	 */
	bool req_client_cert;

	uint32_t pac_lifetime;
	char const *authority_identity;
	char const *pac_opaque_key;

	/*
	 *	Virtual server for inner tunnel session.
	 */
	char const *virtual_server;
} rlm_eap_teap_t;


static CONF_PARSER module_config[] = {
	{ "tls", FR_CONF_OFFSET(PW_TYPE_STRING, rlm_eap_teap_t, tls_conf_name), NULL },
	{ "default_eap_type", FR_CONF_OFFSET(PW_TYPE_STRING, rlm_eap_teap_t, default_method_name), "md5" },
	{ "copy_request_to_tunnel", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_eap_teap_t, copy_request_to_tunnel), "no" },
	{ "use_tunneled_reply", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_eap_teap_t, use_tunneled_reply), "no" },
	{ "require_client_cert", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_eap_teap_t, req_client_cert), "no" },
	{ "pac_lifetime", FR_CONF_OFFSET(PW_TYPE_INTEGER, rlm_eap_teap_t, pac_lifetime), "604800" },
	{ "authority_identity", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_REQUIRED, rlm_eap_teap_t, authority_identity), NULL },
	{ "pac_opaque_key", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_REQUIRED, rlm_eap_teap_t, pac_opaque_key), NULL },
	{ "virtual_server", FR_CONF_OFFSET(PW_TYPE_STRING, rlm_eap_teap_t, virtual_server), NULL },
	CONF_PARSER_TERMINATOR
};


/*
 *	Attach the module.
 */
static int mod_instantiate(CONF_SECTION *cs, void **instance)
{
	rlm_eap_teap_t		*inst;

	*instance = inst = talloc_zero(cs, rlm_eap_teap_t);
	if (!inst) return -1;

	/*
	 *	Parse the configuration attributes.
	 */
	if (cf_section_parse(cs, inst, module_config) < 0) {
		return -1;
	}

	if (!inst->virtual_server) {
		ERROR("rlm_eap_teap: A 'virtual_server' MUST be defined for security");
		return -1;
	}

	/*
	 *	Convert the name to an integer, to make it easier to
	 *	handle.
	 */
	if (inst->default_method_name && *inst->default_method_name) {
		inst->default_method = eap_name2type(inst->default_method_name);
		if (inst->default_method < 0) {
			ERROR("rlm_eap_teap: Unknown EAP type %s",
			      inst->default_method_name);
		return -1;
		}
	}

	/*
	 *	Read tls configuration, either from group given by 'tls'
	 *	option, or from the eap-tls configuration.
	 */
	inst->tls_conf = eaptls_conf_parse(cs, "tls");

	if (!inst->tls_conf) {
		ERROR("rlm_eap_teap: Failed initializing SSL context");
		return -1;
	}

	return 0;
}

/*
 *	Allocate the TEAP per-session data
 */
static teap_tunnel_t *teap_alloc(TALLOC_CTX *ctx, rlm_eap_teap_t *inst)
{
	teap_tunnel_t *t;

	t = talloc_zero(ctx, teap_tunnel_t);

	t->received_version = -1;
	t->default_method = inst->default_method;
	t->copy_request_to_tunnel = inst->copy_request_to_tunnel;
	t->use_tunneled_reply = inst->use_tunneled_reply;
	t->virtual_server = inst->virtual_server;
	return t;
}


/*
 *	Send an initial eap-tls request to the peer, using the libeap functions.
 */
static int mod_session_init(void *type_arg, eap_handler_t *handler)
{
	int		status;
	tls_session_t	*ssn;
	rlm_eap_teap_t	*inst;
	VALUE_PAIR	*vp;
	bool		client_cert;
	REQUEST		*request = handler->request;

	inst = type_arg;

	handler->tls = true;

	/*
	 *	Check if we need a client certificate.
	 */

	/*
	 * EAP-TLS-Require-Client-Cert attribute will override
	 * the require_client_cert configuration option.
	 */
	vp = fr_pair_find_by_num(handler->request->config, PW_EAP_TLS_REQUIRE_CLIENT_CERT, 0, TAG_ANY);
	if (vp) {
		client_cert = vp->vp_integer ? true : false;
	} else {
		client_cert = inst->req_client_cert;
	}

	/*
	 *	Disallow TLS 1.3 for now.
	 */
	ssn = eaptls_session(handler, inst->tls_conf, client_cert, false);
	if (!ssn) {
		return 0;
	}

	handler->opaque = ((void *)ssn);

	/*
	 *	As TEAP is a unique special snowflake and wants to use its
	 *	own rolling MSK for MPPE we we set the label to NULL so in that
	 *	eaptls_gen_mppe_keys() is NOT called in eaptls_success.
	 */
	ssn->label = NULL;

	/*
	 *	Really just protocol version.
	 */
	ssn->peap_flag = EAP_TEAP_VERSION;

        /*
	 *	hostapd's wpa_supplicant gets upset if we include all the
	 *	S+L+O flags but is happy with S+O (TLS payload is zero bytes
	 *	for S anyway) - FIXME not true for early-data TLSv1.3!
	 */
	ssn->length_flag = false;

	vp = fr_pair_make(ssn, NULL, "FreeRADIUS-EAP-TEAP-Authority-ID", inst->authority_identity, T_OP_EQ);
	fr_pair_add(&ssn->outer_tlvs, vp);

	/*
	 *	TLS session initialization is over.  Now handle TLS
	 *	related handshaking or application data.
	 */
	status = eaptls_request(handler->eap_ds, ssn, true);
	if ((status == FR_TLS_INVALID) || (status == FR_TLS_FAIL)) {
		REDEBUG("[eaptls start] = %s", fr_int2str(fr_tls_status_table, status, "<INVALID>"));
	} else {
		RDEBUG3("[eaptls start] = %s", fr_int2str(fr_tls_status_table, status, "<INVALID>"));
	}
	if (status == 0) return 0;

	/*
	 *	The next stage to process the packet.
	 */
	handler->stage = PROCESS;

	return 1;
}


/*
 *	Do authentication, by letting EAP-TLS do most of the work.
 */
static int mod_process(void *arg, eap_handler_t *handler)
{
	int rcode;
	int ret = 0;
	fr_tls_status_t	status;
	rlm_eap_teap_t *inst = (rlm_eap_teap_t *) arg;
	tls_session_t *tls_session = (tls_session_t *) handler->opaque;
	teap_tunnel_t *t = (teap_tunnel_t *) tls_session->opaque;
	REQUEST *request = handler->request;

	RDEBUG2("Authenticate");

	/*
	 *	Process TLS layer until done.
	 */
	status = eaptls_process(handler);
	if ((status == FR_TLS_INVALID) || (status == FR_TLS_FAIL)) {
		REDEBUG("[eaptls process] = %s", fr_int2str(fr_tls_status_table, status, "<INVALID>"));
	} else {
		RDEBUG3("[eaptls process] = %s", fr_int2str(fr_tls_status_table, status, "<INVALID>"));
	}

	/*
	 *	Make request available to any SSL callbacks
	 */
	SSL_set_ex_data(tls_session->ssl, FR_TLS_EX_INDEX_REQUEST, request);
	switch (status) {
	/*
	 *	EAP-TLS handshake was successful, tell the
	 *	client to keep talking.
	 *
	 *	If this was EAP-TLS, we would just return
	 *	an EAP-TLS-Success packet here.
	 */
	case FR_TLS_SUCCESS:
		if (SSL_session_reused(tls_session->ssl)) {
			RDEBUG("Skipping Phase2 due to session resumption");
			goto do_keys;
		}

		if (t && t->authenticated) {
			if (t->accept_vps) {
				RDEBUG2("Using saved attributes from the original Access-Accept");
				rdebug_pair_list(L_DBG_LVL_2, request, t->accept_vps, NULL);
				fr_pair_list_mcopy_by_num(handler->request->reply,
					   &handler->request->reply->vps,
					   &t->accept_vps, 0, 0, TAG_ANY);
			} else if (t->use_tunneled_reply) {
				RDEBUG2("No saved attributes in the original Access-Accept");
			}

		do_keys:
			/*
			 *	Success: Automatically return MPPE keys.
			 */
			ret = eaptls_success(handler, 0);
			goto done;
		}
		goto phase2;

	/*
	 *	The TLS code is still working on the TLS
	 *	exchange, and it's a valid TLS request.
	 *	do nothing.
	 */
	case FR_TLS_HANDLED:
		ret = 1;
		goto done;

	/*
	 *	Handshake is done, proceed with decoding tunneled
	 *	data.
	 */
	case FR_TLS_OK:
		break;

	/*
	 *	Anything else: fail.
	 */
	default:
		ret = 0;
		goto done;
	}

phase2:
	/*
	 *	Session is established, proceed with decoding
	 *	tunneled data.
	 */
	RDEBUG2("Session established.  Proceeding to decode tunneled attributes");

	/*
	 *	We may need TEAP data associated with the session, so
	 *	allocate it here, if it wasn't already alloacted.
	 */
	if (!tls_session->opaque) {
		tls_session->opaque = teap_alloc(tls_session, inst);
		t = (teap_tunnel_t *) tls_session->opaque;
		if (t->received_version < 0) t->received_version = handler->eap_ds->response->type.data[0] & 0x07;
	}

	/*
	 *	Process the TEAP portion of the request.
	 */
	rcode = eap_teap_process(handler, tls_session);
	switch (rcode) {
	case PW_CODE_ACCESS_REJECT:
		eaptls_fail(handler, 0);
		ret = 0;
		goto done;

		/*
		 *	Access-Challenge, continue tunneled conversation.
		 */
	case PW_CODE_ACCESS_CHALLENGE:
		eaptls_request(handler->eap_ds, tls_session, false);
		ret = 1;
		goto done;

		/*
		 *	Success: Automatically return MPPE keys.
		 */
	case PW_CODE_ACCESS_ACCEPT:
		goto do_keys;

	default:
		break;
	}

	/*
	 *	Something we don't understand: Reject it.
	 */
	eaptls_fail(handler, 0);

done:
	SSL_set_ex_data(tls_session->ssl, FR_TLS_EX_INDEX_REQUEST, NULL);

	return ret;
}

/*
 *	The module name should be the only globally exported symbol.
 *	That is, everything else should be 'static'.
 */
extern rlm_eap_module_t rlm_eap_teap;
rlm_eap_module_t rlm_eap_teap = {
	.name		= "eap_teap",
	.instantiate	= mod_instantiate,	/* Create new submodule instance */
	.session_init	= mod_session_init,	/* Initialise a new EAP session */
	.process	= mod_process		/* Process next round of EAP method */
};
