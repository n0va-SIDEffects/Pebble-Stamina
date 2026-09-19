// Baut die Clay-Konfiguration für eine Sprache.
var BMC_URL = 'https://buymeacoffee.com/SIDEffects';

var TOUCH_PLATFORMS = ['emery', 'gabbro'];

// Reihenfolge wie LANGS in tools/gen_strings.py (Wert = Index + 1, 0 = automatisch)
var LANGUAGES = [
  'English', 'Deutsch', 'Français', 'Español', 'Italiano', 'Português', 'Nederlands',
  'Polski', 'Čeština', 'Svenska', 'Dansk', 'Norsk', 'Suomi', 'Русский', 'Українська',
  'Türkçe', '日本語', '中文', '한국어'
];
var LANG_CODES = ['en', 'de', 'fr', 'es', 'it', 'pt', 'nl', 'pl', 'cs', 'sv', 'da', 'nb', 'fi',
  'ru', 'uk', 'tr', 'ja', 'zh', 'ko'];

module.exports = function (t, platform) {
  var tracking = [
    { type: 'heading', defaultValue: t.tracking },
    {
      type: 'select',
      messageKey: 'SOLO_STYLE',
      label: t.style,
      description: t.style_desc,
      defaultValue: '0',
      options: [
        { label: t.style_auto, value: '0' },
        { label: t.style_stroke, value: '1' },
        { label: t.style_rub, value: '2' },
        { label: t.style_toy, value: '3' }
      ]
    },
    {
      type: 'select',
      messageKey: 'SENSITIVITY',
      label: t.sens,
      description: t.sens_desc,
      defaultValue: '1',
      options: [
        { label: t.sens_low, value: '0' },
        { label: t.sens_normal, value: '1' },
        { label: t.sens_high, value: '2' }
      ]
    }
  ];

  // Unbekannte Plattform: Option lieber zeigen als verstecken
  if (!platform || TOUCH_PLATFORMS.indexOf(platform) !== -1) {
    tracking.push({
      type: 'toggle',
      messageKey: 'TOUCH_SESSION',
      label: t.touch,
      description: t.touch_desc,
      defaultValue: true
    });
  }

  tracking.push({
    type: 'toggle',
    messageKey: 'CHECKIN',
    label: t.checkin,
    description: t.checkin_desc,
    defaultValue: true
  });

  var languageOptions = [{ label: t.lang_auto, value: '0' }].concat(
    LANGUAGES.map(function (name, i) { return { label: name, value: String(i + 1) }; })
  );

  return [
    { type: 'heading', defaultValue: t.title },
    { type: 'text', defaultValue: t.privacy },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t.profile },
        { type: 'text', defaultValue: t.profile_desc },
        {
          type: 'select',
          messageKey: 'SEX',
          label: t.sex,
          defaultValue: '0',
          options: [
            { label: t.male, value: '0' },
            { label: t.female, value: '1' },
            { label: t.other, value: '2' }
          ]
        },
        { type: 'slider', messageKey: 'AGE', label: t.age, defaultValue: 30, min: 16, max: 99, step: 1 },
        { type: 'slider', messageKey: 'WEIGHT', label: t.weight, defaultValue: 80, min: 35, max: 250, step: 1 }
      ]
    },
    { type: 'section', items: tracking },
    {
      type: 'section',
      items: [
        {
          type: 'select',
          messageKey: 'LANGUAGE',
          label: t.language,
          description: t.lang_desc,
          defaultValue: '0',
          options: languageOptions
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: '☕ ' + t.support },
        { type: 'text', defaultValue: t.support_text },
        {
          type: 'text',
          defaultValue:
            '<a href="' + BMC_URL + '" target="_blank" ' +
            'style="display:block;text-align:center;margin:8px 0;padding:12px;border-radius:8px;' +
            'background:#FFDD00;color:#000;font-weight:bold;text-decoration:none;">☕ ' +
            t.coffee + '</a>'
        }
      ]
    },
    { type: 'submit', defaultValue: t.save }
  ];
};

module.exports.LANG_CODES = LANG_CODES;
