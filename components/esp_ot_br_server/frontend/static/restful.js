var OT_SERVER_PACKAGE_VERSION = "v1.0.0";

/* --------------------------------------------------------------------
               App State — Device Mode & Network Status
-------------------------------------------------------------------- */

var MODE_NAMES  = ['Thread OTBR', 'Zigbee Coordinator USB', 'Zigbee Coordinator Net', 'Zigbee Router'];
var MODE_ICONS  = ['icon-thread',  'icon-zigbee',            'icon-zigbee',             'icon-zigbee'];
var MODE_GROUPS = ['thread',       'zigbee',                  'zigbee',                  'zigbee'];

var NET_STATE = {
  0: {label: 'Initializing', icon: 'icon-softap'},
  1: {label: 'Ethernet',     icon: 'icon-ethernet'},
  2: {label: 'WiFi',         icon: 'icon-wifi'},
  3: {label: 'SoftAP',       icon: 'icon-softap'},
  4: {label: 'ETH Retry',    icon: 'icon-ethernet'},
  5: {label: 'WiFi Retry',   icon: 'icon-wifi'}
};

function initAppState() {
  $.ajax({
    url: '/device/mode', type: 'GET', dataType: 'json',
    success: function(data) { applyDeviceMode(data.mode); },
    error:   function()     { document.body.classList.add('mode-thread'); }
  });
  pollNetworkStatus();
}

function applyDeviceMode(mode) {
  var group = MODE_GROUPS[mode] || 'thread';
  document.body.classList.remove('mode-thread', 'mode-zigbee', 'mode-coordinator');
  document.body.classList.add('mode-' + group);
  if (mode === 1 || mode === 2) {
    document.body.classList.add('mode-coordinator');
    var usbBtn = document.getElementById('zb-btn-usb');
    var netBtn = document.getElementById('zb-btn-net');
    if (usbBtn) usbBtn.classList.toggle('active', mode === 1);
    if (netBtn) netBtn.classList.toggle('active', mode === 2);
  }

  var ovMode = document.getElementById('ov-mode');
  if (ovMode) ovMode.innerText = MODE_NAMES[mode] || '—';

  var badge = document.getElementById('hdr-mode');
  if (badge) {
    badge.querySelector('.hdr-badge-icon use').setAttribute('href', '#' + (MODE_ICONS[mode] || 'icon-thread'));
    badge.querySelector('.hdr-badge-label').innerText = MODE_NAMES[mode] || '—';
  }
}

function pollNetworkStatus() {
  $.ajax({
    url: '/network/status', type: 'GET', dataType: 'json',
    success: function(status) {
      var info  = NET_STATE[status.mode] || {label: 'Unknown', icon: 'icon-softap'};
      var label = (status.connected && status.ip) ? info.label + ' · ' + status.ip : info.label;

      var badge = document.getElementById('hdr-net');
      if (badge) {
        badge.querySelector('.hdr-badge-icon use').setAttribute('href', '#' + info.icon);
        badge.querySelector('.hdr-badge-label').innerText = label;
      }

      var ovNet = document.getElementById('ov-net');
      if (ovNet) ovNet.innerText = label;
    },
    error: function() {}
  });
  setTimeout(pollNetworkStatus, 10000);
}

/* --------------------------------------------------------------------
                        Network Config
-------------------------------------------------------------------- */

function toggleStaticIpFields(prefix) {
  var dhcp = document.getElementById(prefix + '-dhcp').checked;
  document.getElementById(prefix + '-static-fields').querySelectorAll('input').forEach(function(inp) {
    inp.disabled = dhcp;
  });
}

function loadNetworkConfig(type) {
  var url    = type === 'wifi' ? '/network/wifi' : '/network/ethernet';
  var prefix = type === 'wifi' ? 'wifi' : 'eth';
  $.ajax({
    url: url, type: 'GET', dataType: 'json',
    success: function(cfg) {
      var form = document.getElementById(prefix + '-config-form');
      if (type === 'wifi') {
        form.querySelector('[name=ssid]').value     = cfg.ssid || '';
        form.querySelector('[name=password]').value = '';
      }
      form.querySelector('[name=static_ip]').value     = cfg.static_ip     || '';
      form.querySelector('[name=gateway]').value        = cfg.gateway       || '';
      form.querySelector('[name=dns_primary]').value    = cfg.dns_primary   || '';
      form.querySelector('[name=dns_secondary]').value  = cfg.dns_secondary || '';
      document.getElementById(prefix + '-dhcp').checked = cfg.dhcp !== false;
      toggleStaticIpFields(prefix);
    },
    error: function() { console.log('Failed to load ' + type + ' config'); }
  });
}

function saveNetworkConfig(type) {
  var prefix   = type === 'wifi' ? 'wifi' : 'eth';
  var url      = type === 'wifi' ? '/network/wifi' : '/network/ethernet';
  var form     = document.getElementById(prefix + '-config-form');
  var statusEl = document.getElementById(prefix + '-save-status');

  var payload = {
    dhcp:          document.getElementById(prefix + '-dhcp').checked,
    static_ip:     form.querySelector('[name=static_ip]').value,
    gateway:       form.querySelector('[name=gateway]').value,
    dns_primary:   form.querySelector('[name=dns_primary]').value,
    dns_secondary: form.querySelector('[name=dns_secondary]').value
  };
  if (type === 'wifi') {
    payload.ssid     = form.querySelector('[name=ssid]').value;
    payload.password = form.querySelector('[name=password]').value;
  }

  statusEl.style.display = 'inline';
  statusEl.style.color   = 'gray';
  statusEl.innerText     = 'Saving…';

  $.ajax({
    url: url, type: 'POST',
    contentType: 'application/json',
    data: JSON.stringify(payload),
    complete: function(jqXHR) {
      /* status 0 = connection dropped (expected when AP shuts down to reconnect) */
      var ok = jqXHR.status === 200 || jqXHR.status === 0;
      if (!ok) {
        statusEl.style.color = 'red';
        statusEl.innerText   = 'Error saving config (HTTP ' + jqXHR.status + ').';
        return;
      }
      if (type === 'wifi') {
        statusEl.style.color = 'darkorange';
        statusEl.innerText   = 'Connecting…';
        pollForNewIp(statusEl);
      } else {
        statusEl.style.color = 'green';
        statusEl.innerText   = 'Saved.';
        setTimeout(function() { statusEl.style.display = 'none'; }, 4000);
      }
    }
  });
}

function pollForNewIp(statusEl, attempts) {
  attempts = attempts || 0;
  if (attempts >= 20) {
    statusEl.style.color = 'red';
    statusEl.innerText   = 'Timeout — check WiFi credentials.';
    return;
  }
  setTimeout(function() {
    $.ajax({
      url: '/network/status', type: 'GET', dataType: 'json',
      success: function(status) {
        if (status.connected && status.ip) {
          statusEl.style.color = 'green';
          statusEl.innerText   = 'Connected! Redirecting to ' + status.ip + '…';
          setTimeout(function() { window.location.href = 'http://' + status.ip + '/'; }, 1500);
        } else {
          statusEl.innerText = 'Connecting… (' + (attempts + 1) + ')';
          pollForNewIp(statusEl, attempts + 1);
        }
      },
      error: function() {
        statusEl.innerText = 'Waiting for device… (' + (attempts + 1) + ')';
        pollForNewIp(statusEl, attempts + 1);
      }
    });
  }, 2000);
}

/* Load both configs when Network section first becomes visible */
$(document).ready(function() {
  var networkSection = document.getElementById('Network');
  if (!networkSection) return;
  var loaded = false;
  var observer = new IntersectionObserver(function(entries) {
    if (entries[0].isIntersecting && !loaded) {
      loaded = true;
      loadNetworkConfig('ethernet');
      loadNetworkConfig('wifi');
    }
  });
  observer.observe(networkSection);
});

/* --------------------------------------------------------------------
                   First Boot — Mode Selection
-------------------------------------------------------------------- */
var g_selected_mode = -1;
var g_mode_names    = ['Thread OTBR', 'Zigbee Coordinator USB', 'Zigbee Coordinator Network', 'Zigbee Router'];

function initFirstBootCheck() {
  $.ajax({
    url: '/device/mode', type: 'GET', dataType: 'json',
    success: function(data) {
      if (!data.device_setup) {
        document.getElementById('internet-waiting-overlay').style.display = 'flex';
        pollInternetForModePopup();
      }
    },
    error: function() { /* device not yet reachable — ignore */ }
  });
}

function pollInternetForModePopup() {
  $.ajax({
    url: '/network/status', type: 'GET', dataType: 'json',
    success: function(status) {
      if (status.connected) {
        document.getElementById('internet-waiting-overlay').style.display = 'none';
        document.getElementById('mode-selection-modal').style.display     = 'flex';
      } else {
        document.getElementById('waiting-status-text').innerText = 'Waiting for Ethernet or WiFi connection...';
        setTimeout(pollInternetForModePopup, 2000);
      }
    },
    error: function() {
      document.getElementById('waiting-status-text').innerText = 'Waiting for device...';
      setTimeout(pollInternetForModePopup, 3000);
    }
  });
}

function selectMode(mode) {
  g_selected_mode = mode;
  for (var i = 0; i < 4; i++) {
    var card = document.getElementById('mode-card-' + i);
    if (card) card.classList.remove('mode-card-selected');
  }
  document.getElementById('mode-card-' + mode).classList.add('mode-card-selected');
  document.getElementById('mode-confirm-name').innerText          = g_mode_names[mode];
  document.getElementById('mode-confirm-bar').style.display       = 'block';
}

function cancelModeSelection() {
  g_selected_mode = -1;
  for (var i = 0; i < 4; i++) {
    var card = document.getElementById('mode-card-' + i);
    if (card) card.classList.remove('mode-card-selected');
  }
  document.getElementById('mode-confirm-bar').style.display = 'none';
}

function confirmModeSelection() {
  if (g_selected_mode < 0) return;
  var btn = document.getElementById('mode-confirm-btn');
  btn.disabled    = true;
  btn.innerText   = 'Flashing RCP & rebooting...';

  $.ajax({
    url: '/device/mode', type: 'POST',
    contentType: 'application/json',
    data: JSON.stringify({mode: g_selected_mode}),
    complete: function() {
      // Connection drop expected on reboot — always treat as success
      document.getElementById('mode-selection-modal').innerHTML =
        '<div class="dialog-content" style="text-align:center; padding:40px;">' +
        '<h2 style="margin-bottom:16px;">Setup complete</h2>' +
        '<p>The RCP is being flashed. The device will reboot automatically.</p>' +
        '<p style="color:gray; font-size:13px; margin-top:12px;">Page reloads in 20 seconds...</p>' +
        '</div>';
      setTimeout(function() { location.reload(); }, 20000);
    }
  });
}

$(document).ready(function() {
  initFirstBootCheck();
  initAppState();
});
/* --------------------------------------------------------------------
                            action
-------------------------------------------------------------------- */
function frontend_click_for_more_form_param() {
  elem = document.getElementById("form-more-param");
  if (elem.style.display == 'block') {
    elem.style.display = 'none';
    document.getElementById('form-more-tip').innerHTML = "for more &#x21B5;";
  } else {
    elem.style.display = 'block';
    document.getElementById('form-more-tip').innerHTML = "for less &#x21B5;";
  }
}

function frontend_click_copy_network_info_to_form(arg) {
  var row = $(arg).parent().parent().find("td");
  if (row.eq(0) == "")
    return;
  var data = {
    id : row.eq(0).text(),
    network_name : row.eq(1).text(),
    extended_panid : row.eq(2).text(),
    panid : row.eq(3).text(),
    mac_address : row.eq(4).text(),
    channel : row.eq(5).text(),
    dBm : row.eq(6).text(),
    LQI : row.eq(7).text(),
  };

  document.getElementsByName("networkName")[0].value = data.network_name;
  document.getElementsByName("extPanId")[0].value = data.extended_panid;
  document.getElementsByName("panId")[0].value = data.panid;
  document.getElementsByName("channel")[0].value = data.channel;

  item = document.getElementById("form_tip");
  item.style.color = "blue";
  item.style.display = "block";
  item.innerHTML = "Form update."
}

function frontend_log_show(title, arg) {

  document.getElementById("log_window_title").innerText = title;
  document.getElementById("log_window_title").style.fontSize = "25px";

  if (!arg.hasOwnProperty("error") || !arg.hasOwnProperty("content")) {
    document.getElementById("log_window").style.display = "flex";
    document.getElementById("log_window_content").innerText = "Unknown: ";
    return;
  }
  if (arg.error == 0)
    document.getElementById("log_window_content").style.color = "green";
  else
    document.getElementById("log_window_content").style.color = "red";

  document.getElementById("log_window").style.display = "flex";
  document.getElementById("log_window_content").innerText = arg.content;
  return;
}

function frontend_log_close() {
  document.getElementById("log_window").style.display = "none";
}

function console_show_response_result(arg) {
  console.log("Error: ", arg.error);
  console.log("Result: ", arg.result);
  console.log("Message: ", arg.message);
}
/* --------------------------------------------------------------------
                            Discover
-------------------------------------------------------------------- */
$("document").ready(function() {
  $("div ul li a").click(function() {
    $("div ul li a").removeClass("active"); // firstly ,remove all actives
    $(this).addClass("active"); // choose 'li' to add option 'active'

    var tabx = $(this).attr('id');
    tabx = tabx.slice(5, 6);
    console.log(tabx);
    var panes = document.querySelectorAll(".tab-pane");
    for (i = 0; i < panes.length; i++) {
      panes[i].className = "tab-pane";
    }
    panes[tabx].className = "tab-pane active";
  });
});

function fill_thread_available_network_table(data) {
  document.getElementById("available_networks_body").innerHTML =
      "<tr><td></td><td></td><td></td><td></td><td></td><td></td><td></td><td></td></tr>"; // clear table
  var rows = '';
  var row_id = 1;
  if (data.error)
    return;
  data.result.forEach(function(keys) {
    rows += '<tr>'
    for (var k in keys) {
      rows += '<td>' + keys[k] + '</td>'
    }
    rows += '<td>'
    rows +=
        "<button class=\"btn-submit\" onclick=\"frontend_show_join_network_window(this)\">Join<\/button>"
    rows += '</td>'
    rows += '</tr>'
    row_id++;
  });

  document.getElementById("available_networks_table").caption.innerText =
      "Available Thread Networks: Scan Completed"
  document.getElementById("available_networks_body").innerHTML = rows;
}

function http_server_scan_thread_network() {
  var log = {error : 0, content : ""};
  var title = "Available Network";

  document.getElementById("available_networks_table").caption.innerText =
      "Available Thread Networks: Waiting ..."

  log.content = "Waiting...";
  frontend_log_show(title, log);

  $.ajax({
    url : '/available_network',
    async : true,
    contentType : 'application/json;charset=utf-8',
    type : 'GET',
    dataType : "json",
    data : "",
    success : function(arg) {
      console_show_response_result(arg);
      fill_thread_available_network_table(arg);
      log.error = arg.error;
      log.content = arg.message;
      frontend_log_show(title, log);
    },
    error : function(arg) {
      log.error = "Error: ";
      log.content = "Unknown: ";
      frontend_log_show(title, log);
      console.log(arg);
    }
  })
}

/* --------------------------------------------------------------------
                            Join
-------------------------------------------------------------------- */
var g_available_networks_row;
function http_server_join_thread_network(root) {
  var log = {error : 0, content : ""};
  var title = "Join"
  $.ajax({
    url : '/join_network',
    async : true,
    contentType : 'application/json;charset=utf-8',
    type : 'POST',
    dataType : "json",
    data : JSON.stringify(root),
    success : function(arg) {
      console_show_response_result(arg);
      log.error = arg.error;
      log.content = arg.message;
      frontend_log_show(title, log);
    },
    error : function(arg) {
      log.error = "Error";
      log.content = "Unknown";
      frontend_log_show(title, log);
      console.log(arg)
    }
  })
}

function frontend_show_join_network_window(arg) {
  g_available_networks_row = $(arg).parent().parent().find("td");
  document.getElementById('join_window').style.display = 'block';
}

function frontend_submit_join_network(arg) {
  if (g_available_networks_row == "" || g_available_networks_row.eq(0) == "") {
    console.log("Invalid Network!");
    return;
  }
  var root = $("#join_network_table").serializeJson();
  root.index = parseInt(g_available_networks_row.eq(0).text());
  if (root.hasOwnProperty("defaultRoute") && root.defaultRoute == "on")
    root.defaultRoute = 1;
  else
    root.defaultRoute = 0;

  http_server_join_thread_network(root);
  document.getElementById('join_window').style.display = "none"
}

function frontend_cancel_join_network(data) {
  var item = document.getElementById('join_window');
  item.style.display = "none"
  return false;
}

function frontend_join_type_select(data) {
  if (data.options[data.selectedIndex].value == "network_key_type") {
    document.getElementById('join_network_key').style.display = 'block'
    document.getElementById('join_thread_pskd').style.display = 'none'
  } else if (data.options[data.selectedIndex].value == "thread_pskd_type") {
    document.getElementById('join_network_key').style.display = 'none'
    document.getElementById('join_thread_pskd').style.display = 'block'
  }
}

/* --------------------------------------------------------------------
                            Form
-------------------------------------------------------------------- */
function handle_form_response_message(arg, form_id) {
  item = document.getElementById(form_id);
  if (arg.hasOwnProperty("error") && !arg.error) {
    if (arg.result == "successful") {
      item.style.color = "green";
      item.innerHTML = arg.message;
    } else {
      item.style.color = "red";
      item.innerHTML = arg.message;
    }
  } else {
    item.style.color = "red";
    item.innerHTML = "Try against.";
  }
}

/* convert form's input to json type */
$.fn.serializeJson =
    function() {
  var serializeObj = {};
  var array = this.serializeArray();
  var str = this.serialize();
  $(array).each(function() {
    if (serializeObj[this.name]) {
      if ($.isArray(serializeObj[this.name])) {
        serializeObj[this.name].push(this.value);
      } else {
        serializeObj[this.name] = [ serializeObj[this.name], this.value ];
      }
    } else {
      serializeObj[this.name] = this.value;
    }
  });
  return serializeObj;
}

function http_server_upload_form_network_table() {
  item = document.getElementById("form_tip");
  item.style.color = "green";
  item.style.display = 'block';

  var root = $("#network_form").serializeJson();
  var title = "Form";
  if (root.hasOwnProperty("defaultRoute") && root.defaultRoute == "on")
    root.defaultRoute = 1;
  else
    root.defaultRoute = 0;
  if (root.hasOwnProperty("defaultRoute") && root.defaultRoute != "")
    root.channel = parseInt(root.channel);

  var log = {error : 0, content : ""};

  $.ajax({
    url : '/form_network',
    async : true,
    contentType : 'application/json;charset=utf-8',
    type : 'POST',
    dataType : "json",
    data : JSON.stringify(root),
    success : function(arg) {
      console_show_response_result(arg);
      if (arg != {})
        handle_form_response_message(arg, "form_tip");
      log.error = arg.error;
      log.content = arg.message;
      frontend_log_show(title, log);
    },
    error : function(arg) {
      log.error = "Error: ";
      log.content = "Unknown: ";
      frontend_log_show(title, log);
      console.log(arg)
    }
  })
}

/* --------------------------------------------------------------------
                            Status
-------------------------------------------------------------------- */
function decode_thread_status_package(package) {
  if (package.error)
    return;

  document.getElementById("ipv6-link_local_address").innerHTML =
      package.result["IPv6:LinkLocalAddress"];
  document.getElementById("ipv6-routing_local_address").innerHTML =
      package.result["IPv6:RoutingLocalAddress"];
  document.getElementById("ipv6-mesh_local_address").innerHTML =
      package.result["IPv6:MeshLocalAddress"];
  document.getElementById("ipv6-mesh_local_prefix").innerHTML =
      package.result["IPv6:MeshLocalPrefix"];

  document.getElementById("network-name").innerHTML =
      package.result["Network:Name"];
  document.getElementById("network-panid").innerHTML =
      package.result["Network:PANID"];
  document.getElementById("network-partition_id").innerHTML =
      package.result["Network:PartitionID"];
  document.getElementById("network-xpanid").innerHTML =
      package.result["Network:XPANID"];
  document.getElementById("network-baid").innerHTML =
      package.result["Network:BorderAgentID"];

  document.getElementById("openthread-version").innerHTML =
      package.result["OpenThread:Version"];
  document.getElementById("openthread-version_api").innerHTML =
      package.result["OpenThread:Version API"];
  document.getElementById("openthread-role").innerHTML =
      package.result["RCP:State"];
  document.getElementById("openthread-PSKc").innerHTML =
      package.result["OpenThread:PSKc"];

  document.getElementById("rcp-channel").innerHTML =
      package.result["RCP:Channel"];
  document.getElementById("rcp-EUI64").innerHTML = package.result["RCP:EUI64"];
  document.getElementById("rcp-txpower").innerHTML =
      package.result["RCP:TxPower"];
  document.getElementById("rcp-version").innerHTML =
      package.result["RCP:Version"];

  document.getElementById("WPAN-service").innerHTML =
      package.result["WPAN service"];

  document.getElementById("t-ipv6-link_local_address").innerHTML =
      package.result["IPv6:LinkLocalAddress"];
  document.getElementById("t-ipv6-routing_local_address").innerHTML =
      package.result["IPv6:RoutingLocalAddress"];
  document.getElementById("t-ipv6-mesh_local_address").innerHTML =
      package.result["IPv6:MeshLocalAddress"];
  document.getElementById("t-ipv6-mesh_local_prefix").innerHTML =
      package.result["IPv6:MeshLocalPrefix"];

  document.getElementById("t-network-name").innerHTML =
      package.result["Network:Name"];
  document.getElementById("t-network-panid").innerHTML =
      package.result["Network:PANID"];
  document.getElementById("t-network-partition_id").innerHTML =
      package.result["Network:PartitionID"];
  document.getElementById("t-network-xpanid").innerHTML =
      package.result["Network:XPANID"];
  document.getElementById("t-network-baid").innerHTML =
      package.result["Network:BorderAgentID"];

  document.getElementById("t-openthread-version").innerHTML =
      package.result["OpenThread:Version"];
  document.getElementById("t-openthread-version_api").innerHTML =
      package.result["OpenThread:Version API"];
  document.getElementById("t-openthread-role").innerHTML =
      package.result["RCP:State"];
  document.getElementById("t-openthread-PSKc").innerHTML =
      package.result["OpenThread:PSKc"];

  document.getElementById("t-rcp-channel").innerHTML =
      package.result["RCP:Channel"];
  document.getElementById("t-rcp-EUI64").innerHTML = package.result["RCP:EUI64"]
  document.getElementById("t-rcp-txpower").innerHTML =
      package.result["RCP:TxPower"];
  document.getElementById("t-rcp-version").innerHTML =
      package.result["RCP:Version"];

  document.getElementById("t-WPAN-service").innerHTML =
      package.result["WPAN service"];
}

function http_server_get_thread_network_properties() {
  var log = {error : 0, content : ""};
  var title = "Properties";
  $.ajax({
    url : '/get_properties',
    async : true,
    contentType : 'application/json;charset=utf-8',
    type : 'GET',
    dataType : "json",
    data : "",
    success : function(arg) {
      console_show_response_result(arg);
      decode_thread_status_package(arg);
      log.error = arg.error;
      log.content = arg.message;
      frontend_log_show(title, log);
    },
    error : function(arg) {
      log.error = "Error: ";
      log.content = "Unknown: ";
      frontend_log_show(title, log);
      console.log(arg)
    }
  })
}

/* --------------------------------------------------------------------
                            Setting
-------------------------------------------------------------------- */
function http_server_add_prefix_to_thread_network() {
  var root = $("#network_setting").serializeJson();
  var log = {error : 0, content : ""};
  var title = "Add Prefix";
  if (root.hasOwnProperty("defaultRoute") && root.defaultRoute == "on")
    root.defaultRoute = 1;
  else
    root.defaultRoute = 0;

  $.ajax({
    url : '/add_prefix',
    async : true,
    contentType : 'application/json;charset=utf-8',
    type : 'POST',
    dataType : "json",
    data : JSON.stringify(root),
    success : function(arg) {
      console_show_response_result(arg);
      log.error = arg.error;
      log.content = arg.message;
      frontend_log_show(title, log);
    },
    error : function(arg) {
      log.error = "Error: ";
      log.content = "Unknown: ";
      frontend_log_show(title, log);
      console.log(arg)
    }
  })
}

function http_server_delete_prefix_from_thread_network() {
  var root = $("#network_setting").serializeJson();
  var log = {error : 0, content : ""};
  var title = "Delete Prefix";
  $.ajax({
    url : '/delete_prefix',
    async : true,
    contentType : 'application/json;charset=utf-8',
    type : 'POST',
    dataType : "json",
    data : JSON.stringify(root),
    success : function(arg) {
      console_show_response_result(arg);
      log.error = arg.error;
      log.content = arg.message;
      frontend_log_show(title, log);
    },
    error : function(arg) {
      log.error = "Error: ";
      log.content = "Unknown: ";
      frontend_log_show(title, log);
      console.log(arg)
    }
  })
}

/* --------------------------------------------------------------------
                            Flash
-------------------------------------------------------------------- */
// var RCP_MANIFEST_URL = 'https://raw.githubusercontent.com/codm/XZG/zb_fws/ti/manifest.json';
// var ESP_RELEASES_URL = 'https://docs.codm.de/tools/releases.php';

var RCP_RELEASES_URL = 'https://api.github.com/repos/codm/czc-ot-rcp-fw/releases';
var ESP_RELEASES_URL = 'https://api.github.com/repos/codm/czc-ot-fw/releases';
var ZB_MANIFEST_URL  = 'https://raw.githubusercontent.com/codm/CZC/refs/heads/zb_fws/ti/manifest.json';

var g_flash_type        = '';
var g_rcp_tab_type      = 'thread';
var g_coordinator_mode  = 1;  // 1 = USB/UART, 2 = Network/TCP

var TAB_MODE = {thread: 0, coordinator: null, router: 3};

function frontend_flash_esp_button() {
  g_flash_type = 'esp';
  document.getElementById('flash_window_title').innerText = 'Select ESP Firmware';
  document.getElementById('flash_firmware_list').innerHTML = '<p>Loading...</p>';
  document.getElementById('flash_status').innerText = '';
  document.getElementById('flash_rcp_tabs').style.display = 'none';
  document.getElementById('flash_window').style.display = 'flex';

  fetch_firmware_list_from_url(ESP_RELEASES_URL, parse_github_releases, render_firmware_list, flash_list_error);
}

function frontend_flash_rcp_button() {
  g_flash_type = 'rcp';
  document.getElementById('flash_window_title').innerText = 'Select RCP Firmware';
  document.getElementById('flash_firmware_list').innerHTML = '<p>Loading...</p>';
  document.getElementById('flash_status').innerText = '';
  document.getElementById('flash_rcp_tabs').style.display = 'block';
  document.getElementById('flash_window').style.display = 'flex';

  flash_rcp_tab('thread');
}

function flash_rcp_tab(type) {
  g_rcp_tab_type = type;
  document.querySelectorAll('#flash_rcp_tabs .rcp-tab-btn').forEach(function(btn) {
    btn.classList.toggle('active', btn.getAttribute('onclick') === "flash_rcp_tab('" + type + "')");
  });
  document.getElementById('flash_coordinator_transport').style.display = (type === 'coordinator') ? 'block' : 'none';
  document.getElementById('flash_firmware_list').innerHTML = '<p>Loading...</p>';

  if (type === 'thread') {
    fetch_firmware_list_from_url(RCP_RELEASES_URL, parse_github_releases, render_firmware_list, flash_list_error);
  } else {
    fetch_firmware_list_from_url(ZB_MANIFEST_URL, function(data) {
      return parse_zb_manifest(data, type);
    }, render_firmware_list, flash_list_error);
  }
}

function setCoordTransport(mode) {
  g_coordinator_mode = mode;
  document.getElementById('coord-btn-usb').classList.toggle('active', mode === 1);
  document.getElementById('coord-btn-net').classList.toggle('active', mode === 2);
}

function setZigbeeTransport(mode) {
  document.getElementById('zb-btn-usb').classList.toggle('active', mode === 1);
  document.getElementById('zb-btn-net').classList.toggle('active', mode === 2);
  document.getElementById('zb-transport-status').innerText = 'Switching — device will reboot...';
  $.ajax({
    url: '/device/mode', type: 'POST',
    contentType: 'application/json',
    data: JSON.stringify({mode: mode}),
    complete: function() {
      document.getElementById('zb-transport-status').innerText = 'Rebooting — page reloads in 15 seconds.';
      setTimeout(function() { location.reload(); }, 15000);
    }
  });
}

function parse_zb_manifest(data, type) {
  var category = data[type] || {};
  var result = [];
  Object.keys(category).forEach(function(device) {
    var entries = category[device];
    Object.keys(entries).forEach(function(filename) {
      var entry = entries[filename];
      result.push({
        name: filename,
        version: entry.ver || '—',
        url: entry.link || ''
      });
    });
  });
  return result;
}

/* Generic fetcher — swap parser to support different release endpoints later */
function fetch_firmware_list_from_url(url, parser, onSuccess, onError) {
  $.ajax({
    url: url,
    async: true,
    type: 'GET',
    dataType: 'json',
    headers: {'Accept': 'application/vnd.github.v3+json'},
    success: function(data) { onSuccess(parser(data)); },
    error: onError
  });
}

function flash_list_error() {
  document.getElementById('flash_firmware_list').innerHTML =
      '<p style="color:red">Failed to load firmware list.</p>';
}

/* GitHub Releases API parser */
function parse_github_releases(releases) {
  var result = [];
  releases.forEach(function(release) {
    // if (release.draft || release.prerelease) return;
    if (release.draft) return;
    release.assets.forEach(function(asset) {
      if (!asset.name.endsWith('.bin')) return;
      result.push({name: asset.name, version: release.tag_name, url: asset.browser_download_url});
    });
  });
  return result;
}

/* Legacy parsers — kept for future use after serving architecture change */
function parse_releases_php(data) {
  var entries = Array.isArray(data) ? data : (data.releases || data.items || []);
  var result = [];
  entries.forEach(function(entry) {
    var url = entry.url || entry.download_url || entry.firmware_url || '';
    var version = entry.version || entry.tag || entry.tag_name || '';
    var name = entry.name || version || url.split('/').pop();
    if (url) result.push({name: name, version: version, url: url});
  });
  return result;
}

function parse_manifest_json(data) {
  var entries = data.files || data.firmware || (Array.isArray(data) ? data : []);
  var result = [];
  entries.forEach(function(entry) {
    var url = entry.url || entry.path || entry.download_url || '';
    var version = entry.ver || entry.version || entry.fw || '';
    var name = entry.name || entry.fw || version || url.split('/').pop();
    var isRelevant = /ot|rcp|openthread|thread/i.test(name + version + url);
    if (url && isRelevant) result.push({name: name, version: version, url: url});
  });
  if (!result.length) {
    entries.forEach(function(entry) {
      var url = entry.url || entry.path || entry.download_url || '';
      var version = entry.ver || entry.version || entry.fw || '';
      var name = entry.name || entry.fw || version || url.split('/').pop();
      if (url) result.push({name: name, version: version, url: url});
    });
  }
  return result;
}

function render_firmware_list(firmwares) {
  var container = document.getElementById('flash_firmware_list');
  if (!firmwares.length) {
    container.innerHTML = '<p style="color:orange">No firmware versions found.</p>';
    return;
  }
  var html = '<table class="pure-table pure-table-horizontal" style="width:100%">'
      + '<thead><tr><th>Name</th><th>Version</th><th></th></tr></thead><tbody>';
  firmwares.forEach(function(fw, idx) {
    html += '<tr><td>' + fw.name + '</td><td>' + (fw.version || '—') + '</td>'
        + '<td><button class="btn-submit" data-fw-idx="' + idx + '">Flash</button></td></tr>';
  });
  html += '</tbody></table>';
  container.innerHTML = html;

  container.querySelectorAll('button[data-fw-idx]').forEach(function(btn) {
    var idx = parseInt(btn.getAttribute('data-fw-idx'));
    btn.addEventListener('click', function() {
      do_flash_with_url(firmwares[idx].url);
    });
  });
}

function do_flash_with_url(url) {
  document.getElementById('flash_window').style.display = 'none';

  var endpoint = g_flash_type === 'esp' ? '/flash/esp' : '/flash/rcp';
  var log = {error: 0, content: ''};
  var title = g_flash_type === 'esp' ? 'Flash ESP' : 'Flash RCP';
  document.getElementById('flash_status').innerText = 'Flashing...';
  $.ajax({
    url: endpoint,
    async: true,
    contentType: 'application/json',
    type: 'POST',
    dataType: 'json',
    data: JSON.stringify({
      url,
      type: g_rcp_tab_type
    }),

    success: function(arg) {
      if (arg.reboot) {
        log.error = 0;
        log.content = 'Flash scheduled. Device is rebooting...';
      } else if (arg.status === 'flashing' || arg.status === 'started') {
        log.error = 0;
        log.content = arg.message || 'Flashing firmware...';
      } else {
        console_show_response_result(arg);
        log.error = arg.error;
        log.content = arg.message || 'Unknown response';
      }
      frontend_log_show(title, log);
    },
    error: function(arg) {
      if (g_flash_type === 'rcp') {
        // Connection drop is expected: ESP reboots immediately after scheduling the flash
        log.error = 0;
        log.content = 'Flash scheduled. Device is rebooting...';
      } else {
        log.error = 1;
        log.content = 'Unknown error';
        console.log(arg);
      }
      frontend_log_show(title, log);
    }
  });
}

function frontend_cancel_flash() {
  document.getElementById('flash_window').style.display = 'none';
}

/* --------------------------------------------------------------------
                            commission
-------------------------------------------------------------------- */
function http_server_thread_network_commissioner() {
  var root = {
    pskd : "1234567890",
  };

  $.ajax({
    url : '/commission',
    async : true,
    contentType : 'application/json;charset=utf-8',
    type : 'POST',
    dataType : "json",
    data : JSON.stringify(root),
    success : function(arg) { console_show_response_result(arg); },
    error : function(arg) { console.log(arg) }
  })
}

/* --------------------------------------------------------------------
                            Topology
-------------------------------------------------------------------- */
function ctrl_thread_network_topology(arg) {
  var node_info = undefined;
  var topology_info = undefined;
  if (arg == "Running" || arg == "Suspend") {

    $.ajax({
      url : '/node_information',
      async : true,
      contentType : 'application/json;charset=utf-8',
      type : 'GET',
      dataType : "json",
      data : "",
      success : function(msg) {
        console_show_response_result(msg);
        node_info = msg;
        if (node_info != undefined && topology_info != undefined) {
          handle_thread_networks_topology_package(node_info, topology_info);
        }
      },
      error : function(msg) { console.log(msg) }
    })
    $.ajax({
      url : '/topology',
      async : true,
      contentType : 'application/json;charset=utf-8',
      type : 'GET',
      dataType : "json",
      data : "",
      success : function(msg) {
        console_show_response_result(msg);
        topology_info = msg;
        if (node_info != undefined && topology_info != undefined) {
          handle_thread_networks_topology_package(node_info, topology_info);
        }
      },
      error : function(msg) { console.log(msg) }
    })
  }
}

function http_server_build_thread_network_topology(arg) {
  ctrl_thread_network_topology("Running");
  document.getElementById("btn_topology").innerHTML = "Reload Topology";
}

function intToHexString(num, len) {
  var value;
  value = num.toString(16);

  while (value.length < len) {
    value = '0' + value;
  }
  return value;
} class Topology_Graph {
  constructor() {
    this.graph_isReady = false;
    this.graph_info = {'nodes' : [], 'links' : []};
    this.node_detialInfo = 'Unknown';
    this.router_number = 0;
    this.detailList = {
      'ExtAddress' : {'title' : false, 'content' : true},
      'Rloc16' : {'title' : false, 'content' : true},
      'Mode' : {'title' : false, 'content' : false},
      'Connectivity' : {'title' : false, 'content' : false},
      'Route' : {'title' : false, 'content' : false},
      'LeaderData' : {'title' : false, 'content' : false},
      'NetworkData' : {'title' : false, 'content' : true},
      'IP6Address List' : {'title' : false, 'content' : true},
      'MACCounters' : {'title' : false, 'content' : false},
      'ChildTable' : {'title' : false, 'content' : false},
      'ChannelPages' : {'title' : false, 'content' : false}
    };
  }
  update_detail_list() {
    for (var detailInfoKey in this.detailList) {
      this.detailList[detailInfoKey]['title'] = false;
    }
    for (var diagInfoKey in this.nodeDetailInfo) {
      if (diagInfoKey in this.detailList) {
        this.detailList[diagInfoKey]['title'] = true;
      }
    }
  }
}

var topology_update = new Topology_Graph();
function handle_thread_networks_topology_package(node, diag) {
  var nodeMap = {};
  var count, src, dist, rloc, child, rlocOfParent, rlocOfChild, diagOfNode,
      linkNode, childInfo;
  let topology = new Topology_Graph();

  var diag_package = diag["result"];
  for (diagOfNode of diag_package) {

    diagOfNode['RouteId'] =
        '0x' + intToHexString(diagOfNode['Rloc16'] >> 10, 2);
    diagOfNode['Rloc16'] = '0x' + intToHexString(diagOfNode['Rloc16'], 4);

    diagOfNode['LeaderData']['LeaderRouterId'] =
        '0x' + intToHexString(diagOfNode['LeaderData']['LeaderRouterId'], 2);
    for (linkNode of diagOfNode['Route']['RouteData']) {
      linkNode['RouteId'] = '0x' + intToHexString(linkNode['RouteId'], 2);
    }
  }

  count = 0;
  var node_info = node["result"];
  for (diagOfNode of diag_package) {
    if ('ChildTable' in diagOfNode) {

      rloc = parseInt(diagOfNode['Rloc16'], 16).toString(16);
      nodeMap[rloc] = count; // give id to every node

      if (diagOfNode['RouteId'] == diagOfNode['LeaderData']['LeaderRouterId']) {
        diagOfNode['Role'] = 'Leader';
      } else {
        diagOfNode['Role'] = 'Router';
      }

      topology.graph_info.nodes.push(diagOfNode);

      if (diagOfNode['Rloc16'] === node_info['Rloc16']) {
        topology.node_detialInfo = diagOfNode
      }
      count = count + 1;
    }
  }
  topology.router_number = count;
  document.getElementById("topology_netwotkname").innerHTML =
      node_info["NetworkName"];
  document.getElementById("topology_leader").innerHTML =
      "0x" + node_info["LeaderData"]["LeaderRouterId"].toString(16);
  document.getElementById("topology_router_number").innerHTML =
      count.toString();

  src = 0; // respent current router id, in order.
  for (diagOfNode of diag_package) {
    if ('ChildTable' in diagOfNode) {
      // Link bewtwen routers
      for (linkNode of diagOfNode['Route']['RouteData']) {
        rloc = (parseInt(linkNode['RouteId'], 16) << 10)
                   .toString(16); // if diagOfNode has 'ChildTable' member,
                                  // diagOfNode is router
        if (rloc in nodeMap) {
          dist = nodeMap[rloc];
          if (src < dist) {
            topology.graph_info.links.push({
              'source' : src,
              'target' : dist,
              'weight' : 1,
              'type' : 0,
              'linkInfo' : {
                'inQuality' : linkNode['LinkQualityIn'],
                'outQuality' : linkNode['LinkQualityOut']
              }
            });
          }
        }
      }

      // Link between router and child
      for (childInfo of diagOfNode['ChildTable']) {
        child = {};
        rlocOfParent = parseInt(diagOfNode['Rloc16'], 16).toString(16);
        rlocOfChild =
            (parseInt(diagOfNode['Rloc16'], 16) + childInfo['ChildId'])
                .toString(16);

        src = nodeMap[rlocOfParent];

        child['Rloc16'] = '0x' + rlocOfChild;
        child['RouteId'] = diagOfNode['RouteId'];
        nodeMap[rlocOfChild] = count;
        child['Role'] = 'Child';
        topology.graph_info.nodes.push(child);
        topology.graph_info.links.push({
          'source' : src,
          'target' : count,
          'weight' : 1,
          'type' : 1,
          'linkInfo' :
              {'Timeout' : childInfo['Timeout'], 'Mode' : childInfo['Mode']}

        });
        count = count + 1;
      }
    }
    src = src + 1;
  }

  draw_thread_topology_graph(topology);
}

var svg = d3.select('.d3graph')
              .append("svg")
              .attr('preserveAspectRatio', 'xMidYMid meet');

var force = d3.layout.force();

var link;
var node;

var trigger_flag = true;
function draw_thread_topology_graph(arg) {
  var json, tooltip;
  var scale, len;
  var topology = new Topology_Graph();
  topology = arg;
  d3.selectAll("svg > *").remove();
  scale = topology.graph_info.nodes.length;
  if (scale > 8) {
    scale = 8;
  }
  len = 150 * Math.sqrt(scale);

  // Topology graph
  svg.attr('viewBox',
           '0, 0, ' + len.toString(10) + ', ' + (len / (3 / 2)).toString(10));

  // Legend
  svg.append('circle')
      .attr('cx', len - 20)
      .attr('cy', 10)
      .attr('r', 3)
      .style('fill', "#7e77f8")
      .style('stroke', '#484e46')
      .style('stroke-width', '0.4px');

  svg.append('circle')
      .attr("cx", len - 20)
      .attr('cy', 20)
      .attr('r', 3)
      .style('fill', '#03e2dd')
      .style('stroke', '#484e46')
      .style('stroke-width', '0.4px');

  svg.append('circle')
      .attr('cx', len - 20)
      .attr('cy', 30)
      .attr('r', 3)
      .style('fill', '#aad4b0')
      .style('stroke', '#484e46')
      .style('stroke-width', '0.4px')
      .style('stroke-dasharray', '2 1');

  svg.append('circle')
      .attr('cx', len - 50)
      .attr('cy', 10)
      .attr('r', 3)
      .style('fill', '#ffffff')
      .style('stroke', '#f39191')
      .style('stroke-width', '0.4px');

  svg.append('text')
      .attr('x', len - 15)
      .attr('y', 10)
      .text('Leader')
      .style('font-size', '4px')
      .attr('alignment-baseline', 'middle');

  svg.append('text')
      .attr('x', len - 15)
      .attr('y', 20)
      .text('Router')
      .style('font-size', '4px')
      .attr('alignment-baseline', 'middle');

  svg.append('text')
      .attr('x', len - 15)
      .attr('y', 30)
      .text('Child')
      .style('font-size', '4px')
      .attr('alignment-baseline', 'middle');

  svg.append('text')
      .attr('x', len - 45)
      .attr('y', 10)
      .text('Selected')
      .style('font-size', '4px')
      .attr('alignment-baseline', 'middle');

  // Tooltip style  for each node
  tooltip = d3.select('body')
                .append('div')
                .attr('data-toggle', 'tooltip')
                .style('position', 'absolute')
                .style('z-index', '10')
                .style('font-size', '17px')
                .style('color', '#000000')
                .style('display', 'block')
                .text('a simple tooltip');

  json = topology.graph_info;

  force.distance(40)
      .size([ len, len / (3 / 2) ])
      .nodes(json.nodes)
      .links(json.links)
      .start();

  link = svg.selectAll('.link')
             .data(json.links)
             .enter()
             .append('line')
             .attr('class', 'link')
             .style('stroke', '#908484')
             // Dash line for link between child and parent
             .style('stroke-dasharray',
                    function(item) {
                      if ('Timeout' in item.linkInfo)
                        return '4 4';
                      else
                        return '0 0'
                    })
             // Line width representing link quality
             .style('stroke-width',
                    function(item) {
                      if ('inQuality' in item.linkInfo)
                        return Math.sqrt(item.linkInfo.inQuality / 2);
                      else
                        return Math.sqrt(0.5)
                    })
             // Effect of mouseover on a line
             .on('mouseover',
                 function(item) {
                   return tooltip.style('visibility', 'visible')
                       .text(item.linkInfo);
                 })
             .on('mousemove',
                 function() {
                   return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                       .style('left', (d3.event.pageX + 10) + 'px');
                 })
             .on('mouseout',
                 function() { return tooltip.style('display', 'none'); });

  node = svg.selectAll('.node')
             .data(json.nodes)
             .enter()
             .append('g')
             .attr('class', function(item) { return item.Role; })
             .call(force.drag)
             // Tooltip effect of mouseover on a node
             .on('mouseover',
                 function(item) {
                   return tooltip.style('display', 'block').text(item.Rloc16);
                 })
             .on('mousemove',
                 function() {
                   return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                       .style('left', (d3.event.pageX + 10) + 'px');
                 })
             .on('mouseout',
                 function() { return tooltip.style('display', 'none'); });

  d3.selectAll('.Child')
      .append('circle')
      .attr('r', '6')
      .attr('fill', '#aad4b0')
      .style('stroke', '#484e46')
      .style('stroke-dasharray', '2 1')
      .style('stroke-width', '0.5px')
      .attr('class', function(item) { return item.Rloc16; })
      .on('mouseover',
          function(item) {
            return tooltip.style('display', 'block').text(item.Rloc16);
          })
      .on('mousemove',
          function() {
            return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                .style('left', (d3.event.pageX + 10) + 'px');
          })
      .on('mouseout', function() { return tooltip.style('display', 'none'); });

  d3.selectAll('.Leader')
      .append('circle')
      .attr('r', '8')
      .attr('fill', '#7e77f8')
      .style('stroke', '#484e46')
      .style('stroke-width', '1px')
      .attr('class', function(item) { return 'Stroke'; })
      // Effect that node will become bigger when mouseover
      .on('mouseover',
          function(item) {
            d3.select(this).transition().attr('r', '9');
            return tooltip.style('display', 'block').text(item.Rloc16);
          })
      .on('mousemove',
          function() {
            return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                .style('left', (d3.event.pageX + 10) + 'px');
          })
      .on('mouseout',
          function() {
            d3.select(this).transition().attr('r', '8');
            return tooltip.style('display', 'none');
          })
      // Effect that node will have a yellow edge when clicked
      .on('click', function(item) {
        d3.selectAll('.Stroke')
            .style('stroke', '#484e46')
            .style('stroke-width', '1px');
        d3.select(this).style('stroke', '#f39191').style('stroke-width', '1px');
        topology.nodeDetailInfo = item;
        topology.update_detail_list();
      });

  d3.selectAll('.Router')
      .append('circle')
      .attr('r', '8')
      .style('stroke', '#484e46')
      .style('stroke-width', '1px')
      .attr('fill', '#03e2dd')
      .attr('class', 'Stroke')
      .on('mouseover',
          function(item) {
            d3.select(this).transition().attr('r', '8');
            return tooltip.style('display', 'block').text(item.Rloc16);
          })
      .on('mousemove',
          function() {
            return tooltip.style('top', (d3.event.pageY - 10) + 'px')
                .style('left', (d3.event.pageX + 10) + 'px');
          })
      .on('mouseout',
          function() {
            d3.select(this).transition().attr('r', '7');
            return tooltip.style('display', 'none');
          })
      // The same effect as Leader
      .on('click', function(item) {
        d3.selectAll('.Stroke')
            .style('stroke', '#484e46')
            .style('stroke-width', '1px');
        d3.select(this).style('stroke', '#f39191').style('stroke-width', '1px');
        topology.nodeDetailInfo = item;
        topology.update_detail_list();
      });

  if (trigger_flag) {
    force.on('tick', function() {
      link.attr('x1', function(item) { return item.source.x; })
          .attr('y1', function(item) { return item.source.y; })
          .attr('x2', function(item) { return item.target.x; })
          .attr('y2', function(item) { return item.target.y; });
      node.attr(
          'transform',
          function(
              item) { return 'translate(' + item.x + ',' + item.y + ')'; });
    });
    trigger_flag = true;
  } else {
    force.on('end', function() {
      link.attr('x1', function(item) { return item.source.x; })
          .attr('y1', function(item) { return item.source.y; })
          .attr('x2', function(item) { return item.target.x; })
          .attr('y2', function(item) { return item.target.y; });
      node.attr(
          'transform',
          function(
              item) { return 'translate(' + item.x + ',' + item.y + ')'; });
    });
  }

  topology.update_detail_list();
  topology.graph_isReady = true;
}
