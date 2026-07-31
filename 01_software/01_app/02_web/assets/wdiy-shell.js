/**
 * Workshop DIY — Shared shell injector
 * ─────────────────────────────────────────────────────────────
 * Reads <body data-*> metadata, injects splash + sidebars + log
 * panel + toast + header + footer, mutates LANG for page title,
 * loads page-specific help, then calls init() from wdiy-script.js.
 *
 * Load order in each page:
 *   <script src="assets/wdiy-script.js"></script>   (defines LANG, init)
 *   <script src="assets/wdiy-shell.js"></script>    (this file — IIFE)
 *
 * Page declares itself via <body data-*>:
 *   data-app-title       — h1 + splash title    (default: "Workshop DIY")
 *   data-app-subtitle    — header subtitle      (default: explore/build/deploy)
 *   data-help-page       — help content key     (default: "default")
 *   data-status-text     — status pill label    (default: "Ready")
 *   data-status-online   — "true" → green dot   (default: false → red)
 *   data-default-theme   — fallback theme       (default: "robot")
 */
(function () {
  'use strict';

  /* ─────────────────────────────────────────────────────────
     STALE LOCALSTORAGE GUARD
     scrub theme names that no longer exist in the CSS so that
     setTheme() doesn't apply an undefined theme on reload.
     ───────────────────────────────────────────────────────── */
  const VALID_THEMES = [
    'lab-light', 'lab-dark', 'terminal', 'solarized',
    'robot', 'bot-pop', 'kapow', 'blastoff', 'dino',
  ];
  try {
    const saved = localStorage.getItem('wdiy-theme');
    if (saved && !VALID_THEMES.includes(saved)) {
      localStorage.removeItem('wdiy-theme');
    }
  } catch { /* private mode etc. */ }

  /* ─────────────────────────────────────────────────────────
     PAGE METADATA
     ───────────────────────────────────────────────────────── */
  const body = document.body;
  const ds = body.dataset;
  const PAGE = {
    title:    ds.appTitle    || 'Workshop DIY',
    subtitle: ds.appSubtitle || '🚀 explore · 🛠️ build · 🚀 deploy',
    help:     ds.helpPage    || 'default',
    statusTx: ds.statusText  || 'Ready',
    statusOn: ds.statusOnline === 'true',
    theme:    ds.defaultTheme || 'robot',
  };

  // Set default theme on <html> if no saved theme exists
  try {
    if (!localStorage.getItem('wdiy-theme')) {
      document.documentElement.dataset.theme = PAGE.theme;
    }
  } catch {
    document.documentElement.dataset.theme = PAGE.theme;
  }

  /* ─────────────────────────────────────────────────────────
     PER-PAGE HELP CONTENT
     ───────────────────────────────────────────────────────── */
  const HELP = {
    'default': {
      faq: [
        ['What is this app?',          'A Workshop-DIY educational web app. Explore, create, innovate.'],
        ['How do I change the theme?', 'Open Settings (⚙️) and pick from 9 themes in 4 mood groups.'],
        ['How do I change language?',  'Open Settings (⚙️) and pick EN / FR / AR. Arabic auto-enables RTL.'],
      ],
      howto: [
        'Use Settings (⚙️) to customize theme &amp; language.',
        'Watch the Activity Log (📜) for events.',
      ],
      wiki: [
        ['🎨 Themes',  '9 built-in themes in 4 groups: Science, Hacker, Pro, Kids action.'],
        ['🔒 Privacy', '100% local. No tracking, no analytics. Stays in your browser.'],
      ],
    },

    /* ── Index — landing page ── */
    'index': {
      faq: [
        ['What is this page?',     'The launcher — pick a tool below: start-here, flash, monitor, audit.'],
        ['Where do I begin?',      'Click <strong>Start Here</strong> if you\'re new — it walks through layout, build, flash.'],
        ['Browser compatibility?', 'Flasher and Monitor need <strong>Chrome / Edge</strong>. Other tools work everywhere.'],
        ['Offline?',               'Yes — every tool is local. The flasher + monitor talk directly to your USB board.'],
      ],
      howto: [
        '<strong>1.</strong> Click <strong>Start Here</strong> to read the project guide.',
        '<strong>2.</strong> Build the firmware: <code>./launch.sh package-web</code>.',
        '<strong>3.</strong> Click <strong>Flash firmware</strong> → connect board → install.',
        '<strong>4.</strong> Click <strong>Serial monitor</strong> to watch the robot boot &amp; log.',
      ],
      wiki: [
        ['🚀 ESP32-C3',     'RISC-V 32-bit MCU · 4 MB flash · 400 KB SRAM · WiFi + BLE.'],
        ['🛠️ PlatformIO',  'Build system — <code>./launch.sh</code> wraps it. See start-here.html.'],
        ['📋 Audit',         'Bug log + fix tracker. Click "Firmware Audit" or see <a href="audit.html">audit.html</a>.'],
      ],
    },

    /* ── Flash — Web Serial flasher ── */
    'flash': {
      faq: [
        ['Why doesn\'t the button work?',     'Web Serial only works in <strong>Chrome / Edge</strong>. Firefox + Safari don\'t support it.'],
        ['"Failed to connect"?',              'Hold <strong>BOOT</strong>, tap <strong>RESET</strong>, release <strong>BOOT</strong>, click again. See <a href="start-here.html#bootreset">Mission 09</a>.'],
        ['Board not in the picker?',          '<strong>Windows:</strong> install CP210x or CH340 driver. <strong>macOS:</strong> pick <code>/dev/cu.usbserial-*</code>. <strong>Linux:</strong> add yourself to <code>dialout</code>.'],
        ['Where does the firmware come from?', 'From <code>./launch.sh package-web</code> — it puts the bins next to <code>manifest.json</code>.'],
      ],
      howto: [
        '<strong>1.</strong> Plug the ESP32-C3 in via USB.',
        '<strong>2.</strong> Click <strong>Connect &amp; install</strong>.',
        '<strong>3.</strong> Pick the <em>USB JTAG/Serial</em> port.',
        '<strong>4.</strong> Wait for the install (~30 s).',
        '<strong>5.</strong> Open the <a href="monitor.html">serial monitor</a> to see the robot boot.',
      ],
      wiki: [
        ['🔌 Web Serial',   'Browser API for direct USB-serial access. Chrome &amp; Edge only.'],
        ['📦 manifest.json', 'Tells esp-web-tools which bins to flash + offsets. Generated by launch.sh.'],
        ['🔁 Boot trick',    'BOOT held + RESET tap forces the chip into ROM bootloader, accepts any flash.'],
      ],
    },

    /* ── Monitor — Web Serial console ── */
    'monitor': {
      faq: [
        ['Browser support?',             'Chrome / Edge only — same Web Serial API as the flasher.'],
        ['Port already open elsewhere?', 'Close PlatformIO/Arduino monitors first — only one app can hold the serial port.'],
        ['What baud rate?',              'Project default is <code>115200</code>. Match what <code>monitor_speed</code> says in <code>platformio.ini</code>.'],
        ['Can I send data to the robot?', 'Yes — type in the input below the log and press Enter or click Send.'],
      ],
      howto: [
        '<strong>1.</strong> Pick the baud rate (default 115200).',
        '<strong>2.</strong> Click <strong>Connect</strong> &amp; choose your port.',
        '<strong>3.</strong> Watch boot logs scroll into the activity panel.',
        '<strong>4.</strong> Type in the send box → Enter to send.',
        '<strong>5.</strong> Click <strong>Disconnect</strong> when done so the flasher can grab the port.',
      ],
      wiki: [
        ['📡 Web Serial',  'Same API the flasher uses. No drivers needed if Windows already sees the chip.'],
        ['📜 Activity log', 'Hit 📜 in the header — the serial output also flows there with timestamps.'],
        ['⏎ Line ending',  'Most embedded REPLs want CRLF. Adjust in the dropdown if your firmware expects LF or none.'],
      ],
    },

    /* ── Audit — bug tracker ── */
    'audit': {
      faq: [
        ['What is this?',          'Live bug log + fix tracker. Each item has severity, file, and a checkbox you can tick.'],
        ['Does it save my ticks?', 'Yes — checkbox state is stored in <code>localStorage</code>. Refresh-safe, browser-local.'],
        ['What\'s a "BUG-NNN"?',  'Detailed forensics for incidents that actually happened — symptom, root cause, fix.'],
        ['How do I add a bug?',    'Edit <code>audit.html</code> directly. Eventually this will be data-driven.'],
      ],
      howto: [
        '<strong>1.</strong> Skim the stat cards — Critical first.',
        '<strong>2.</strong> Open a phase to see items.',
        '<strong>3.</strong> Tick checkboxes as you fix things.',
        '<strong>4.</strong> Read the <em>BUG REPORTS</em> section for the war stories.',
      ],
      wiki: [
        ['🔴 Severity',  'Critical = crash/corruption. High = wrong behavior. Medium = waste/bug-prone. Low = polish.'],
        ['🐛 BUG-001',   '<a href="#bug-001">RMT ISR recursion</a> — the FastLED stack-overflow that triggered the platform pin.'],
        ['📋 Format',    'Bugs include symptom, dump, root-cause analysis, fix, and lessons learned.'],
      ],
    },

    /* ── Start-here — keeps the rich FAQ already in start-here.html ── */
    'start-here': {
      faq: [
        ['What is this robot?',         'ESP32-C3 RISC-V robot with servos, NeoPixels, OLED, ultrasonic sensor, BT control via RemoteXY.'],
        ['How do I flash firmware?',    'Run <code>./launch.sh</code> → option 10 (full flash).'],
        ['Upload fails — what now?',    'Hold BOOT, tap RESET, release BOOT, retry. See Mission 09.'],
        ['Where is the source code?',   'In <code>01_src/</code>. Entry: <code>01_src.ino</code>.'],
      ],
      howto: [
        '<strong>1.</strong> Run <code>./launch.sh</code> → option 0 to install all tools.',
        '<strong>2.</strong> Open each mission below to learn what each tool does.',
        '<strong>3.</strong> Watch the Activity Log (📜) for timestamped events.',
        '<strong>4.</strong> Use Settings (⚙️) to switch theme and language.',
      ],
      wiki: [
        ['🤖 ESP32-C3', 'RISC-V 32-bit MCU · 4 MB flash · 400 KB SRAM · WiFi + BLE.'],
        ['⚡ RMT driver', 'Platform pinned to <code>espressif32@6.7.0</code> (ESP-IDF 5.1) to fix the FastLED ISR crash.'],
        ['🔒 Privacy',   '100% local. Stays in your browser.'],
      ],
    },

    /* ── User Guide — kid / end-user driving the robot ── */
    'user-guide': {
      faq: [
        ['How do I turn it on?',     'Plug USB-C to charge, then flip the bottom switch. You\'ll hear the boot tune. See section 2.'],
        ['How do I connect my phone?', 'Install RemoteXY (free), open it, scan, tap the robot, pair. See section 3.'],
        ['What do the colors mean?', 'Lights = battery + status. See section 5 for the full color key.'],
        ['It stopped working!',      'Check battery LEDs, then phone Bluetooth, then power-cycle the robot. See section 8.'],
      ],
      howto: [
        '<strong>1.</strong> Power on with the bottom switch.',
        '<strong>2.</strong> Open RemoteXY app on your phone.',
        '<strong>3.</strong> Tap "Connect", pick the robot, pair.',
        '<strong>4.</strong> Drive with the joystick · tap action button for tricks.',
      ],
      wiki: [
        ['🤖 Robot-01',   'ESP32-C3 brain · 2 motors · 4 LEDs · OLED · ultrasonic sensor · buzzer.'],
        ['📱 RemoteXY',   'Free phone app for Bluetooth control. iOS + Android.'],
        ['🛡️ Safety',    'Don\'t drive off tables or stairs. Don\'t charge while wet.'],
      ],
    },

    /* ── Build Guide — maker assembling from parts ── */
    'build-guide': {
      faq: [
        ['Skill level needed?',    'Basic soldering. ~4 hours total bench time.'],
        ['Total cost?',            'USD 30–45 depending on supplier (AliExpress cheapest, ships slow).'],
        ['Can I substitute parts?', 'ESP32-C3 is fixed. Servos can be other 5 V continuous-rotation. NeoPixel count must match firmware config.'],
        ['Where do the STLs live?', '<code>03_3d/</code> in the project — chassis, cover, wheels, mounts.'],
      ],
      howto: [
        '<strong>1.</strong> Buy the BOM (section 1).',
        '<strong>2.</strong> Print the chassis from <code>03_3d/</code>.',
        '<strong>3.</strong> Solder following the wire diagram (section 3).',
        '<strong>4.</strong> Assemble (section 4).',
        '<strong>5.</strong> Flash firmware via <a href="flash.html">flash.html</a>.',
      ],
      wiki: [
        ['📋 BOM',       '12 parts · ESP32-C3 + 2 servos + 4 NeoPixels + ultrasonic + OLED + buzzer + battery + charger.'],
        ['🔌 Pin map',   'GPIOs documented in section 3. Note pin conflicts (audit C1, H1, H2).'],
        ['🖨️ 3D parts', '6 STL files · ~3 h total print at 0.2 mm / 20 % infill PLA.'],
      ],
    },

    /* ── Hardware — KiCad schematic + PCB browser ── */
    'hardware': {
      faq: [
        ['Where is the schematic source?',  '<code><a href="https://github.com/abourdim/esp32c3_super_mini_robot/tree/master/02_hardware/v3">02_hardware/v3/</a></code> — KiCad 7. The <code>.kicad_sch</code> is plain S-expression text.'],
        ['Can I open it without KiCad?',    'Yes — the live page renders all 5 PCB layers as toggleable SVGs. For the schematic itself, KiCad 7+ is needed.'],
        ['What chip drives the motors?',    '<strong>TB6612FNG</strong> dual H-bridge — handles 2 DC motors at up to 1.2 A each. PWMA + AIN1/AIN2 for motor A; PWMB + BIN1/BIN2 for motor B.'],
        ['Is this still a v3 board?',       'Yes — v3 is the current revision (June–July 2025). v1 and v2 are archived in their own folders.'],
        ['Can I order a board?',            'Send <code>02_hardware/v3/production/37_richa_light_move_v3.zip</code> (the gerbers) to JLCPCB / PCBWay. ~$5 for 5 boards.'],
      ],
      howto: [
        '<strong>1.</strong> Toggle layers on the PCB browser to see what\'s on each side.',
        '<strong>2.</strong> Open the interactive BOM (<code>hardware/v3/bom/ibom.html</code>) — hover a part to see it highlighted on the board.',
        '<strong>3.</strong> Read the GPIO pin map (§ 2) — single source of truth for firmware <code>00_config.h</code>.',
        '<strong>4.</strong> Read the connector reference (§ 3) for what plugs into J1–J11.',
      ],
      wiki: [
        ['🔌 ESP32-C3',  'MCU. Native USB-CDC. 13 GPIOs broken out.'],
        ['⚙️ TB6612FNG', 'Dual H-bridge motor driver. STBY pull-up enables it.'],
        ['🔋 Power',     'Battery → SK12D07VG4 switch → AMS1117-5.0 → 5 V rail. 3V3 from C3 onboard.'],
        ['🔧 Jumpers',   'JP1: bridge GPIO 0 ↔ GPIO 20. JP2: GPIO 3 = motor PWMA <em>or</em> external J3.'],
      ],
    },

    /* ── Learn — technical deep-dive reference ── */
    'learn': {
      faq: [
        ['How is this different from start-here?', 'Start-here is the narrative tour (14 missions). Learn is the reference manual — every option of launch.sh, every directive of platformio.ini, with failure modes and cross-links.'],
        ['Who is this for?',                       'Devs past start-here who want the <em>why</em> and <em>what\'s underneath</em>. Reference, not tutorial.'],
        ['Can I add a topic?',                     'Yes — edit <code>learn.html</code> directly, follow the existing collapsible / .opt pattern. Open a PR or issue.'],
        ['How do I jump to a section?',            'Each top-level section has an id (<code>#launcher</code>, <code>#ini</code>, <code>#esp32</code>, <code>#crash</code>, <code>#webserial</code>). Deep-link from anywhere.'],
      ],
      howto: [
        '<strong>1.</strong> Skim the section headers — pick what you want to dig into.',
        '<strong>2.</strong> Read the option/directive cards top-down — each has a what/under-the-hood/failure-modes block.',
        '<strong>3.</strong> Follow cross-links to <a href="audit.html">audit.html</a> for incident forensics.',
        '<strong>4.</strong> Copy code snippets with the COPY button on each <code>&lt;pre&gt;</code>.',
      ],
      wiki: [
        ['🚀 launch.sh',     'All 20 menu options · grouped by section · with underlying CLI command + failure modes.'],
        ['⚙️ platformio.ini', 'Every directive in our config + commonly-needed extras (partitions, monitor_filters, upload_protocol).'],
        ['💡 ESP32-C3',     'RMT channels · partition layout · USB CDC vs UART · GPIO strapping pins.'],
        ['🐛 addr2line',    'MCAUSE table · address-range cheat sheet · flag reference · common signatures.'],
        ['📡 Web Serial',   'Browser support · security model · minimal code example.'],
      ],
    },

    /* ── Instructor — workshop teacher running a class ── */
    'instructor': {
      faq: [
        ['How long is a session?',     '90 minutes for ages 9–14, group of 4–8. Adapt timing for younger / older.'],
        ['What if a robot breaks mid-class?', 'Swap to a spare. Don\'t troubleshoot in front of impatient kids — debug after.'],
        ['Battery management?',        'Charge every robot the night before. Keep 2 spares fully charged. Battery dip is the #1 mid-session failure.'],
        ['Do I need Wi-Fi?',           'Optional. Robots use Bluetooth. Web tools work offline if you serve locally with <code>./launch.sh serve</code>.'],
      ],
      howto: [
        '<strong>1.</strong> Charge robots the night before.',
        '<strong>2.</strong> 30 min before: pre-pair your demo robot, project user-guide.html.',
        '<strong>3.</strong> Run the 90-min lesson plan (section 2).',
        '<strong>4.</strong> Use the cheat sheet (section 4) when things go sideways.',
        '<strong>5.</strong> Run reset checklist (section 5) between groups.',
      ],
      wiki: [
        ['📋 Lesson plan',  '8-segment, 90 min flow — welcome → demo → power → pair → free play → lights/sounds → mini-challenge → wrap.'],
        ['🆘 Cheat sheet',  'Robot · phone · laptop · behavioral fixes — each in 30 seconds.'],
        ['🔁 Reset',        'Power, hardware, phone, logging, space — checklist persists per browser tab.'],
      ],
    },
  };

  /* ─────────────────────────────────────────────────────────
     HTML BUILDERS
     ───────────────────────────────────────────────────────── */

  const help = HELP[PAGE.help] || HELP.default;

  const escapeAttr = s => String(s).replace(/"/g, '&quot;');

  const faqHTML = help.faq.map(([q, a]) => `
    <details class="help-item">
      <summary>${q}</summary>
      <p>${a}</p>
    </details>
  `).join('');

  const howtoHTML = help.howto.map((p, i) => `
    <div class="help-step">
      <div class="help-step-num">${i + 1}</div>
      <p>${p}</p>
    </div>
  `).join('');

  const wikiHTML = help.wiki.map(([t, d]) => `
    <div class="wiki-entry">
      <h3>${t}</h3>
      <p>${d}</p>
    </div>
  `).join('');

  /* ── Splash ── */
  const SPLASH = `
    <div id="splash" onclick="dismissSplash()" aria-label="Loading splash, click to skip">
      <div class="splash-inner">
        <div class="splash-logo" id="splashLogo"></div>
        <div class="splash-title">${PAGE.title}</div>
        <div class="splash-sub">${PAGE.subtitle}</div>
        <div class="splash-hint" data-i18n="splashHint">tap to skip</div>
      </div>
    </div>`;

  /* ── Overlays ── */
  const OVERLAYS = `
    <div class="sidebar-overlay" id="helpOverlay"></div>
    <div class="sidebar-overlay" id="settingsOverlay"></div>`;

  /* ── Help panel (left sidebar) ── */
  const HELP_PANEL = `
    <div class="sidebar sidebar-left" id="helpPanel" role="dialog" aria-label="Help" aria-modal="true">
      <div class="sidebar-header">
        <span class="sidebar-title" data-i18n="help">❓ Help</span>
        <button class="btn-icon-only" id="helpCloseBtn" aria-label="Close help">✕</button>
      </div>
      <div class="help-tabs">
        <button class="help-tab active" data-tab="faq"   data-i18n="faq">FAQ</button>
        <button class="help-tab"        data-tab="howto" data-i18n="howto">How-To</button>
        <button class="help-tab"        data-tab="wiki"  data-i18n="wiki">Wiki</button>
      </div>
      <div class="sidebar-body" style="padding:12px 14px">
        <div class="help-content active" id="helpFaq">${faqHTML}</div>
        <div class="help-content"        id="helpHowto">${howtoHTML}</div>
        <div class="help-content"        id="helpWiki">${wikiHTML}</div>
      </div>
      <div class="sidebar-footer">
        <span>Workshop DIY v1.2</span>
        <span id="hijriDate"></span>
      </div>
    </div>`;

  /* ── Settings panel (right sidebar) ── */
  const SETTINGS_PANEL = `
    <div class="sidebar" id="settingsPanel" role="dialog" aria-label="Settings" aria-modal="true">
      <div class="sidebar-header">
        <span class="sidebar-title" data-i18n="settings">⚙️ Settings</span>
        <button class="btn-icon-only" id="settingsCloseBtn" aria-label="Close settings">✕</button>
      </div>
      <div class="sidebar-body">
        <div class="sidebar-group">
          <label class="sidebar-label" data-i18n="theme">Theme</label>
          <select class="sidebar-select" id="themeSelect">
            <optgroup label="🔬 Science">
              <option value="lab-light" data-i18n-opt="t_lab-light">🔬 Lab (light)</option>
              <option value="lab-dark"  data-i18n-opt="t_lab-dark">🔬 Lab (dark)</option>
            </optgroup>
            <optgroup label="💻 Hacker">
              <option value="terminal"  data-i18n-opt="t_terminal">💻 Terminal</option>
              <option value="solarized" data-i18n-opt="t_solarized">💻 Solarized</option>
            </optgroup>
            <optgroup label="🤖 Pro">
              <option value="robot"     data-i18n-opt="t_robot">🤖 Robot</option>
            </optgroup>
            <optgroup label="🦸 Kids action">
              <option value="bot-pop"   data-i18n-opt="t_bot-pop">🤖 Bot Pop</option>
              <option value="kapow"     data-i18n-opt="t_kapow">⚡ Kapow!</option>
              <option value="blastoff"  data-i18n-opt="t_blastoff">🚀 Blastoff</option>
              <option value="dino"      data-i18n-opt="t_dino">🦖 Dino</option>
            </optgroup>
          </select>
        </div>
        <div class="sidebar-group">
          <label class="sidebar-label" data-i18n="language">Language</label>
          <select class="sidebar-select" id="langSelect">
            <option value="en">English</option>
            <option value="fr">Français</option>
            <option value="ar">العربية</option>
          </select>
        </div>
        <div class="sidebar-group">
          <label class="sidebar-label" data-i18n="soundEffects">🔊 Sound Effects</label>
          <label class="toggle-ctrl">
            <input type="checkbox" class="checkbox-ctrl" id="soundToggle">
            <span>Enable sounds</span>
          </label>
        </div>
        <div class="sidebar-group">
          <label class="sidebar-label">🫁 Focus tools</label>
          <button class="btn-sm" id="breathingBtn" data-i18n="breathingGuide">Breathing guide</button>
          <div id="dhikrDisplay" style="display:none;align-items:center;gap:8px;margin-top:8px">
            <span style="font-size:.76rem;color:var(--text-muted)" data-i18n="dhikrTap">Tap</span>
            <button class="btn-sm primary" id="dhikrBtn">📿</button>
            <span id="dhikrCounter" style="color:var(--accent);font-weight:700">0</span>
          </div>
        </div>
        <div class="sidebar-group">
          <label class="sidebar-label">🎤 Voice</label>
          <button class="btn-sm" id="whisperBtn" data-i18n="whisperMode">Whisper mode</button>
        </div>
        <div class="sidebar-group">
          <label class="sidebar-label">🎵 Audio</label>
          <button class="btn-sm" id="musicBtn" data-i18n="musicMode">Music reactive</button>
        </div>
        <div class="sidebar-group">
          <label class="sidebar-label">📋 Project</label>
          <a href="index.html"        class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">🏠 Home</a>
          <a href="start-here.html"   class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">📖 Dev / Start here</a>
          <a href="learn.html"        class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">🔬 Tech deep-dive</a>
          <a href="hardware.html"     class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">🔌 Hardware</a>
          <a href="user-guide.html"   class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">🎮 User guide</a>
          <a href="build-guide.html"  class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">🛠️ Build guide</a>
          <a href="instructor.html"   class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">🎓 Instructor</a>
          <a href="flash.html"        class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">⚡ Flasher</a>
          <a href="monitor.html"      class="btn-sm" style="display:inline-flex;align-items:center;gap:4px;margin-bottom:6px">📡 Monitor</a>
          <a href="audit.html"        class="btn-sm" style="display:inline-flex;align-items:center;gap:4px">📋 Audit</a>
        </div>
      </div>
      <div class="sidebar-footer">
        <span>Workshop DIY v1.2</span>
        <span>Robot-01 · ESP32-C3</span>
      </div>
    </div>`;

  /* ── Log panel (right, docked) ── */
  const LOG_PANEL = `
    <div class="sidebar" id="logPanel" role="complementary" aria-label="Activity Log">
      <div id="logResizeHandle" class="resize-handle"></div>
      <div class="sidebar-header">
        <span class="sidebar-title" data-i18n="activityLog">Activity Log</span>
        <div style="display:flex;gap:4px;flex-wrap:wrap">
          <button class="btn-sm" id="clearLogBtn"  data-i18n="clear">Clear</button>
          <button class="btn-sm" id="copyLogBtn"   data-i18n="copy">Copy</button>
          <button class="btn-sm" id="exportLogBtn" data-i18n="export">Export</button>
          <button class="btn-icon-only" id="logCloseBtn" aria-label="Close log">✕</button>
        </div>
      </div>
      <div class="log-filters">
        <button class="log-filter active" data-filter="all"     data-i18n="filterAll">All</button>
        <button class="log-filter"        data-filter="info">Info</button>
        <button class="log-filter"        data-filter="success">OK</button>
        <button class="log-filter"        data-filter="error">ERR</button>
        <button class="log-filter"        data-filter="tx">TX</button>
        <button class="log-filter"        data-filter="rx">RX</button>
      </div>
      <div class="log-body" style="flex:1;overflow:hidden;padding:6px 8px">
        <div class="log" id="logContainer" style="height:100%;max-height:none;border:none;border-radius:6px"></div>
      </div>
      <div class="sidebar-footer">
        <span data-i18n="eventsMsg">Events &amp; messages</span>
      </div>
    </div>`;

  /* ── Toast ── */
  const TOAST = `
    <div class="toast-indicator" id="toastIndicator">
      <div class="toast-inner">
        <div class="spinner"></div>
        <span class="toast-text" id="toastMessage"></span>
      </div>
    </div>`;

  /* ── Header (goes inside .app at top) ── */
  const HEADER = `
    <div class="deco-band"></div>
    <header class="header">
      <div class="bismillah">
        <span class="bism-ornament">✦</span>
        بِسْمِ اللهِ الرَّحْمٰنِ الرَّحِيْمِ
        <span class="bism-ornament">✦</span>
      </div>
      <div class="title-block">
        <div class="logo-wrap" id="logoWrap"
             style="width:54px;height:30px;color:var(--accent);flex-shrink:0"></div>
        <div>
          <h1>${PAGE.title}</h1>
          <p class="subtitle">${PAGE.subtitle}</p>
        </div>
      </div>
      <div class="header-right">
        <div class="header-buttons">
          <button class="btn-icon-only" id="helpBtn"     aria-label="Help"          title="Help (❓)">❓</button>
          <button class="btn-icon-only" id="logBtn"      aria-label="Activity log"  title="Activity Log (📜)">📜</button>
          <button class="btn-icon-only" id="settingsBtn" aria-label="Settings"      title="Settings (⚙️)">⚙️</button>
        </div>
        <div class="status-pill ${PAGE.statusOn ? 'connected' : ''}" id="statusPill">
          <div class="status-dot"></div>
          <span id="statusText">${escapeAttr(PAGE.statusTx)}</span>
        </div>
      </div>
    </header>`;

  /* ── Footer (goes inside .app at bottom) ── */
  const FOOTER = `
    <div class="deco-band bottom-band"></div>
    <footer class="app-footer">
      <span class="footer-text">
        <a href="https://workshop-diy.org" target="_blank" rel="noopener">workshop-diy.org</a>
        · <a href="https://github.com/abourdim/esp32c3_super_mini_robot" target="_blank" rel="noopener" title="Source on GitHub">⭐ github</a>
        · ESP32-C3 Robot · <a href="audit.html">audit</a>
      </span>
    </footer>`;

  /* ─────────────────────────────────────────────────────────
     INJECT
     ───────────────────────────────────────────────────────── */
  body.insertAdjacentHTML(
    'afterbegin',
    SPLASH + OVERLAYS + HELP_PANEL + SETTINGS_PANEL + LOG_PANEL + TOAST
  );

  const app = body.querySelector('.app');
  if (app) {
    app.insertAdjacentHTML('afterbegin', HEADER);
    app.insertAdjacentHTML('beforeend',  FOOTER);
  } else {
    console.warn('[wdiy-shell] No <div class="app"> found — header/footer not injected.');
  }

  /* ─────────────────────────────────────────────────────────
     i18n OVERRIDES — page title + subtitle
     wdiy-script.js's setLanguage() reads LANG[lang].title and
     puts it in document.title. Override per page so each tab
     reads correctly.
     ───────────────────────────────────────────────────────── */
  if (typeof LANG !== 'undefined') {
    ['en', 'fr', 'ar'].forEach(lang => {
      if (LANG[lang]) {
        LANG[lang].title    = PAGE.title;
        LANG[lang].subtitle = PAGE.subtitle;
      }
    });
  }

  /* ─────────────────────────────────────────────────────────
     COPY BUTTONS for <pre> blocks (page-wide, idempotent)
     ───────────────────────────────────────────────────────── */
  document.querySelectorAll('pre').forEach(pre => {
    if (pre.querySelector('.copy-btn')) return;
    const btn = document.createElement('button');
    btn.className = 'copy-btn';
    btn.type = 'button';
    btn.textContent = 'COPY';
    btn.addEventListener('click', e => {
      e.preventDefault();
      const text = pre.querySelector('code')?.textContent ?? pre.textContent;
      navigator.clipboard.writeText(text).then(() => {
        btn.textContent = 'COPIED';
        btn.classList.add('ok');
        setTimeout(() => { btn.textContent = 'COPY'; btn.classList.remove('ok'); }, 1400);
      }).catch(() => {});
    });
    pre.appendChild(btn);
  });

  /* ─────────────────────────────────────────────────────────
     BOOT
     ───────────────────────────────────────────────────────── */
  if (typeof init === 'function') {
    init();
  } else {
    console.warn('[wdiy-shell] init() not found — load wdiy-script.js BEFORE wdiy-shell.js');
  }
})();
