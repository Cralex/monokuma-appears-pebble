module.exports = [
  {
    "type": "heading",
    "defaultValue": "Magical Miracle Girl \u2605 Usami"
  },
  {
    // The gem and the Hope Fragments count; index.js fills it in.
    "type": "text",
    "id": "hope",
    "defaultValue": ""
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Appearance"
      },
      {
        "type": "radiogroup",
        "messageKey": "CLOCK_FONT",
        "label": "Clock font",
        "defaultValue": "0",
        "options": [
          { "label": "DorBlue", "value": "0" },
          { "label": "LECO (Pebble system font)", "value": "1" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Behavior"
      },
      {
        "type": "toggle",
        "messageKey": "SHAKE_ENABLED",
        "label": "Change image on shake",
        "description": "Shake or tap the watch to show Usami in a different pose.",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
