<% do_hpagehead("idx.titl"); %>
	<body class="help-bg">
		<div id="header">
			<div class="logo"> </div>
			<div class="navig"><a href="index.asp">Index</a> | <a href="javascript:self.close();"><% tran("sbutton.clos"); %></a></div>
		</div>
		<div id="content">
			<h2><% tran("bmenu.setupbasic"); %></h2>
			<dl>
				<% tran("hidx.intro"); %>
				<dt><% tran("idx.legend"); %></dt>
				<% tran("hidx.wanctype"); %>
				<dt><% tran("idx.ignore_wan_dns"); %></dt>
				<% tran("hidx.ignwandns"); %>
				<dt><% tran("share.routername"); %></dt>
				<% tran("hidx.rname"); %>
				<dt><% tran("share.hostname"); %></dt>
				<% tran("hidx.hname"); %>
				<dt><% tran("share.domainname"); %></dt>
				<% tran("hidx.dname"); %>
				<dt><% tran("idx.mtu"); %></dt>
				<% tran("hidx.mtu"); %>
				<dt><% tran("idx.sfe"); %></dt>
				<% tran("hidx.sfe"); %>
				<dt><% tran("idx.fa"); %></dt>
				<% tran("hidx.fa"); %>
				<dt><% tran("idx.stp"); %></dt>
				<% tran("hidx.stp"); %>
				<dt><% tran("idx_pptp.wan_ip"); %> / <% tran("share.subnet"); %></dt>
				<% tran("hidx.page5"); %>
				<dt><% tran("share.gateway"); %></dt>
				<% tran("hidx.page6"); %>
				<dt><% tran("idx_static.dns"); %></dt>
				<% tran("hidx.page7"); %>
				<dt><% tran("share.usrname"); %> / <% tran("share.passwd"); %></dt>
				<% tran("hidx.page8"); %>
				<dt><% tran("share.compression"); %></dt>
				<% tran("hidx.page9"); %>
				<dt><% tran("service.pptpd_encry"); %></dt>
				<% tran("hidx.page10"); %>
				<dt><% tran("idx_h.reconnect"); %></dt>
				<% tran("hidx.page11"); %>
				<dt><% tran("idx_h.max_idle"); %></dt>
				<% tran("hidx.page12"); %>
				<dt><% tran("idx_h.alive"); %></dt>
				<% tran("hidx.page13"); %>
				<dt><% tran("idx.lanip"); %> / <% tran("share.subnet"); %></dt>
				<% tran("hidx.page15"); %>
				<dt><% tran("idx.dhcp_srv"); %></dt>
				<% tran("hidx.page16"); %>
				<dt><% tran("idx.dhcp_start"); %></dt>
				<% tran("hidx.page17"); %>
				<dt><% tran("idx.dhcp_maxusers"); %></dt>
				<% tran("hidx.page18"); %>
				<dt><% tran("idx.dhcp_lease"); %></dt>
				<% tran("hidx.page19"); %>
				<dt><% tran("idx_static.dns"); %> 1-3</dt>
				<% tran("hidx.page20"); %>
				<dt>WINS</dt>
				<% tran("hidx.page21"); %>
				<dt><% tran("idx.dns_dnsmasq"); %></dt>
				<% tran("hidx.dns_dnsmasq"); %>
				<dt><% tran("idx.auth_dnsmasq"); %></dt>
				<% tran("hidx.auth_dnsmasq"); %>
				<dt><% tran("idx.force_dnsmasq"); %></dt>
				<% tran("hidx.force_dnsmasq"); %>
				<dt><% tran("idx.legend3"); %></dt>
				<% tran("hidx.page22"); %>
			</dl>
		</div>
		<div class="also">
			<h4><% tran("share.seealso"); %></h4>
			<ul>
				<li><a href="HWanMAC.asp"><% tran("bmenu.setupmacclone"); %></a></li>
				<li><a href="HManagement.asp"><% tran("bmenu.adminManagement"); %></a></li>
				<li><a href="HStatus.asp"><% tran("bmenu.statuRouter"); %></a></li>
			</ul>
		</div>
	</body>
</html>
