/* --------------------------------------------------------------------
               App State — Device Mode & Network Status
-------------------------------------------------------------------- */

const MODES = [
  { name: 'Thread OTBR',            icon: 'icon-thread', group: 'thread',  isCoordinator: false },
  { name: 'Zigbee Coordinator USB',  icon: 'icon-zigbee', group: 'zigbee',  isCoordinator: true  },
  { name: 'Zigbee Coordinator Net',  icon: 'icon-zigbee', group: 'zigbee',  isCoordinator: true  },
  { name: 'Zigbee Router',           icon: 'icon-zigbee', group: 'zigbee',  isCoordinator: false },
];

const NET_STATE = {
  0: {label: 'Initializing', icon: 'icon-softap',   isAccessPoint: false},
  1: {label: 'Ethernet',     icon: 'icon-ethernet', isAccessPoint: false},
  2: {label: 'WiFi',         icon: 'icon-wifi',     isAccessPoint: false},
  3: {label: 'SoftAP',       icon: 'icon-softap',   isAccessPoint: true},
  4: {label: 'ETH Retry',    icon: 'icon-ethernet', isAccessPoint: false},
  5: {label: 'WiFi Retry',   icon: 'icon-wifi',     isAccessPoint: false}
};

$(document).ready(function() {
  initFrontend();
  initEventSource();
});

/**
 * @brief Gets current ESP Device Mode
 * 
 *        Displays relevant Sections and hides the rest
 *        Builds Sidebar Navigation Links
 */
function initFrontend()
{
  $.ajax({
    url: '/device/mode',
    type: 'GET',
    dataType: 'json',
    success:
      function(data)
      {
        applyDeviceMode(data.mode);
        networkSetupCheck();

        if (!data.device_setup)
          document.getElementById('mode-selection-modal').style.display = 'flex';
      },
    error:
      function()
      {
        applyDeviceMode(0);
      }
  });
}

/* Always fetches network status once to populate the header badge / overview.
  Live updates afterwards come from the 'network_state_change' SSE event. */
function networkSetupCheck()
{
  $.ajax({
    url: '/network/status',
    type: 'GET',
    dataType: 'json',
    success: function(status)
    {
      setHeaderNetworkBadge(status);
      setOverviewNetwork(status);
    },
    error: function()
    {
      if (firstBoot)
        setTimeout(function() { networkSetupCheck(true); }, 3000);
    }
  });
}

function applyDeviceMode(mode)
{
  let currentMode = MODES[mode];

  setOverviewMode(mode);
  setHeaderModeBadge(mode);

  if (currentMode.isCoordinator)
  {
    setProxyMode(mode);
  }

  applySectionVisibility(currentMode);
  buildSidebar(currentMode);
}

function setOverviewMode(mode)
{
  let ovMode = document.getElementById('ov-mode');
  if (ovMode) 
    ovMode.innerText = MODES[mode].name || '—';
}

function setHeaderModeBadge(mode)
{
  let badge = document.getElementById('hdr-mode');
  if (badge) 
  {
    badge.querySelector('.badge-icon use').setAttribute('href', '#' + (MODES[mode].icon || 'icon-thread'));
    badge.querySelector('.badge-label').innerText = MODES[mode].name || '—';
  }
}

function setProxyMode(mode)
{
  let usbBtn = document.getElementById('zb-btn-usb');
    let netBtn = document.getElementById('zb-btn-net');
    if (usbBtn) 
      usbBtn.classList.toggle('active', mode === 1);
    if (netBtn) 
      netBtn.classList.toggle('active', mode === 2);
}

function isSectionVisible(section, currentMode)
{
  let sectionModes = (section.dataset.modes || 'all').split(' ');
  let sectionUsedInMode = sectionModes.some(function(mode)
    {
      return (mode === 'all') || (mode === currentMode.group);
    }
  );
  return sectionUsedInMode;
}

/**
 * @brief Loads Section visibility Information.
 *        Checks if Section should be displayed and visualizes accordingly.
 *
*/
function applySectionVisibility(currentMode) {
  document.querySelectorAll('section[data-modes]').forEach
  (
    function(section)
    {
      section.classList.toggle('hidden', !isSectionVisible(section, currentMode));
    }
  );
}

function buildSidebar(currentMode) {
  let navbar = document.getElementById('sidebar-nav');
  if (!navbar)
    return;

  let navbarHtml = '';
  document.querySelectorAll('section[data-nav-label]').forEach(function(section)
  {
    if (!isSectionVisible(section, currentMode))
      return;
    let label = section.dataset.navLabel;
    let icon  = section.dataset.navIcon || '';
    let href  = section.dataset.navHref || ('#' + section.id);
    navbarHtml += '<li><a href="' + href + '">'
          + (icon ? '<svg class="icon-stroke"><use href="#' + icon + '"/></svg> ' : '')
          + label
          + '</a></li>';
  });
  navbar.innerHTML = navbarHtml;

  // closes sidebar after click on mobile
  navbar.querySelectorAll('a').forEach(function(link) 
  {
    link.addEventListener('click', function() {
      document.querySelector('.app-body').classList.remove('sidebar-open');
    });
  });
}

function buildNetworkStatusString(network_status) 
{
  let overViewText = "Unknown";
  if (network_status.ip && !NET_STATE[network_status.mode].isAccessPoint)
  {
    networkState = NET_STATE[network_status.mode];
    overViewText = networkState.label + ' · ' +  network_status.ip;
  }
  return overViewText; 
}

function setOverviewNetwork(network_status)
{
  if (!network_status)
    return;

  let ovNet = document.getElementById('ov-net');
  if (ovNet)
    ovNet.innerText = buildNetworkStatusString(network_status);
}

function setHeaderNetworkBadge(network_status) 
{
  if (!network_status)
    return;

  let badge = document.getElementById('hdr-net');
  if (badge) 
  {
    badge.querySelector('.badge-icon use').setAttribute('href', '#' + NET_STATE[network_status.mode].icon);
    badge.querySelector('.badge-label').innerText = buildNetworkStatusString(network_status);
  }
}

/* --------------------------------------------------------------------
                        Network Config
-------------------------------------------------------------------- */

/* Load both configs when Network section first becomes visible */
$(document).ready(function() 
{
  let networkSection = document.getElementById('Network');
  if (!networkSection) 
    return;
  let loaded = false;
  let observer = new IntersectionObserver(function(entries) 
  {
    if (entries[0].isIntersecting && !loaded) {
      loaded = true;
      loadNetworkConfig('ethernet');
      loadNetworkConfig('wifi');
    }
  });
  observer.observe(networkSection);
});

function loadNetworkConfig(type) 
{
  let url    = type === 'wifi' ? '/network/wifi' : '/network/ethernet';
  let prefix = type === 'wifi' ? 'wifi' : 'eth';
  $.ajax({
    url: url, 
    type: 'GET', 
    dataType: 'json',
    success: function(cfg) 
    {
      let form = document.getElementById(prefix + '-config-form');
      if (type === 'wifi') {
        form.querySelector('[name=ssid]').value     = cfg.ssid || '';
        form.querySelector('[name=password]').value = '';
      }
      form.querySelector('[name=static_ip]').value      = cfg.static_ip     || '';
      form.querySelector('[name=gateway]').value        = cfg.gateway       || '';
      form.querySelector('[name=dns_primary]').value    = cfg.dns_primary   || '';
      form.querySelector('[name=dns_secondary]').value  = cfg.dns_secondary || '';
      document.getElementById(prefix + '-dhcp').checked = cfg.dhcp !== false;
      toggleStaticIpFields(prefix);
    },
    error: function() 
    { 
      console.log('Failed to load ' + type + ' config'); 
    }
  });
}

/* Toggle Input fields of Networkconfig depending on the dhcp checkbox */
function toggleStaticIpFields(prefix) 
{
  let dhcp = document.getElementById(prefix + '-dhcp').checked;
  document.getElementById(prefix + '-static-fields').querySelectorAll('input').forEach(function(inp) {
    inp.disabled = dhcp;
  });
}

function saveNetworkConfig(type) 
{
  let prefix   = type === 'wifi' ? 'wifi' : 'eth';
  let url      = type === 'wifi' ? '/network/wifi' : '/network/ethernet';
  let form     = document.getElementById(prefix + '-config-form');
  let statusEl = document.getElementById(prefix + '-save-status');

  let payload = 
  {
    dhcp:          document.getElementById(prefix + '-dhcp').checked,
    static_ip:     form.querySelector('[name=static_ip]').value,
    gateway:       form.querySelector('[name=gateway]').value,
    dns_primary:   form.querySelector('[name=dns_primary]').value,
    dns_secondary: form.querySelector('[name=dns_secondary]').value
  };
  if (type === 'wifi') 
  {
    payload.ssid     = form.querySelector('[name=ssid]').value;
    payload.password = form.querySelector('[name=password]').value;
  }

  statusEl.style.display = 'inline';
  statusEl.style.color   = 'gray';
  statusEl.innerText     = 'Saving…';

  $.ajax({
    url: url, 
    type: 'POST',
    contentType: 'application/json',
    data: JSON.stringify(payload),
    complete: function(jqXHR) {
      /* status 0 = connection dropped (expected when AP shuts down to reconnect) */
      let ok = jqXHR.status === 200 || jqXHR.status === 0;
      if (!ok) 
      {
        statusEl.style.color = 'red';
        statusEl.innerText   = 'Error saving config (HTTP ' + jqXHR.status + ').';
        return;
      }

      if (type === 'wifi') 
      {
        statusEl.style.color = 'darkorange';
        statusEl.innerText   = 'Connecting…';
        pollWifiConnection(statusEl);
      } 
      else 
      {
        statusEl.style.color = 'green';
        statusEl.innerText   = 'Saved.';
        setTimeout
        (function() { statusEl.style.display = 'none'; }, 4000);
      }
    }
  });
}

/* Polls ESP API and checks if Wifi got a valid IP address. 
Redirects to new IP or prints error message */
function pollWifiConnection(statusEl, attempts) 
{
  attempts = attempts || 0;
  if (attempts >= 20) 
  {
    statusEl.style.color = 'red';
    statusEl.innerText   = 'Timeout — check WiFi credentials.';
    return;
  }
  
  setTimeout(function() {
    $.ajax({
      url: '/network/status', type: 'GET', dataType: 'json',
      success: function(status) 
      {
        if (!NET_STATE[status.mode].isAccessPoint && status.ip)
        {
          statusEl.style.color = 'green';
          statusEl.innerText   = 'Connected! Redirecting to ' + status.ip + '…';
          setTimeout(function() { window.location.href = 'http://' + status.ip + '/'; }, 1500);
        } 
        else 
        {
          statusEl.innerText = 'Connecting… (' + (attempts + 1) + ')';
          pollWifiConnection(statusEl, attempts + 1);
        }
      },
      error: function() 
      {
        statusEl.innerText = 'Waiting for device… (' + (attempts + 1) + ')';
        pollWifiConnection(statusEl, attempts + 1);
      }
    });
  }, 2000);
}

/* --------------------------------------------------------------------
                   First Boot — Mode Selection
-------------------------------------------------------------------- */

const numberOfModeBoxes = 4;

function selectMode(mode) 
{
  for (let iii = 0; iii < numberOfModeBoxes; iii++) 
  {
    let card = document.getElementById('mode-card-' + iii);
    if (card) 
      card.classList.remove('selection-card--active');
  }

  document.getElementById('mode-card-' + mode).classList.add('selection-card--active');
  document.getElementById('mode-confirm-name').innerText = MODES[mode].name;
  document.getElementById('mode-confirm-bar').style.display = 'block';
}

function cancelModeSelection() 
{
  for (let iii = 0; iii < numberOfModeBoxes; iii++) 
  {
    let card = document.getElementById('mode-card-' + iii);
    if (card) 
      card.classList.remove('selection-card--active');
  }
  document.getElementById('mode-confirm-bar').style.display = 'none';
}

function getSelectedMode()
{
  for (let iii = 0; iii < numberOfModeBoxes; iii++)
  {
    let card = document.getElementById('mode-card-' + iii);
    if (card)
    {
      if (card.classList.contains('selection-card--active'))
        return iii;
    }
  }
  return -1;
}

function confirmModeSelection() {
  let selectedMode = getSelectedMode();
  if (selectedMode < 0) 
    return;

  let btn = document.getElementById('mode-confirm-btn');
  btn.disabled = true;
  btn.innerText = 'Flashing RCP & rebooting...';

  fetchNewestRcpRelease(selectedMode).then(function(rcpUrl) 
  {
    console.log(selectedMode);
    console.log(rcpUrl);

    $.ajax({
      url: '/flash/rcp',
      async: true,
      type: 'POST',
      contentType: 'application/json',
      dataType: 'json',
      data: JSON.stringify({
        url: rcpUrl,
        type: selectedMode
      }),
      complete: function() {
        // Connection drop expected on reboot — always treat as success
        document.getElementById('mode-select-view').style.display = 'none';
        document.getElementById('mode-flash-view').style.display = 'block';
        setTimeout(function() { location.reload(); }, 20000);
      }
    });
  });
}

/* --------------------------------------------------------------------
               Server-Sent Events
-------------------------------------------------------------------- */

function initEventSource()
{
  const es = new EventSource('/events');

  es.addEventListener('device_state', function(e) {
    const data = JSON.parse(e.data);
    if (data.mode === 'flashing') {
      showFlashModal(data.phase);
    } else {
      hideFlashModal();
    }
  });

  es.addEventListener('flash_progress', function(e) {
    const data = JSON.parse(e.data);
    updateFlashProgress(data.target, data.phase, data.percent);
  });

  es.addEventListener('flash_complete', function(e) {
    const data = JSON.parse(e.data);
    onFlashComplete(data.target, data.success, data.error);
  });

  // network
  es.addEventListener('network_state_change', function(e) {
    const status = JSON.parse(e.data);

    setHeaderNetworkBadge(status);
    setOverviewNetwork(status);
  });
}

/* --------------------------------------------------------------------
                     Flash Status Modal
-------------------------------------------------------------------- */

function _isFlashStatusActive() {
  return !!document.querySelector('#flash_status .flash-status-view');
}

function showFlashModal(phase) {
  if (!_isFlashStatusActive()) {
    const statusEl = document.getElementById('flash_status');
    const tpl = document.getElementById('flash-status-tpl').content.cloneNode(true);
    statusEl.replaceChildren(tpl);
    document.getElementById('flash_firmware_list').style.display = 'none';
    document.getElementById('flash_rcp_tabs').style.display = 'none';
    document.getElementById('flash_window_close').style.display = 'none';
    document.getElementById('flash_window_title').innerText = 'Flashing…';
  }
  document.getElementById('flash_status').querySelector('.flash-status-phase').textContent =
    _flashPhaseLabel(phase);
  document.getElementById('flash_window').style.display = 'flex';
}

function hideFlashModal() {
  document.getElementById('flash_window').style.display = 'none';
  _resetFlashStatus();
}

function updateFlashProgress(target, phase, percent) {
  if (!_isFlashStatusActive()) showFlashModal(phase);
  const statusEl = document.getElementById('flash_status');
  statusEl.querySelector('.flash-status-phase').textContent = _flashPhaseLabel(phase);
  statusEl.querySelector('.flash-progress-bar-fill').style.width = percent.toFixed(1) + '%';
  statusEl.querySelector('.flash-progress-label').textContent = percent.toFixed(1) + '%';
}

function onFlashComplete(target, success, error) {
  if (_isFlashStatusActive()) {
    const statusEl = document.getElementById('flash_status');
    statusEl.querySelector('.flash-progress-bar-fill').style.width = '100%';
    statusEl.querySelector('.flash-progress-label').textContent = '100%';
    statusEl.querySelector('.flash-status-phase').textContent =
      success ? 'Flash complete!' : 'Flash failed: ' + (error || 'Unknown error');
  }
  if (success) setTimeout(hideFlashModal, 2000);
}

function _flashPhaseLabel(phase) {
  switch (phase) {
    case 'downloading': return 'Downloading firmware…';
    case 'writing':     return 'Writing to device…';
    case 'verifying':   return 'Verifying…';
    case 'rebooting':   return 'Rebooting…';
    default:            return 'Flashing…';
  }
}

function _resetFlashStatus() {
  document.getElementById('flash_status').replaceChildren();
  document.getElementById('flash_firmware_list').style.display = '';
  document.getElementById('flash_window_close').style.display = '';
  document.getElementById('flash_window_title').innerText = 'Select Firmware';
}

/* --------------------------------------------------------------------
                            Flash
-------------------------------------------------------------------- */
// var ESP_RELEASES_URL = 'https://docs.codm.de/tools/releases.php';

let RCP_RELEASES_URL = 'https://api.github.com/repos/codm/czc-ot-rcp-fw/releases';
let ESP_RELEASES_URL = 'https://api.github.com/repos/codm/czc-ot-fw/releases';
let ZB_MANIFEST_URL  = 'https://raw.githubusercontent.com/codm/CZC/refs/heads/zb_fws/ti/manifest.json';

// Returns a Promise that resolves to the download URL of the newest firmware for the given mode
function fetchNewestRcpRelease(mode)
{
  switch (mode) {
    case 0: // Thread — GitHub RCP releases, newest first
      return $.getJSON(RCP_RELEASES_URL).then(newest_github_bin_url);

    case 1: // Coordinator USB
    case 2: // Coordinator Network
      return $.getJSON(ZB_MANIFEST_URL).then(function(data) { return newest_manifest_url(data.coordinator); });

    case 3: // Router
      return $.getJSON(ZB_MANIFEST_URL).then(function(data) { return newest_manifest_url(data.router); });

    default:
      return $.Deferred().reject('Invalid mode').promise();
  }
}

// Returns browser_download_url of the first .bin from the newest non-draft GitHub release
function newest_github_bin_url(releases) {
  for (var i = 0; i < releases.length; i++) {
    if (releases[i].draft) continue;
    for (var j = 0; j < releases[i].assets.length; j++) {
      if (releases[i].assets[j].name.endsWith('.bin'))
        return releases[i].assets[j].browser_download_url;
    }
  }
  return null;
}

// Returns the link of the entry with the highest version string in a manifest category.
// Strips short variant prefixes (e.g. "x4_") before comparing — uses the part after the first "_".
function newest_manifest_url(category) {
  var bestUrl = null, bestVer = null;
  Object.keys(category || {}).forEach(function(device) {
    Object.keys(category[device] || {}).forEach(function(filename) {
      var entry = category[device][filename];
      var ver = entry.ver || '';
      var u = ver.indexOf('_');
      var comparableVer = (u > 0 && u <= 3) ? ver.slice(u + 1) : ver;
      if (!bestVer || comparableVer > bestVer) {
        bestVer = comparableVer;
        bestUrl = entry.link || null;
      }
    });
  });
  return bestUrl;
}

function frontend_flash_esp_button() {
  // build dialog window
  document.getElementById('flash_window_title').innerText = 'Select ESP Firmware';
  document.getElementById('flash_firmware_list').innerHTML = '<p>Loading...</p>';
  document.getElementById('flash_status').innerText = '';
  document.getElementById('flash_rcp_tabs').style.display = 'none';
  document.getElementById('flash_window').style.display = 'flex';

  fetch_github_firmwares(ESP_RELEASES_URL)
    .done(function(list) { render_firmware_list(list, "esp"); })
    .fail(flash_list_error);
}

function frontend_flash_rcp_button() {
  document.getElementById('flash_window_title').innerText = 'Select RCP Firmware';
  document.getElementById('flash_firmware_list').innerHTML = '<p>Loading...</p>';
  document.getElementById('flash_status').innerText = '';
  document.getElementById('flash_rcp_tabs').style.display = 'block';
  document.getElementById('flash_window').style.display = 'flex';

  buildThreadFirmwareList();
}

function setRcpTabActive(btn) {
  document.querySelectorAll('#flash_rcp_tabs .tab-btn').forEach(function(b) {
    b.classList.remove('active');
  });
  btn.classList.add('active');
}

function buildThreadFirmwareList(btn)
{
  if (btn) setRcpTabActive(btn);
  document.getElementById('flash_firmware_list').innerHTML = '<p>Loading...</p>';
  fetch_github_firmwares(RCP_RELEASES_URL)
    .done(function(list) { render_firmware_list(list, "rcp", 0); })
    .fail(flash_list_error);
}

function buildCoordinatorFirmwareList(btn)
{
  if (btn) setRcpTabActive(btn);
  document.getElementById('flash_firmware_list').innerHTML = '<p>Loading...</p>';
  fetch_manifest_firmwares(ZB_MANIFEST_URL, "coordinator")
    .done(function(list) { render_firmware_list(list, "rcp", 1); })
    .fail(flash_list_error);
}

function buildRouterFirmwareList(btn)
{
  if (btn) setRcpTabActive(btn);
  document.getElementById('flash_firmware_list').innerHTML = '<p>Loading...</p>';
  fetch_manifest_firmwares(ZB_MANIFEST_URL, "router")
    .done(function(list) { render_firmware_list(list, "rcp", 3); })
    .fail(flash_list_error);
}

function flash_list_error() {
  document.getElementById('flash_firmware_list').innerHTML =
      '<p style="color:red">Failed to load firmware list.</p>';
}

/* ── Firmware info fetchers ────────────────────────────────────────────────
 *
 * Both functions return a jQuery Promise that resolves to a list of objects:
 *
 *   {
 *     version    {string}  Release tag or firmware version string
 *     link       {string}  Direct download URL of the .bin binary
 *     notes_link {string}  URL of the release page / changelog
 *   }
 *
 * Usage:
 *   fetch_github_firmwares(url).done(function(list) { ... }).fail(onError);
 *   fetch_manifest_firmwares(url, 'coordinator').done(function(list) { ... });
 */

/**
 * Fetches firmware entries from a GitHub Releases API endpoint.
 * Each .bin asset in a non-draft release produces one entry.
 *
 * @param  {string}  url  GitHub Releases API URL
 * @return {$.Deferred}   Resolves to FirmwareEntry[]
 */
function fetch_github_firmwares(url) {
  return $.getJSON(url).then(function(releases) {
    var result = [];
    releases.forEach(function(release) {
      if (release.draft) return;
      release.assets.forEach(function(asset) {
        if (!asset.name.endsWith('.bin')) return;
        result.push({
          version:    release.tag_name,
          link:       asset.browser_download_url,
          notes_link: release.html_url
        });
      });
    });
    return result;
  });
}

/**
 * Fetches firmware entries from a manifest JSON for a specific mode.
 * Manifest structure: data[mode][device][filename] = {ver, link, notes, baud}
 *
 * @param  {string}                url   Manifest JSON URL
 * @param  {'router'|'coordinator'} mode  Firmware category to extract
 * @return {$.Deferred}                  Resolves to FirmwareEntry[]
 */
function fetch_manifest_firmwares(url, mode) {
  return $.getJSON(url).then(function(data) {
    var category = data[mode] || {};
    var result = [];
    Object.keys(category).forEach(function(device) {
      var entries = category[device];
      Object.keys(entries).forEach(function(filename) {
        var entry = entries[filename];
        result.push({
          version:    entry.ver  || '—',
          link:       entry.link || '',
          notes_link: manifest_extract_url(entry.notes || '')
        });
      });
    });
    return result;
  });
}

/* Extracts the first URL from a markdown link "[text](url)" or a bare https:// string */
function manifest_extract_url(str) {
  var match = str.match(/\]\(([^)]+)\)/);
  if (match) return match[1];
  return /^https?:\/\//.test(str) ? str : '';
}

/**
 * @brief Renders firmware list from FirmwareEntry[] List
 * 
 * @param[in] `firmwares` FirmwareEntry[] List
 * @param[in] `device` ESP or RCP
 * @param[in] `mode` only needed when device is RCP - RCP mode  
*/ 
function render_firmware_list(firmwares, device, mode) {
  var container = document.getElementById('flash_firmware_list');
  var template  = document.getElementById('fw-row-tpl');

  container.innerHTML = '';

  if (!firmwares.length) {
    var msg = document.createElement('p');
    msg.style.color = 'orange';
    msg.textContent = 'No firmware versions found.';
    container.appendChild(msg);
    return;
  }

  var table = document.createElement('table');
  table.className = 'pure-table pure-table-horizontal';
  table.style.width = '100%';

  var thead     = table.createTHead();
  var headerRow = thead.insertRow();
  ['Version', 'Info', ''].forEach(function(label) {
    var th = document.createElement('th');
    th.textContent = label;
    headerRow.appendChild(th);
  });

  var tbody = table.createTBody();
  firmwares.forEach(function(fw) {
    var row = template.content.cloneNode(true).querySelector('tr');

    row.querySelector('.fw-version').textContent = fw.version || '—';

    if (fw.notes_link) {
      var a = document.createElement('a');
      a.href        = fw.notes_link;
      a.textContent = 'Firmware Info';
      a.target      = '_blank';
      a.rel         = 'noopener';
      row.querySelector('.fw-notes').appendChild(a);
    }

    if (device === 'esp') 
    {
      row.querySelector('button').addEventListener('click', function() {
        do_esp_flash_with_url(fw.link);
      });
    }
    else 
    {
      row.querySelector('button').addEventListener('click', function() {
        do_rcp_flash_with_url(fw.link, mode);
      });
    }

    tbody.appendChild(row);
  });

  container.appendChild(table);
}

function do_esp_flash_with_url(url) 
{
  document.getElementById('flash_window').style.display = 'none';
  let log = {error: 0, content: ''};
  let title = "Flash ESP";

  $.ajax({
    url: '/flash/esp',
    async: true,
    contentType: 'application/json',
    type: 'POST',
    dataType: 'json',
    data: JSON.stringify({
      url
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
      log.error = 1;
      log.content = 'Unknown error';
      console.log(arg);
      frontend_log_show(title, log);
    }
  });
}

function pollUntilOnline(onReady) {
  fetch(window.location.href, { cache: 'no-store', signal: AbortSignal.timeout(2000) })
    .then(function(r) {
      if (r.ok) onReady();
      else setTimeout(function() { pollUntilOnline(onReady); }, 2000);
    })
    .catch(function() {
      setTimeout(function() { pollUntilOnline(onReady); }, 2000);
    });
}

function do_rcp_flash_with_url(url, mode) {
  document.getElementById('flash_window').style.display = 'none';

  let log = {error: 0, content: ''};
  let title = "Flash RCP";
  document.getElementById('flash_status').innerText = 'Flashing...';
  $.ajax({
    url: '/flash/rcp',
    async: true,
    contentType: 'application/json',
    type: 'POST',
    dataType: 'json',
    data: JSON.stringify({
      url,
      type: mode
    }),

    success: function(arg) {
      if (arg.reboot) {
        log.error = 0;
        log.content = 'Flash scheduled. Device is rebooting...';
        frontend_log_show(title, log);
        pollUntilOnline(function() { window.location.reload(); });
      } else if (arg.status === 'flashing' || arg.status === 'started') {
        log.error = 0;
        log.content = arg.message || 'Flashing firmware...';
        frontend_log_show(title, log);
      } else {
        console_show_response_result(arg);
        log.error = arg.error;
        log.content = arg.message || 'Unknown response';
        frontend_log_show(title, log);
      }
    },
    error: function() {
      // Connection drop is expected: ESP reboots immediately after scheduling the flash
      log.error = 0;
      log.content = 'Flash scheduled. Device is rebooting...';
      frontend_log_show(title, log);
      pollUntilOnline(function() { window.location.reload(); });
    }
  });
}

function frontend_cancel_flash() {
  document.getElementById('flash_window').style.display = 'none';
}

/* --------------------------------------------------------------------
                     Zigbee Transport Mode
-------------------------------------------------------------------- */

function setZigbeeTransport(mode) {
  document.getElementById('zb-btn-usb').classList.toggle('active', mode === 1);
  document.getElementById('zb-btn-net').classList.toggle('active', mode === 2);
  document.getElementById('zb-transport-status').innerText = 'Switching — this may take a few seconds ...';
  $.ajax({
    url: '/device/mode', type: 'POST',
    contentType: 'application/json',
    data: JSON.stringify({mode: mode}),
    complete: function() {
      document.getElementById('zb-transport-status').innerText = ' page reloads in 10 seconds.';
      setTimeout(function() { location.reload(); }, 10000);
    }
  });
}

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
        "<button class=\"btn-sm\" onclick=\"frontend_show_join_network_window(this)\">Join<\/button>"
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
