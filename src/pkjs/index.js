// Clay 1.0.4 direkt eingebunden: das npm-Paket kennt die neuen Plattformen (flint) nicht
var Clay = require('./vendor/clay');
var i18n = require('./i18n');
var buildConfig = require('./config');

var SETTINGS_KEY = 'clay-settings';

function storedSettings() {
  try {
    return JSON.parse(localStorage.getItem(SETTINGS_KEY)) || {};
  } catch (e) {
    return {};
  }
}

// Fest eingestellte Sprache der Uhr, sonst die des Handys, sonst der Uhr, sonst Englisch
function detectLanguage() {
  var chosen = parseInt(storedSettings().LANGUAGE, 10);
  if (chosen > 0 && buildConfig.LANG_CODES[chosen - 1]) return buildConfig.LANG_CODES[chosen - 1];

  var candidates = [];
  if (typeof navigator !== 'undefined' && navigator.language) candidates.push(navigator.language);
  try {
    var info = Pebble.getActiveWatchInfo && Pebble.getActiveWatchInfo();
    if (info && info.language) candidates.push(info.language);
  } catch (e) { /* ältere App-Versionen */ }

  for (var i = 0; i < candidates.length; i++) {
    var base = String(candidates[i]).toLowerCase().split(/[-_]/)[0];
    if (base === 'no' || base === 'nn') base = 'nb';
    if (i18n[base]) return base;
  }
  return 'en';
}

function watchPlatform() {
  try {
    var info = Pebble.getActiveWatchInfo && Pebble.getActiveWatchInfo();
    return info && info.platform;
  } catch (e) {
    return null;
  }
}

var clay = new Clay(buildConfig(i18n.en), null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function () {
  clay.config = buildConfig(i18n[detectLanguage()], watchPlatform());
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var dict = clay.getSettings(e.response);
  Pebble.sendAppMessage(dict, null, function (err) {
    console.log('Einstellungen konnten nicht gesendet werden: ' + JSON.stringify(err));
  });
});

// Die Uhr ist die Quelle der Wahrheit: beim Start den aktuellen Stand holen,
// damit die Einstellungsseite Änderungen zeigt, die direkt auf der Uhr gemacht wurden
Pebble.addEventListener('ready', function () {
  Pebble.sendAppMessage({ REQUEST_PROFILE: 1 });
});

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload || {};
  if (p.SEX === undefined) return;

  var settings = storedSettings();

  settings.SEX = String(p.SEX);
  settings.AGE = p.AGE;
  settings.WEIGHT = p.WEIGHT;
  settings.SENSITIVITY = String(p.SENSITIVITY);
  settings.TOUCH_SESSION = !!p.TOUCH_SESSION;
  settings.SOLO_STYLE = String(p.SOLO_STYLE);
  settings.LANGUAGE = String(p.LANGUAGE);
  settings.CHECKIN = !!p.CHECKIN;
  settings.AUTO_START = String(p.AUTO_START);
  settings.LIGHT = String(p.LIGHT);
  for (var n = 1; n <= 8; n++) settings['POS_NAME_' + n] = p['POS_NAME_' + n] || '';
  localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
  console.log('Profil von der Uhr übernommen: ' + JSON.stringify(settings));
});
